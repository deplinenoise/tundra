#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include "util.h"
#include "bin_alloc.h"
#include "portable.h"
#include "md5.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#ifdef _WIN32
#define snprintf _snprintf
#endif

extern int tundra_walk_path(lua_State*);
extern void td_engine_open(lua_State*);
extern void td_scanner_open(lua_State*);
extern void td_cpp_scanner_open(lua_State*);

static int tundra_exit(lua_State *L)
{
	TD_UNUSED(L);
	exit(1);
}

static int tundra_digest_guid(lua_State *L)
{
	int i, cursor, narg;
	MD5_CTX ctx;
	unsigned char digest[16];
	char result[36];

	narg = lua_gettop(L);

	MD5Init(&ctx);
	for (i = 1; i <= narg; ++i)
	{
		size_t len;
		unsigned char* data = (unsigned char*) lua_tolstring(L, i, &len);
		MD5Update(&ctx, data, (unsigned int) len);
	}

	MD5Final(digest, &ctx);


	for (i = 0, cursor = 0; i<16; ++i)
	{
		static const char hex_tab[] = "0123456789ABCDEF";

		switch (i)
		{
		case 4: case 6: case 8: case 10:
			result[cursor++] = '-';
			break;
		}

		result[cursor++] = hex_tab[ digest[i] & 0x0f ];
		result[cursor++] = hex_tab[ (digest[i] & 0xf0) >> 4 ];
	}
	assert(cursor == 36);
	lua_pushlstring(L, result, sizeof(result));
	return 1;
}

static int tundra_getenv(lua_State *L)
{
	const char* key = luaL_checkstring(L, 1);
	const char* result = getenv(key);

	if (result)
	{
		lua_pushstring(L, result);
		return 1;
	}
	else if (lua_gettop(L) >= 2)
	{
		lua_pushvalue(L, 2);
		return 1;
	}
	else
	{
		return luaL_error(L, "key %s not present in environment (and no default given)", key);
	}
}

static int tundra_open(lua_State *L)
{
	static const luaL_Reg engine_entries[] = {
		/* used to quit after printing fatal error messages from Lua */
		{ "exit", tundra_exit },
		/* directory path traversal, similar to Python's os.walk */
		{ "walk_path", tundra_walk_path },
		/* digest passed in strings and return a string formatted in GUID style */
		{ "digest_guid", tundra_digest_guid },
		/* query for environment string */
		{ "getenv", tundra_getenv },
#ifdef _WIN32
		/* windows-specific registry query function*/
		{ "reg_query", td_win32_register_query },
#endif
		{ NULL, NULL }
	};

	luaL_register(L, "tundra.native", engine_entries);
	lua_pushstring(L, td_platform_string);
	lua_setfield(L, -2, "host_platform");

	/* native table on the top of the stack */
	td_engine_open(L);
	td_scanner_open(L);
	td_cpp_scanner_open(L);
	lua_pop(L, 1);
	return 0;
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

static int on_lua_panic(lua_State *L)
{
	TD_UNUSED(L);
	printf("lua panic! -- shutting down\n");
	exit(1);
}

int global_tundra_stats = 0;
int global_tundra_exit_code = 0;

int main(int argc, char** argv)
{
	td_bin_allocator bin_alloc;
	const char *homedir;
	char boot_script[260];
	int res, rc, i;
	lua_State* L;

	td_init_timer();

	if (NULL == (homedir = td_init_homedir()))
		return 1;

	td_bin_allocator_init(&bin_alloc);

	L = lua_newstate(td_lua_alloc, &bin_alloc);
	if (!L)
		exit(1);

	lua_atpanic(L, on_lua_panic);

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

	{
		double t1, t2;
		t1 = td_timestamp();
		res = lua_pcall(L, /*narg:*/1, /*nres:*/0, /*errorfunc:*/ -3);
		t2 = td_timestamp();
		if (global_tundra_stats)
			printf("total time spent in tundra: %.4fs\n", t2-t1);
	}

	if (res == 0)
	{
		rc = global_tundra_exit_code;
	}
	else
	{
		fprintf(stderr, "%s\n", lua_tostring(L, -1));
		rc = 1;
	}

	lua_close(L);

	td_bin_allocator_cleanup(&bin_alloc);

	return rc;
}
