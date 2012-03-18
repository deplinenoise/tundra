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
#include "files.h"
#include "portable.h"
#include "relcache.h"
#include "scanner.h"
#include "util.h"

#include <lua.h>
#include <lauxlib.h>
#include <string.h>

int td_scanner_open(lua_State *L)
{
	luaL_newmetatable(L, TUNDRA_SCANNER_REF_MTNAME);
	lua_pop(L, 1);
	return 0;
}

td_scanner *td_alloc_scanner(td_engine *engine, struct lua_State* L, int size)
{
	td_scanner *scanner;
	td_scanner_ref *result;
	
	scanner = (td_scanner *) td_page_alloc(&engine->alloc, size);
	memset(scanner, 0, size);

	result = (td_scanner_ref *) lua_newuserdata(L, sizeof(td_scanner));
	result->scanner = scanner; 
	luaL_getmetatable(L, TUNDRA_SCANNER_REF_MTNAME);
	lua_setmetatable(L, -2);
	return scanner;
}

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

static unsigned int relation_salt_cpp(const td_scanner *scanner)
{
	int i, count;
	unsigned int hash = 0;
	for (i = 0, count = scanner->path_count; i < count; ++i)
	{
		hash ^= (unsigned int) djb2_hash(scanner->include_paths[i]->path);
	}
	return hash;
}

static td_file *
find_file(td_file *base_file, td_engine *engine, const td_include_data *inc, const td_scanner *scanner)
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
	
	for (i = 0, count = scanner->path_count; i < count; ++i)
	{
		const td_file *dir = scanner->include_paths[i];
		td_file *file;
		td_build_path(&path[0], sizeof(path), dir, inc->string, inc->string_len, TD_BUILD_CONCAT);
		file = td_engine_get_file(engine, path, TD_COPY_STRING);
		if (TD_STAT_EXISTS & td_stat_file(engine, file)->flags)
			return file;
	}

	return NULL;
}

static int
scan_file_data(td_alloc *scratch, td_file *file, td_include_data *out, int max_count, td_scanner *scanner)
{
	FILE *f;
	int file_count = 0;
	int at_start_of_file = 1;
	int at_end_of_file = 0;
	char line_buffer[1024];
	char *buffer_start = line_buffer;
	int buffer_size = sizeof(line_buffer);
	static const unsigned char utf8_mark[] = { 0xef, 0xbb, 0xbf };
	td_scan_fn *line_scanner = scanner->scan_fn;

	if (NULL == (f = fopen(file->path, "r")))
		return 0;

	for (; !at_end_of_file ;)
	{
		char *p, *line;
		int count, remain;
		count = (int) fread(buffer_start, 1, (int) buffer_size, f);
		if (0 == count)
		{
			/* add an implicit newline at the end of the file, which is C++11 conformant and good practical behaviour */
			buffer_start[0] = '\n';
			count = 1;
			at_end_of_file = 1;
		}

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
				file_count += (*line_scanner)(scratch, line, &out[file_count], scanner);
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
	td_scanner *scanner,
	unsigned int salt,
	include_set *set)
{
	int i, count;
	td_file **files;

	int found_count = 0;
	td_file* found_files[TD_MAX_INCLUDES_IN_FILE];
	td_include_data includes[TD_MAX_INCLUDES_IN_FILE];

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

	count = scan_file_data(scratch, file, &includes[0], sizeof(includes)/sizeof(includes[0]), scanner);

	for (i = 0; i < count; ++i)
	{
		if (NULL != (found_files[found_count] = find_file(file, engine, &includes[i], scanner)))
			++found_count;
	}

	for (i = 0; i < found_count; ++i)
		push_include(set, found_files[i]);

	if (td_debug_check(engine, TD_DEBUG_SCAN))
		printf("%s: inserting %d entries in relation cache\n", file->path, found_count);

	td_engine_set_relations(engine, file, salt, found_count, found_files);
}

int
td_scan_includes(td_engine *engine, td_node *node, td_scanner *state)
{
	td_alloc scratch;
	int i, count;
	td_scanner *config = (td_scanner *) state;
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

