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

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include "util.h"
#include "bin_alloc.h"
#include "portable.h"
#include "md5.h"

#if defined(TD_STANDALONE)
#include "gen_lua_data.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#ifdef _WIN32
#define snprintf _snprintf
#endif

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

	MD5_Init(&ctx);
	for (i = 1; i <= narg; ++i)
	{
		size_t len;
		unsigned char* data = (unsigned char*) lua_tolstring(L, i, &len);
		MD5_Update(&ctx, data, (unsigned int) len);
	}

	MD5_Final(digest, &ctx);


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

static int tundra_delete_file(lua_State *L)
{
	const char *fn = luaL_checkstring(L, 1);
	if (0 == remove(fn))
	{
		lua_pushboolean(L, 1);
		return 1;
	}
	else
	{
		lua_pushnil(L);
		lua_pushstring(L, "couldn't delete file");
		return 2;
	}
}

static int tundra_rename_file(lua_State *L)
{
	const char *from = luaL_checkstring(L, 1);
	const char *to = luaL_checkstring(L, 2);
	if (0 == rename(from, to))
	{
		lua_pushboolean(L, 1);
		return 1;
	}
	else
	{
		lua_pushnil(L);
		lua_pushstring(L, "couldn't rename file");
		return 2;
	}
}

extern int td_luaprof_install(lua_State *L);
extern int td_luaprof_report(lua_State *L);
extern int tundra_walk_path(lua_State*);
extern void td_engine_open(lua_State*);
extern int td_scanner_open(lua_State*);
extern int td_cpp_scanner_open(lua_State*);
extern int td_get_cwd(lua_State*);
extern int td_set_cwd(lua_State*);
extern int td_sanitize_lua_path(lua_State*);

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
		/* working dir mgmt */
		{ "get_cwd", td_get_cwd },
		{ "set_cwd", td_set_cwd },
		{ "install_profiler", td_luaprof_install },
		{ "report_profiler", td_luaprof_report },
		{ "delete_file", tundra_delete_file },
		{ "rename_file", tundra_rename_file },
		{ "sanitize_path", td_sanitize_lua_path },
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

#if defined(TD_STANDALONE)
static int
td_load_embedded_file(lua_State *L)
{
	int i, count;
	const char *module_name = luaL_checkstring(L, 1);

	for (i = 0, count = td_lua_file_count; i < count; ++i)
	{
		if (0 == strcmp(td_lua_files[i].module_name, module_name))
		{
			if (0 != luaL_loadbuffer(L, td_lua_files[i].data, td_lua_files[i].size, module_name))
				td_croak("couldn't load embedded chunk for %s", module_name);
			return 1;
		}
	}
	
	return 0;
}
#endif

static int on_lua_panic(lua_State *L)
{
	TD_UNUSED(L);
	printf("lua panic! -- shutting down\n");
	exit(1);
}

double script_call_t1 = 0.0;
int global_tundra_stats = 0;
int global_tundra_exit_code = 0;

static const char boot_snippet[] =
	"local m = require 'tundra.boot'\n"
	"m.main(...)\n";

int main(int argc, char** argv)
{
	td_bin_allocator bin_alloc;
	const char *homedir;
	int res, rc, i;
	lua_State* L;

	td_init_portable();

	if (NULL == (homedir = td_init_homedir()))
		return 1;

	td_bin_allocator_init(&bin_alloc);

	L = lua_newstate(td_lua_alloc, &bin_alloc);
	if (!L)
		exit(1);

	lua_atpanic(L, on_lua_panic);

	luaL_openlibs(L);

	tundra_open(L);

#if defined(TD_STANDALONE)
	/* this is equivalent to table.insert(package.loaders, 1, td_load_embedded_file) */

	/* get the function */
	lua_getglobal(L, "table");
	lua_getfield(L, -1, "insert");
	lua_remove(L, -2);
	assert(!lua_isnil(L, -1));

	/* arg1: the package.loaders table */
	lua_getglobal(L, "package");
	lua_getfield(L, -1, "loaders");
	lua_remove(L, -2);
	assert(!lua_isnil(L, -1));

	lua_pushinteger(L, 1); /* arg 2 */
	lua_pushcfunction(L, td_load_embedded_file); /* arg 3 */

	lua_call(L, 3, 0);
#endif

	/* setup package.path */
	{
		char ppath[1024];
		snprintf(ppath, sizeof(ppath),
			"%s" TD_PATHSEP_STR "scripts" TD_PATHSEP_STR "?.lua;"
			"%s" TD_PATHSEP_STR "lua" TD_PATHSEP_STR "etc" TD_PATHSEP_STR "?.lua", homedir, homedir);
		lua_getglobal(L, "package");
		assert(LUA_TTABLE == lua_type(L, -1));
		lua_pushstring(L, ppath);
		lua_setfield(L, -2, "path");
	}

	/* push our error handler on the stack now (before the chunk to run) */
	lua_pushcclosure(L, get_traceback, 0);

	switch (luaL_loadbuffer(L, boot_snippet, sizeof(boot_snippet)-1, "boot_snippet"))
	{
	case LUA_ERRMEM:
		td_croak("out of memory");
		return 1;
	case LUA_ERRSYNTAX:
		td_croak("syntax error\n%s\n", lua_tostring(L, -1));
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
		double t2;
		script_call_t1 = td_timestamp();
		res = lua_pcall(L, /*narg:*/1, /*nres:*/0, /*errorfunc:*/ -3);
		t2 = td_timestamp();
		if (global_tundra_stats)
			printf("total time spent in tundra: %.4fs\n", t2 - script_call_t1);
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
