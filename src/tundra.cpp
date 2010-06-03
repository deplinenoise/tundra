#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include <cstdlib>
#include <cstring>
#include <cassert>
#include <algorithm>

#include "GraphBuilder.hpp"
#include "LuaScopeGuard.hpp"
#include "LuaFileSys.hpp"
#include "LuaUtil.hpp"
#include "Portable.hpp"
#include "md5.h"

#ifdef _MSC_VER
#define snprintf _snprintf
#endif

static bool opt_verbose = false;

static int tundra_init(lua_State* L)
{
	(void) L;
	return 0;
}

static int tundra_add_fs(lua_State*)
{
	return 0;
}

inline const char* spaces(int howmany)
{
	return &("                "[16-std::min(howmany, 16)]);
}


static int tundra_make0(lua_State* L)
{
	TUNDRA_LUA_SCOPE_GUARD_EXPECT(L, 1);

	if (opt_verbose)
	{
		lua_getfield(L, -1, "id");
		printf("make0 %s\n", lua_tostring(L, -1));
		lua_pop(L, 1);
	}

	lua_pop(L, 1);
	lua_pushinteger(L, 0);
	return 1;
}

static int tundra_process_dag1(lua_State* L)
{
	TUNDRA_LUA_SCOPE_GUARD_EXPECT(L, 1);
	int result_code = 0;

	// two input args:
	// 1) the memoization table
	// 2) the dag node to process

	const int idx_memo = 1;
	const int base_stack = 1;

	// Prepare to iterate the topmost table
	// We keep triplets on the stack (dag, deps, index)
	lua_getfield(L, -1, "deps");
	lua_pushinteger(L, 1);

	for (;;)
	{
		const int stack_top = lua_gettop(L);
		const int stack_depth = stack_top - base_stack;
		const int idx_dag = stack_top - 2;
		const int idx_deps = stack_top - 1;
		const int idx_iter = stack_top ;

		// dump_stack_top(L, 10);

		assert((stack_depth % 3) == 0);

		if (stack_depth == 0)
			break;

		if (opt_verbose)
		{
			lua_getfield(L, idx_dag, "id");
			printf("%s%s\n", spaces(stack_depth/3 - 1), lua_tostring(L, -1));
			lua_pop(L, 1);
		}

		// See if this node is already memoized as executed
		{
			lua_pushvalue(L, idx_dag);
			lua_gettable(L, idx_memo);

			if (!lua_isnil(L, -1))
			{
				lua_pop(L, 4);
				continue;
			}
			else
			{
				lua_pop(L, 1);
			}
		}

		const int dep_index = (int) lua_tointeger(L, idx_iter);
		lua_rawgeti(L, idx_deps, dep_index);

		if (lua_isnil(L, -1))
		{
			lua_pop(L, 1); // pop the nil result from lua_rawgeti

			// We've exhausted all dependencies for this node.
			// Attempt to make it to date.
			lua_pushcfunction(L, tundra_make0);
			lua_pushvalue(L, idx_dag);
			lua_call(L, 1, 1);
			int dag_result = (int) lua_tointeger(L, -1);
			lua_pop(L, 1); // pop result

			// Memoize this dag and its result
			lua_pushvalue(L, idx_dag);
			lua_pushinteger(L, dag_result);
			lua_settable(L, idx_memo);

			result_code |= dag_result;

			// Clean up stack at this level.
			lua_pop(L, 3);
		}
		else
		{
			lua_pushinteger(L, dep_index+1);
			lua_replace(L, idx_iter);

			// Push this new dependency on the stack.
			lua_getfield(L, -1, "deps");
			lua_pushinteger(L, 1);
		}
	}

	lua_pop(L, 1);
	lua_pushinteger(L, result_code);
	return 1;
}

static int tundra_process_dag(lua_State* L)
{
	const int num_args = lua_gettop(L);

	lua_getglobal(L, "Options");
	lua_getfield(L, -1, "Verbose");
	opt_verbose = lua_toboolean(L, -1) ? true : false;
	lua_pop(L, 1);

	int res = 0;

	lua_newtable(L);
	const int idx_memo = lua_gettop(L);

	for (int i=1; i<=num_args; ++i)
	{
		lua_pushcfunction(L, tundra_process_dag1);
		lua_pushvalue(L, idx_memo);
		lua_pushvalue(L, i);
		lua_call(L, 2, 1);
		res |= lua_tointeger(L, -1);
		lua_pop(L, 1);
	}
	
	return 0;
}

