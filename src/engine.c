
#include <lua.h>
#include <lauxlib.h>
#include <stdlib.h>
#include <string.h>

struct td_node;
struct td_pass;
struct td_job;

typedef int (*td_scanner_func)(const struct td_node *n, struct td_job *job);

enum
{
	TD_STRING_PAGE_SIZE = 65536,
	TD_STRING_PAGE_MAX = 100,
	TD_PASS_MAX = 32
};

struct td_scanner
{
	int (*scan)(struct td_node *node);
};

struct td_node
{
	const char *label;
	const char *action;
	int pass_index;

	const struct td_scanner *scanner;

	int dep_count;
	struct td_node *deps;
};

struct td_pass
{
	const char *name;
	int build_order;
};

struct td_engine
{
	lua_State* L;
	int page_index;
	int page_left;
	char* pages[TD_STRING_PAGE_MAX];

	int pass_count;
	struct td_pass passes[TD_PASS_MAX];
};

static char *
td_engine_strdup(struct td_engine *engine, const char* str, size_t len)
{
	int left, page;
	char *addr;
	lua_State* L = engine->L;

	left = engine->page_left;
	page = engine->page_index;

	if (left < len)
	{
		if (page == TD_STRING_PAGE_MAX)
			luaL_error(L, "out of string page memory");

		page = engine->page_index = page + 1;
		left = engine->page_left = TD_STRING_PAGE_SIZE;
		engine->pages[page] = malloc(TD_STRING_PAGE_SIZE);
		if (!engine->pages[page])
			luaL_error(L, "out of memory allocating string page");
	}

	addr = engine->pages[page] + TD_STRING_PAGE_SIZE - left;
	memcpy(addr, str, len + 1);
	engine->page_index -= (int) len + 1;
	return addr;
}

static char *
td_engine_strdup_lua(struct td_engine *engine, int index)
{
	const char *str;
	size_t len;
	str = lua_tolstring(engine->L, index, &len);
	return td_engine_strdup(engine, str, len);
}

static int
get_pass_index(struct td_engine* engine, int index)
{
	lua_State* L = engine->L;
	int build_order, i, e;
	size_t name_len;
	const char* name;

	lua_getfield(L, index, "BuildOrder");
	lua_getfield(L, index, "Name");

	name = lua_tolstring(L, -1, &name_len);

	if (lua_isnil(L, -2))
		luaL_error(L, "no build order set for pass %s", name);

	build_order = (int) lua_tointeger(L, -2);

	for (i = 0, e = engine->pass_count; i < e; ++i)
	{
		if (engine->passes[i].build_order == build_order)
		{
			if (0 == strcmp(name, engine->passes[i].name))
			{
				lua_pop(L, 2);
				return i;
			}
		}
	}

	if (TD_PASS_MAX == engine->pass_count)
		luaL_error(L, "too many passes adding pass %s", name);

	i = engine->pass_count++;

	engine->passes[i].name = td_engine_strdup(engine, name, name_len);
	engine->passes[i].build_order = build_order;

	lua_pop(L, 2);
	return i;
}


static void
make_node(struct td_engine *engine, int index, struct td_node *output)
{
	lua_State* L = engine->L;

	lua_getfield(L, index, "Label");
	output->label = td_engine_strdup_lua(engine, -1);
	lua_pop(L, 1);

	lua_getfield(L, index, "Action");
	output->label = td_engine_strdup_lua(engine, -1);
	lua_pop(L, 1);

	lua_getfield(L, index, "Pass");
	if (lua_isnil(L, -1))
		luaL_error(L, "no pass associated with %s", output->label);

	output->pass_index = get_pass_index(engine, -1);
}

/*
 * Execute actions needed to update a dependency graph.
 *
 * Input:
 * A list of dag nodes to build.
 */
int tundra_build(lua_State* L)
{
	int i, narg;

	narg = lua_gettop(L);

	for (i=0; i<narg; ++i)
	{

	}

	return 0;
}
