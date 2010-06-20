#include "scanner.h"

#include <lua.h>
#include <lauxlib.h>
#include <string.h>

int td_scanner_open(lua_State *L)
{
	luaL_newmetatable(L, TUNDRA_SCANNER_MTNAME);
	lua_pop(L, 1);
	return 0;
}

td_scanner *td_alloc_scanner(struct lua_State* L, int size)
{
	td_scanner *result = (td_scanner *) lua_newuserdata(L, size);
	memset(result, 0, size);
	luaL_getmetatable(L, TUNDRA_SCANNER_MTNAME);
	lua_setmetatable(L, -2);
	return result;
}
