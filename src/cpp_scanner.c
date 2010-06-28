#include "engine.h"
#include "util.h"
#include "scanner.h"

#include <lua.h>
#include <lauxlib.h>

typedef struct td_cpp_scanner_tag
{
	td_scanner head;
	int path_count;
	const char **paths;
} td_cpp_scanner;

static int scan_cpp(td_engine *engine, td_node *node, td_scanner *state)
{
	td_cpp_scanner *self = (td_cpp_scanner *) state;
	(void) self;
	return 1;
}

static int make_cpp_scanner(lua_State *L)
{
	td_engine *engine = td_check_engine(L, 1);
	td_cpp_scanner *self = (td_cpp_scanner *) td_alloc_scanner(L, sizeof(td_cpp_scanner));

	self->head.ident = "cpp";
	self->head.scan_fn = &scan_cpp;
	self->paths = td_build_string_array(L, engine, 2, &self->path_count);

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
