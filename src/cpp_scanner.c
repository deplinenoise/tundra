#include "engine.h"
#include "util.h"
#include "scanner.h"
#include "portable.h"

#include <ctype.h>
#include <lua.h>
#include <lauxlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

/* a simple c preprocessor #include scanner */

typedef struct td_cpp_scanner_tag
{
	td_scanner head;
	int path_count;
	td_file **paths;
} td_cpp_scanner;

static unsigned int relation_salt_cpp(const td_cpp_scanner *config)
{
	int i, count;
	unsigned int hash = 0;
	for (i = 0, count = config->path_count; i < count; ++i)
	{
		hash ^= (unsigned int) djb2_hash(config->paths[i]->path);
	}
	return hash;
}

typedef struct cpp_include_tag {
	const char *string;
	int string_len;
	int is_system_include;
	struct cpp_include_tag *next;
} cpp_include;

static td_file *
find_file(td_file *base_file, td_engine *engine, const cpp_include *inc, const td_cpp_scanner *config)
{
	int i, count;
	char path[512];
	td_stat stat;

	/* for non-system includes, try a path relative to the base file */
	if (!inc->is_system_include)
	{
		td_build_path(&path[0], sizeof(path), base_file, inc->string, inc->string_len, TD_BUILD_REPLACE_NAME);
		if (0 == td_stat_file(path, &stat))
			return td_engine_get_file(engine, path);
	}
	
	for (i = 0, count = config->path_count; i < count; ++i)
	{
		const td_file *dir = config->paths[i];
		td_build_path(&path[0], sizeof(path), dir, inc->string, inc->string_len, TD_BUILD_CONCAT);
		if (0 == td_stat_file(path, &stat))
			return td_engine_get_file(engine, path);
	}

	return NULL;
}

static cpp_include*
scan_line(td_alloc *alloc, const char *start, cpp_include *head)
{
	char separator;
	const char *str_start;
	cpp_include *result = head;

	while (isspace(*start))
		++start;

	if (*start++ != '#')
		return head;

	while (isspace(*start))
		++start;
	
	if (0 != strncmp("include", start, 7))
		return head;

	start += 7;

	if (!isspace(*start++))
		return head;

	while (isspace(*start))
		++start;

	result = (cpp_include *) td_page_alloc(alloc, sizeof(cpp_include));

	separator = *start++;
	if ('<' == separator)
	{
		result->is_system_include = 1;
		separator = '>';
	}
	else
		result->is_system_include = 0;
	
	str_start = start;
	for (;;)
	{
		char ch = *start++;
		if (ch == separator)
			break;
		if (!ch)
			return head;
	}

	result->string_len = start - str_start - 1;
	result->string = td_page_strdup(alloc, str_start, result->string_len);
	result->next = head;

	return result;
}

cpp_include*
scan_includes(td_alloc *alloc, td_file *file, int *count_out)
{
	FILE *f;
	int file_count = 0;
	cpp_include *head = NULL;
	char line_buffer[1024];
	char *buffer_start = line_buffer;
	int buffer_size = sizeof(line_buffer);

	if (NULL == (f = fopen(file->path, "r")))
		return NULL;

	for (;;)
	{
		char *p, *line;
		int count, remain;
		count = fread(buffer_start, 1, buffer_size, f);
		if (0 == count)
			break;

		buffer_start += count;

		line = line_buffer;
		for (p = line_buffer; p < buffer_start; ++p)
		{
			if ('\n' == *p)
			{
				cpp_include *prev = head;
				*p = 0;
				head = scan_line(alloc, line, head);
				if (prev != head)
					++file_count;
				line = p+1;
			}
		}

		if (line > buffer_start)
			line = buffer_start;
		   
		remain = buffer_start - line;
		memmove(line_buffer, line, remain);
		buffer_start = line_buffer + remain;
		buffer_size = sizeof(line_buffer) - remain;
	}

	*count_out = file_count;

	fclose(f);
	return head;
}

typedef struct cpp_fileset_tag
{
	int count;
	td_file** files;
	struct cpp_fileset_tag *next;
} cpp_fileset;

static cpp_fileset *
make_fileset(td_alloc *alloc, int count, td_file **files, int copy_files)
{
	cpp_fileset *result = (cpp_fileset*) td_page_alloc(alloc, sizeof(cpp_fileset));

	result->next = NULL;
	result->count = count;

	if (copy_files)
	{
		result->files = (td_file **) td_page_alloc(alloc, sizeof(td_file *) * count);
		memcpy(result->files, files, sizeof(td_file *) * count);
	}
	else
		result->files = files;

	return result;
}

static cpp_fileset *
scan_file(
	td_engine *engine,
	td_alloc *alloc,
	pthread_mutex_t *mutex,
	td_file *file,
	td_cpp_scanner *config,
	unsigned int salt)
{
	cpp_include *include_chain;
	td_file **files;
	int i, count, valid_count;

	/* see if there is a cached include set for this file */
	pthread_mutex_lock(mutex);
	files = td_engine_get_relations(engine, file, salt, &count);
	pthread_mutex_unlock(mutex);

	if (files)
		return make_fileset(alloc, count, files, 1);

	count = 0;
	include_chain = scan_includes(alloc, file, &count);

	files = (td_file **) td_page_alloc(alloc, sizeof(td_file *) * count);
	valid_count = 0;
	for (i = 0; i < count; ++i)
	{
		assert(include_chain);
		if (NULL != (files[valid_count] = find_file(file, engine, include_chain, config)))
			valid_count++;
		include_chain = include_chain->next;
	}

	return make_fileset(alloc, valid_count, files, 0);
}

static int
scan_cpp(td_engine *engine, void *mutex, td_node *node, td_scanner *state)
{
	td_alloc scratch;
	int i, count;
	td_cpp_scanner *config = (td_cpp_scanner *) state;
	unsigned int salt = relation_salt_cpp(config);

	td_alloc_init(&scratch, 10, 1024 * 1024);

	cpp_fileset* set_chain = NULL;

	for (i = 0, count = node->input_count; i < count; ++i)
	{
		cpp_fileset *chain = NULL;
		td_file *input = node->inputs[i];
		chain = scan_file(engine, &scratch, (pthread_mutex_t *)mutex, input, config, salt);
		chain->next = set_chain;
		set_chain = chain;
	}

	td_alloc_cleanup(&scratch);

	return 1;
}

static int make_cpp_scanner(lua_State *L)
{
	td_engine *engine = td_check_engine(L, 1);
	td_cpp_scanner *self = (td_cpp_scanner *) td_alloc_scanner(L, sizeof(td_cpp_scanner));

	self->head.ident = "cpp";
	self->head.scan_fn = &scan_cpp;
	self->paths = td_build_file_array(L, engine, 2, &self->path_count);

	return 1;
}

static const luaL_Reg cpp_scanner_entries[] = {
	{ "make_cpp_scanner", make_cpp_scanner },
	{ NULL, NULL },
};

int td_cpp_scanner_open(lua_State *L)
{
	luaL_newmetatable(L, TUNDRA_ENGINE_MTNAME);
	luaL_register(L, NULL, cpp_scanner_entries);
	lua_pop(L, 1);

	return 0;
}
