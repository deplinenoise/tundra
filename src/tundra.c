#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include "util.h"
#include "bin_alloc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#ifdef _MSC_VER
#include <windows.h>
#define snprintf _snprintf
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

extern int tundra_walk_path(lua_State*);
extern void td_engine_open(lua_State*);
extern void td_scanner_open(lua_State*);
extern void td_cpp_scanner_open(lua_State*);

static const char *get_platform_string(void)
{
#if defined(__APPLE__)
	return "macosx";
#elif defined(linux)
	return "linux";
#elif defined(_WIN32)
	return "windows";
#endif
}

#ifdef _WIN32
static void push_win32_error(lua_State *L, DWORD err, const char *context)
{
	int chars;
	char buf[1024];
	lua_pushstring(L, context);
	lua_pushstring(L, ": ");
	if (0 != (chars = (int) FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, err, LANG_NEUTRAL, buf, sizeof(buf), NULL)))
		lua_pushlstring(L, buf, chars);
	else
		lua_pushfstring(L, "win32 error %d", (int) err);
	lua_concat(L, 3);
}

static int reg_query(lua_State *L)
{
	HKEY regkey, root_key;
	LONG result = 0;
	const char *key_name, *subkey_name, *value_name = NULL;
	int i;
	static const REGSAM sams[] = { KEY_READ, KEY_READ|KEY_WOW64_32KEY, KEY_READ|KEY_WOW64_64KEY };

	key_name = luaL_checkstring(L, 1);

	if (0 == strcmp(key_name, "HKLM") || 0 == strcmp(key_name, "HKEY_LOCAL_MACHINE"))
		root_key = HKEY_LOCAL_MACHINE;
	else if (0 == strcmp(key_name, "HKCU") || 0 == strcmp(key_name, "HKEY_CURRENT_USER"))
		root_key = HKEY_CURRENT_USER;
	else
		return luaL_error(L, "%s: unsupported root key; use HKLM, HKCU or HKEY_LOCAL_MACHINE, HKEY_CURRENT_USER", key_name);

	subkey_name = luaL_checkstring(L, 2);

	if (lua_gettop(L) >= 3 && lua_isstring(L, 3))
		value_name = lua_tostring(L, 3);

	for (i = 0; i < sizeof(sams)/sizeof(sams[0]); ++i)
	{
		result = RegOpenKeyExA(root_key, subkey_name, 0, sams[i], &regkey);

		if (ERROR_SUCCESS == result)
		{
			DWORD stored_type;
			BYTE data[8192];
			DWORD data_size = sizeof(data);
			result = RegQueryValueExA(regkey, value_name, NULL, &stored_type, &data[0], &data_size);
			RegCloseKey(regkey);

			if (ERROR_FILE_NOT_FOUND != result)
			{
				if (ERROR_SUCCESS != result)
				{
					lua_pushnil(L);
					push_win32_error(L, (DWORD) result, "RegQueryValueExA");
					return 2;
				}

				switch (stored_type)
				{
				case REG_DWORD:
					if (4 != data_size)
						luaL_error(L, "expected 4 bytes for integer key but got %d", data_size);
					lua_pushinteger(L, *(int*)data);
					return 1;

				case REG_SZ:
					/* don't use lstring because that would include potential null terminator */
					lua_pushstring(L, (const char*) data);
					return 1;

				default:
					return luaL_error(L, "unsupported registry value type (%d)", (int) stored_type);
				}
			}
		}
		else if (ERROR_FILE_NOT_FOUND == result)
		{
			continue;
		}
		else
		{
			lua_pushnil(L);
			push_win32_error(L, (DWORD) result, "RegOpenKeyExA");
			return 2;
		}

	}


	lua_pushnil(L);
	push_win32_error(L, ERROR_FILE_NOT_FOUND, "RegOpenKeyExA");
	return 2;
}
#endif

static int tundra_open(lua_State* L)
{
	static const luaL_Reg engine_entries[] = {
		{ "walk_path", tundra_walk_path },
#ifdef _WIN32
		{ "reg_query", reg_query },
#endif
		{ NULL, NULL }
	};

	luaL_register(L, "tundra.native", engine_entries);
	lua_pushstring(L, get_platform_string());
	lua_setfield(L, -2, "host_platform");

	/* native table on the top of the stack */
	td_engine_open(L);
	td_scanner_open(L);
	td_cpp_scanner_open(L);
	lua_pop(L, 1);
	return 0;
}

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
	if (NULL != (tmp = getenv("TUNDRA_HOME")))
		return set_homedir(tmp);

#if defined(_WIN32)
	if (0 == GetModuleFileNameA(NULL, homedir, (DWORD)sizeof(homedir)))
		return 1;

	if (NULL != (tmp = strrchr(homedir, '\\')))
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
	if (-1 == readlink("/proc/self/exe", homedir, sizeof(homedir)))
		return 1;

	if ((tmp = strrchr(homedir, '/')))
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

static int on_lua_panic(lua_State *L)
{
	printf("lua panic! -- shutting down\n");
	exit(1);
}

int main(int argc, char** argv)
{
	td_bin_allocator bin_alloc;
	char boot_script[260];
	int res, rc, i;
	lua_State* L;

	if (0 != init_homedir())
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

	td_bin_allocator_cleanup(&bin_alloc);

	return rc;
}