#if 0
void
NormalizedPath::normalize(const char* pathName)
{
	m_normalizedLength = 0;
	m_queryParameters = nullptr;
	m_segments.clear();

	// Iterate [pathName], looking for path separators. [segmentStart]
	// keeps track where the last path segment started. Note that this loop
	// extends to one beyond the string length (that is, the null terminator)
	// to process any segment following the last path separator--this is--the
	// one-past-the-last character of the string is treated as if it were a
	// path separator.

	const char* segmentStart = pathName;

	for (const char* i=pathName; ; ++i)
	{
		const char ch = *i;

		// Check if we're at a separator (or the end of the string)
		const bool is_separator = (0 == ch) | ('/' == ch) | ('\\' == ch) | ('?' == ch);

		if (is_separator)
		{
			const ptrdiff_t segmentSize = i - segmentStart;

			// Treat ".." as a pop from the segment stack.
			if (2 == segmentSize && segmentStart[0] == '.' && segmentStart[1] == '.')
			{
				if (!m_segments.empty())
					m_segments.pop_back();
			}

			// Collapse empty segments at the start of the path or between
			// double slashes, e.g. in foo//bar. Also collapse single
			// redundant single dots.
			else if (0 == segmentSize || (1 == segmentSize && segmentStart[0] == '.'))
			{
				// nop
			}

			else
			{
				// add the segment to the stack
				m_segments.push_back(Segment(segmentStart, segmentSize));
			}

			// If the new segment starts with a question mark, let the remaining string
			// be the query parameters and bail out.
			if ('?' == ch)
			{
				m_queryParameters = i + 1;
				return;
			}

			// Prepare the next segment start
			segmentStart = i + 1;
		}

		if (0 == ch)
			break; // We're done.
	}
}
#endif

static int tundra_open(lua_State* L)
{
	static const luaL_Reg path_walker_mt_entries[] = {
		{ "__gc", tundra_pathwalker_gc },
		{ NULL, NULL }
	};

	// Set up metatable for hasher objects
	luaL_newmetatable(L, "tundra.path_walker");
	lua_pushstring(L, "__index");
	lua_pushvalue(L, -2);
	lua_settable(L, -3);
	luaL_register(L, NULL, path_walker_mt_entries);
	lua_pop(L, 1);

	static const luaL_Reg hasher_mt_entries[] = {
		{ "AddString", tundra_hasher_add_string },
		{ "GetDigest", tundra_hasher_get_digest },
		{ NULL, NULL }
	};

	// Set up metatable for hasher objects
	luaL_newmetatable(L, "tundra.hasher");
	lua_pushstring(L, "__index");
	lua_pushvalue(L, -2);
	lua_settable(L, -3);
	luaL_register(L, NULL, hasher_mt_entries);
	lua_pop(L, 1);

	static const luaL_Reg engine_entries[] = {
		{ "Init", tundra_init },
		{ "AddFileSystem", tundra_add_fs },
		{ "ProcessDag", tundra_process_dag },
		{ "StatPath", tundra_stat_path },
		{ "CreateHasher", tundra_hasher_new },
		{ "GetDigest", tundra_get_digest },
		{ "WalkPath", tundra_walk_path },
		{ "NormalizePath", tundra_normalize_path },
		{ NULL, NULL }
	};

	luaL_register(L, "tundra.native.engine", engine_entries);
	lua_pop(L, 1);
	return 0;
}

// Algorithm:
// 1) Boot native image
// 2) Load boot lua script
// 3) Parse command line options
// 4) Read build scripts (query native side for everything about the FS through Change Journal)
// 5) Pass DAG to native exector

int main(int argc, char** argv)
{
	lua_State* L = luaL_newstate();
	luaL_openlibs(L);

	tundra_open(L);
	tundra::GraphBuilder::Export(L);

	char homeDir[260];
	if (const char* rt = getenv("TUNDRA_HOME"))
	{
		strncpy(homeDir, rt, sizeof homeDir);
		homeDir[sizeof homeDir - 1] = '\0';
	}
	else
		tundra::GetExecutableDir(homeDir, sizeof homeDir);

	char bootScript[260];
	snprintf(bootScript, sizeof(bootScript), "%s/scripts/boot.lua", homeDir);

	switch (luaL_loadfile(L, bootScript))
	{
	case LUA_ERRMEM:
		fprintf(stderr, "%s: out of memory\n", bootScript);
		return 1;
	case LUA_ERRSYNTAX:
		fprintf(stderr, "%s: syntax error\n%s\n", bootScript, lua_tostring(L, -1));
		return 1;
	case LUA_ERRFILE:
		fprintf(stderr, "%s: file not found", bootScript);
		return 1;
	}

	lua_newtable(L);
	lua_pushinteger(L, 1);
	lua_pushstring(L, homeDir);
	lua_settable(L, -3);

	for (int i=1; i<argc; ++i)
	{
		lua_pushinteger(L, i+1);
		lua_pushstring(L, argv[i]);
		lua_settable(L, -3);
	}

	int res = lua_pcall(L, 1, 1, 0);

	int rc = 0;

	if (res == 0)
	{
		rc = (int) lua_tointeger(L, -1);
	}
	else
	{
		fprintf(stderr, "%s\n", lua_tostring(L, -1));
		rc = 1;
	}

	lua_close(L);

	return rc;
}
