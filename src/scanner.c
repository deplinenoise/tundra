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

#include "scanner.h"
#include "engine.h"

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
