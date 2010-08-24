/*
   Copyright 2010 Andreas Fredriksson

   This file is part of Tundra.

   Tundra is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Tundra is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Tundra.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "engine.h"
#include "util.h"
#include "scanner.h"
#include "portable.h"
#include "relcache.h"

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
		td_file *file;
		td_build_path(&path[0], sizeof(path), base_file, inc->string, inc->string_len, TD_BUILD_REPLACE_NAME);
		file = td_engine_get_file(engine, path, TD_COPY_STRING);
		if (TD_STAT_EXISTS & td_stat_file(engine, file)->flags)
			return file;
	}
	
	for (i = 0, count = config->path_count; i < count; ++i)
	{
		const td_file *dir = config->paths[i];
		td_file *file;
		td_build_path(&path[0], sizeof(path), dir, inc->string, inc->string_len, TD_BUILD_CONCAT);
		file = td_engine_get_file(engine, path, TD_COPY_STRING);
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
	int at_start_of_file = 1;
	char line_buffer[1024];
	char *buffer_start = line_buffer;
	int buffer_size = sizeof(line_buffer);
	static const unsigned char utf8_mark[] = { 0xef, 0xbb, 0xbf };

	if (NULL == (f = fopen(file->path, "r")))
		return 0;

	for (;;)
	{
		char *p, *line;
		int count, remain;
		count = (int) fread(buffer_start, 1, (int) buffer_size, f);
		if (0 == count)
			break;

		/* skip past any UTF-8 bytemark, or isspace() and related functions trigger asserts in MSVC debug builds! */
		if (at_start_of_file && count >= sizeof(utf8_mark) && 0 == memcmp(buffer_start, utf8_mark, sizeof(utf8_mark)))
		{
			memmove(buffer_start, buffer_start + sizeof(utf8_mark), count - sizeof(utf8_mark));
			count -= sizeof(utf8_mark);
		}

		at_start_of_file = 0;

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
		   
		remain = (int) (buffer_start - line);
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
	td_file *file,
	td_cpp_scanner *config,
	unsigned int salt,
	include_set *set)
{
	int i, count;
	td_file **files;

	int found_count = 0;
	td_file* found_files[TD_MAX_INCLUDES_IN_FILE];
	cpp_include includes[TD_MAX_INCLUDES_IN_FILE];

	/* see if there is a cached include set for this file */
	files = td_engine_get_relations(engine, file, salt, &count);

	if (files)
	{
		if (td_debug_check(engine, TD_DEBUG_SCAN))
			printf("%s: hit relation cache; %d entries\n", file->path, count);
		for (i = 0; i < count; ++i)
			push_include(set, files[i]);
		return;
	}

	if (td_debug_check(engine, TD_DEBUG_SCAN))
		printf("%s: scanning\n", file->path);

	count = scan_includes(scratch, file, &includes[0], sizeof(includes)/sizeof(includes[0]));

	for (i = 0; i < count; ++i)
	{
		if (NULL != (found_files[found_count] = find_file(file, engine, &includes[i], config)))
			++found_count;
	}

	for (i = 0; i < found_count; ++i)
		push_include(set, found_files[i]);

	if (td_debug_check(engine, TD_DEBUG_SCAN))
		printf("%s: inserting %d entries in relation cache\n", file->path, found_count);

	td_engine_set_relations(engine, file, salt, found_count, found_files);
}

static int
scan_cpp(td_engine *engine, td_node *node, td_scanner *state)
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
		scan_file(engine, &scratch, input, config, salt, set);
	}

	node->job.idep_count = set->count - node->input_count;

	td_mutex_lock_or_die(engine->lock);
	node->job.ideps = (td_file **) td_page_alloc(&engine->alloc, sizeof(td_file*) * node->job.idep_count);
	td_mutex_unlock_or_die(engine->lock);

	memcpy(&node->job.ideps[0], &set->files[node->input_count], sizeof(td_file*) * node->job.idep_count);

	td_alloc_cleanup(&scratch);

	return 0;
}

static int make_cpp_scanner(lua_State *L)
{
	td_engine *engine = td_check_engine(L, 1);
	td_cpp_scanner *self = (td_cpp_scanner *) td_alloc_scanner(engine, L, sizeof(td_cpp_scanner));

	self->head.ident = "cpp";
	self->head.scan_fn = &scan_cpp;
	self->paths = td_build_file_array(L, engine, 2, &self->path_count);

	td_call_cache_hook(L, &td_scanner_hook_key, 2, lua_gettop(L));

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
