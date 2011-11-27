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
#include "files.h"

#include <ctype.h>
#include <lua.h>
#include <lauxlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

/* a simple c preprocessor #include scanner */

static int
scan_line(td_alloc *scratch, const char *start, td_include_data *dest)
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

static int make_cpp_scanner(lua_State *L)
{
	td_engine *engine = td_check_engine(L, 1);
	td_scanner *self = (td_scanner *) td_alloc_scanner(engine, L, sizeof(td_scanner));

	self->ident = "cpp";
	self->scan_fn = &scan_line;
	self->include_paths = td_build_file_array(L, engine, 2, &self->path_count);

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
