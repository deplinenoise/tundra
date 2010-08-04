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

#include "util.h"
#include "engine.h"
#include "portable.h"

#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include <lua.h>
#include <lauxlib.h>

void
td_croak(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	fprintf(stderr, "\n");
	va_end(args);
	exit(1);
}

unsigned long
djb2_hash(const char *str_)
{
	unsigned const char *str = (unsigned const char*) str_;
	unsigned long hash = 5381;
	int c;

	while (0 != (c = *str++))
	{
		hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
	}

	return hash;
}

char *
td_page_strdup(td_alloc *alloc, const char *str, size_t len)
{
	char *addr = (char*) td_page_alloc(alloc, len + 1);

	memcpy(addr, str, len);

	/* rather than copying len+1, explicitly store a nul byte so strdup can
	 * work for substrings too. */
	addr[len] = '\0';

	return addr;
}

char *
td_page_strdup_lua(lua_State *L, td_alloc *alloc, int index, const char *context)
{
	const char *str;
	size_t len;
	str = lua_tolstring(L, index, &len);
	if (!str)
		luaL_error(L, "%s: expected a string", context);
	return td_page_strdup(alloc, str, len);
}

const char **
td_build_string_array(lua_State *L, td_alloc *alloc, int index, int *count_out)
{
	int i;
	const int count = (int) lua_objlen(L, index);
	const char **result;

	*count_out = count;
	if (!count)
		return NULL;
   
	result = (const char **) td_page_alloc(alloc, sizeof(const char*) * count);

	for (i = 0; i < count; ++i)
	{
		lua_rawgeti(L, index, i+1);
		result[i] = td_page_strdup_lua(L, alloc, -1, "string array");
		lua_pop(L, 1);
	}

	return result;
}

td_file **
td_build_file_array(lua_State *L, td_engine *engine, int index, int *count_out)
{
	int i;
	const int count = (int) lua_objlen(L, index);
	td_file **result;

	*count_out = count;
	if (!count)
		return NULL;
   
	result = (td_file **) td_page_alloc(&engine->alloc, sizeof(td_file*) * count);

	for (i = 0; i < count; ++i)
	{
		lua_rawgeti(L, index, i+1);
		result[i] = td_engine_get_file(engine, lua_tostring(L, -1));
		lua_pop(L, 1);
	}

	return result;
}

const char *
td_indent(int level)
{
	static const char spaces[] =
		"                                                     "
		"                                                     "
		"                                                     ";

	int adjust = sizeof(spaces) - 1 - level * 2;
	if (adjust < 0)
		adjust = 0;

	return spaces + adjust;
}

void
td_alloc_init(struct td_alloc *alloc, int page_count_max, int page_size)
{
	alloc->page_index = alloc->page_left = 0;
	alloc->page_size = page_size;
	alloc->total_page_count = page_count_max;
	alloc->pages = (char **) calloc(page_count_max, sizeof(char*));
}

void
td_alloc_cleanup(td_alloc *self)
{
	int p;
	for (p = self->page_index; p >= 0; --p)
	{
		char *page = self->pages[p];
		if (page)
		{
#ifndef NDEBUG
			memset(page, 0xdd, self->page_size);
#endif
			free(page);
		}
	}

	free(self->pages);
	self->pages = NULL;
}

void
td_build_path(char *buffer, int buffer_size, const td_file *base, const char *subpath, int subpath_len, td_build_path_mode mode)
{
	int offset = 0;
	int path_len = 0;
	switch (mode)
	{
		case TD_BUILD_CONCAT:
			path_len = base->path_len;
			break;

		case TD_BUILD_REPLACE_NAME:
			path_len = (int) (base->name - base->path);
			break;

		default:
			assert(0);
			break;
	}

	if (path_len + subpath_len + 2 > buffer_size)
		td_croak("combined path too long: %s -> include %s (limit: %d)", base->path, subpath, buffer_size);

	memcpy(buffer, base->path, path_len);
	offset = path_len;
	if (path_len > 0 && buffer[offset-1] != '/' && buffer[offset-1] != '\\')
		buffer[offset++] = TD_PATHSEP;
	memcpy(buffer + offset, subpath, subpath_len);
	offset += subpath_len;
	buffer[offset] = '\0';
}



