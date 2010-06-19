#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _MSC_VER
#include <windows.h>
#define snprintf _snprintf
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

static int tundra_open(lua_State* L)
{
	extern int tundra_walk_path(lua_State*);

	static const luaL_Reg engine_entries[] = {
		{ "walk_path", tundra_walk_path },
		{ NULL, NULL }
	};

	luaL_register(L, "tundra.native", engine_entries);
	lua_pop(L, 1);
	return 0;
}

// Algorithm:
// 1) Boot native image
// 2) Load boot lua script
// 3) Parse command line options
// 4) Read build scripts (query native side for everything about the FS through Change Journal)
// 5) Pass DAG to native exector

static char homedir[260];

static int set_homedir(const char* dir)
{
	strncpy(homedir, dir, sizeof homedir);
	homedir[sizeof homedir - 1] = '\0';
	return 0;
}

static int init_homedir()
{
	char* tmp;
	if (tmp = getenv("TUNDRA_HOME"))
		return set_homedir(tmp);

#if defined(_WIN32)
	if (0 == GetModuleFileNameA(NULL, homedir, (DWORD)sizeof(homedir)))
		return 1;

	if ((tmp = strrchr(homedir, '\\')))
		*tmp = 0;
	return 0;

#elif defined(__APPLE__)
	char path[1024], resolved_path[1024];
	uint32_t size = sizeof(path);
	if (_NSGetExecutablePath(path, &size) != 0)
		return 1;
	if ((tmp = realpath(path, resolved_path))) 
		return set_homedir(tmp);
	else
		return 1;

#elif defined(linux)
	if (-1 == readlink("/proc/self/exe", path, path_max))
		return 1;

	if ((tmp = strrchr(path, '/')))
		*tmp = 0;
	return 0;
#else
#error "unsupported platform"
#endif
}

static int get_traceback(lua_State *L)
{
	lua_getfield(L, LUA_GLOBALSINDEX, "debug");
	if (!lua_istable(L, -1))
	{
		lua_pop(L, 1);
		return 1;
	}
	lua_getfield(L, -1, "traceback");
	if (!lua_isfunction(L, -1))
	{
		lua_pop(L, 2);
		return 1;
	}
	lua_pushvalue(L, 1);  /* pass error message */
	lua_pushinteger(L, 2);  /* skip this function and traceback */
	lua_call(L, 2, 1);  /* call debug.traceback */
	return 1;
}

int main(int argc, char** argv)
{
	char boot_script[260];
	int res, rc, i;
	lua_State* L;

	if (0 != init_homedir())
		return 1;

	L = luaL_newstate();
	luaL_openlibs(L);

	tundra_open(L);

	snprintf(boot_script, sizeof(boot_script), "%s/scripts/boot.lua", homedir);

	/* push our error handler on the stack now (before the chunk to run) */
	lua_pushcclosure(L, get_traceback, 0);

	switch (luaL_loadfile(L, boot_script))
	{
	case LUA_ERRMEM:
		fprintf(stderr, "%s: out of memory\n", boot_script);
		return 1;
	case LUA_ERRSYNTAX:
		fprintf(stderr, "%s: syntax error\n%s\n", boot_script, lua_tostring(L, -1));
		return 1;
	case LUA_ERRFILE:
		fprintf(stderr, "%s: file not found\n", boot_script);
		return 1;
	}

	lua_newtable(L);
	lua_pushstring(L, homedir);
	lua_rawseti(L, -2, 1);

	for (i=1; i<argc; ++i)
	{
		lua_pushstring(L, argv[i]);
		lua_rawseti(L, -2, i+1);
	}

	res = lua_pcall(L, /*narg:*/1, /*nres:*/1, /*errorfunc:*/ -3);

	rc = 0;

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
