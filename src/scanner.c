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
