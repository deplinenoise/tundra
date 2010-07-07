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

enum {
	TD_MAX_INCLUDES = 5000,
	TD_MAX_INCLUDES_IN_FILE = 2000
};

typedef struct {
	int count;
	td_file *files[TD_MAX_INCLUDES];
} include_set;

static int push_include(include_set *set, td_file *f)
{
	int i, count;
	for (i = 0, count = set->count; i < count; ++i)
	{
		if (f == set->files[i])
			return 0;
	}

	if (TD_MAX_INCLUDES == set->count)
		td_croak("too many includes");

	set->files[set->count++] = f;
	return 1;
}


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
	unsigned short string_len;
	unsigned char is_system_include;
} cpp_include;

static td_file *
find_file(td_file *base_file, td_engine *engine, const cpp_include *inc, const td_cpp_scanner *config)
{
	int i, count;
	char path[512];

	/* for non-system includes, try a path relative to the base file */
	if (!inc->is_system_include)
	{
		td_build_path(&path[0], sizeof(path), base_file, inc->string, inc->string_len, TD_BUILD_REPLACE_NAME);
		td_file *file = td_engine_get_file(engine ,path);
		if (TD_STAT_EXISTS & td_stat_file(engine, file)->flags)
			return file;
	}
	
	for (i = 0, count = config->path_count; i < count; ++i)
	{
		const td_file *dir = config->paths[i];
		td_build_path(&path[0], sizeof(path), dir, inc->string, inc->string_len, TD_BUILD_CONCAT);
		td_file *file = td_engine_get_file(engine ,path);
		if (TD_STAT_EXISTS & td_stat_file(engine, file)->flags)
			return file;
	}

	return NULL;
}

static int
scan_line(td_alloc *scratch, const char *start, cpp_include *dest)
{
	char separator;
	const char *str_start;

	while (isspace(*start))
		++start;

	if (*start++ != '#')
		return 0;

	while (isspace(*start))
		++start;
	
	if (0 != strncmp("include", start, 7))
		return 0;

	start += 7;

	if (!isspace(*start++))
		return 0;

	while (isspace(*start))
		++start;

	separator = *start++;
	if ('<' == separator)
	{
		dest->is_system_include = 1;
		separator = '>';
	}
	else
		dest->is_system_include = 0;
	
	str_start = start;
	for (;;)
	{
		char ch = *start++;
		if (ch == separator)
			break;
		if (!ch)
			return 0;
	}

	dest->string_len = (unsigned short) (start - str_start - 1);
	dest->string = td_page_strdup(scratch, str_start, dest->string_len);
	return 1;
}

static int
scan_includes(td_alloc *scratch, td_file *file, cpp_include *out, int max_count)
{
	FILE *f;
	int file_count = 0;
	char line_buffer[1024];
	char *buffer_start = line_buffer;
	int buffer_size = sizeof(line_buffer);

	if (NULL == (f = fopen(file->path, "r")))
		return 0;

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
				*p = 0;
				if (file_count == max_count)
					td_croak("%s: too many includes", file->path);
				file_count += scan_line(scratch, line, &out[file_count]);
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

	fclose(f);
	return file_count;
}

static void
scan_file(
	td_engine *engine,
	td_alloc *scratch,
	pthread_mutex_t *mutex,
	td_file *file,
	td_cpp_scanner *config,
	unsigned int salt,
	include_set *set)
{
	int i, count;
	td_file **files;

	/* see if there is a cached include set for this file */
	pthread_mutex_lock(mutex);
	files = td_engine_get_relations(engine, file, salt, &count);
	pthread_mutex_unlock(mutex);

	if (files)
	{
		if (td_debug_check(engine, 20))
			printf("%s: hit relation cache; %d entries\n", file->path, count);
		for (i = 0; i < count; ++i)
			push_include(set, files[i]);
		return;
	}

	{
		if (td_debug_check(engine, 20))
			printf("%s: scanning\n", file->path);

		int found_count = 0;
		td_file* found_files[TD_MAX_INCLUDES_IN_FILE];
		cpp_include includes[TD_MAX_INCLUDES_IN_FILE];
		count = scan_includes(scratch, file, &includes[0], sizeof(includes)/sizeof(includes[0]));

		for (i = 0; i < count; ++i)
		{
			if (NULL != (found_files[found_count] = find_file(file, engine, &includes[i], config)))
				++found_count;
		}

		for (i = 0; i < found_count; ++i)
			push_include(set, found_files[i]);

		if (td_debug_check(engine, 20))
			printf("%s: inserting %d entries in relation cache\n", file->path, found_count);

		pthread_mutex_lock(mutex);
		td_engine_set_relations(engine, file, salt, found_count, found_files);
		pthread_mutex_unlock(mutex);
	}
}

static int
scan_cpp(td_engine *engine, void *mutex, td_node *node, td_scanner *state)
{
	td_alloc scratch;
	int i, count;
	td_cpp_scanner *config = (td_cpp_scanner *) state;
	unsigned int salt = relation_salt_cpp(config);
	int set_cursor;
	include_set *set;

	td_alloc_init(&scratch, 10, 1024 * 1024);

	set = (include_set *) td_page_alloc(&scratch, sizeof(include_set));
	set->count = 0;

	for (i = 0, count = node->input_count; i < count; ++i)
		push_include(set, node->inputs[i]);

	set_cursor = 0;
	while (set_cursor < set->count)
	{
		td_file *input = set->files[set_cursor++];
		scan_file(engine, &scratch, (pthread_mutex_t *)mutex, input, config, salt, set);
	}

	pthread_mutex_lock((pthread_mutex_t *)mutex);
	node->job.idep_count = set->count - node->input_count;
	node->job.ideps = (td_file **) td_page_alloc(&engine->alloc, sizeof(td_file*) * node->job.idep_count);
	memcpy(&node->job.ideps[0], &set->files[node->input_count], sizeof(td_file*) * node->job.idep_count);
	pthread_mutex_unlock((pthread_mutex_t *)mutex);

	td_alloc_cleanup(&scratch);

	return 0;
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
