#include "MemAllocHeap.hpp"
#include "Hash.hpp"
#include "FileInfo.hpp"
#include "BinaryWriter.hpp"
#include "DagData.hpp"
#include "PathUtil.hpp"

extern "C"
{
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
}

#include <cstdlib>

#if defined(TUNDRA_WIN32)
#include <windows.h>
#endif

#if defined(_MSC_VER)
#define snprintf _snprintf
#endif

namespace t2
{

static void* LuaAllocFunc(void* ud, void* old_ptr, size_t old_size, size_t new_size)
{
  MemAllocHeap* heap = static_cast<MemAllocHeap*>(ud);
  if (new_size && old_size)
    return HeapReallocate(heap, old_ptr, new_size);
  else if (new_size)
    return HeapAllocate(heap, new_size);
  else if (old_size)
    HeapFree(heap, old_ptr);

  return nullptr;
}

static int OnLuaPanic(lua_State *)
{
  Croak("lua panic!");
}

static int LuaTundraExit(lua_State *)
{
	exit(1);
}

static void DoDigest(HashDigest* out, lua_State* L)
{
  HashState h;
  HashInit(&h);

  int narg = lua_gettop(L);

  for (int i = 1; i <= narg; ++i)
  {
    size_t len;
    unsigned char* data = (unsigned char*) luaL_checklstring(L, i, &len);
    HashUpdate(&h, data, len);
  }

  HashFinalize(&h, out);
}

static int LuaTundraDigestGuidRaw(lua_State* L)
{
  HashDigest digest;
  DoDigest(&digest, L);
	lua_pushlstring(L, (char*) digest.m_Data, sizeof digest);
	return 1;
}

static int LuaTundraDigestGuid(lua_State* L)
{
  HashDigest digest;
  DoDigest(&digest, L);
  char result[41];
  DigestToString(result, digest);
	lua_pushlstring(L, result, 40);
	return 1;
}

static int LuaTundraGetEnv(lua_State *L)
{
	const char* key    = luaL_checkstring(L, 1);
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

static int LuaGetTraceback(lua_State *L)
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

static void OnFileIter(void* user_data, const FileInfo& info, const char* name)
{
  lua_State* L = static_cast<lua_State*>(user_data);

  if (info.IsFile()) 
  {
    lua_pushstring(L, name);
    lua_rawseti(L, -2, (int) lua_objlen(L, -2) + 1);
  }
  else if (info.IsDirectory()) 
  {
    lua_pushstring(L, name);
    lua_rawseti(L, -3, (int) lua_objlen(L, -3) + 1);
  }
  else
  {
    Log(kWarning, "ignoring unsupported file type: %s\n", name);
  }
}

static int LuaListDirectory(lua_State* L)
{
  const char* dir = luaL_checkstring(L, 1);

  // Push dir result table on stack
  lua_newtable(L);
  // Push file result table on stack
  lua_newtable(L);

  ListDirectory(dir, L, OnFileIter);

  // Sort both tables
  lua_getglobal(L, "table");
  lua_getfield(L, -1, "sort");

  // Sort dirs
  // stack here: dirs, files, table package, table.sort
  lua_pushvalue(L, -1); // table.sort
  lua_pushvalue(L, -5);
  lua_call(L, 1, 0);

  // Sort files
  // stack here: dirs, files, table package, table.sort
  lua_pushvalue(L, -1); // table.sort
  lua_pushvalue(L, -4);
  lua_call(L, 1, 0);

  lua_pop(L, 2);

  // stack here: dirs, files

  // Compute directory listing hash digest for signature purposes
  HashState h;
  HashInit(&h);

  // Hash directories
  for (size_t i = 1, count = lua_objlen(L, -2); i <= count; ++i)
  {
    lua_rawgeti(L, -2, (int) i);
    HashAddString(&h, lua_tostring(L, -1));
    HashAddSeparator(&h);
    lua_pop(L, 1);
  }

  // Hash files
  for (size_t i = 1, count = lua_objlen(L, -1); i <= count; ++i)
  {
    lua_rawgeti(L, -1, (int) i);
    HashAddString(&h, lua_tostring(L, -1));
    HashAddSeparator(&h);
    lua_pop(L, 1);
  }

  HashDigest digest;
  HashFinalize(&h, &digest);

  lua_pushlstring(L, (char*)digest.m_Data, sizeof digest.m_Data);

  return 3;
}

static int LuaStatFile(lua_State* L)
{
  const char* fn = luaL_checkstring(L, 1);

  FileInfo info = GetFileInfo(fn);

  // Push dir result table on stack
  lua_newtable(L);
  lua_pushinteger(L, (lua_Integer) info.m_Size);
  lua_setfield(L, -2, "size");
  lua_pushinteger(L, (lua_Integer) info.m_Timestamp);
  lua_setfield(L, -2, "timestamp");
  lua_pushboolean(L, info.IsDirectory());
  lua_setfield(L, -2, "isdirectory");
  lua_pushboolean(L, info.Exists());
  lua_setfield(L, -2, "exists");

  return 1;
}

static int LuaMakePathBuf(lua_State* L)
{
  const char* fn = luaL_checkstring(L, 1);
  PathBuffer buffer;
  PathInit(&buffer, fn);

  lua_newtable(L);
  lua_pushinteger(L, buffer.m_Type);
  lua_setfield(L, -2, "type");
  lua_pushinteger(L, buffer.m_Flags);
  lua_setfield(L, -2, "flags");
  lua_pushinteger(L, buffer.m_LeadingDotDots);
  lua_setfield(L, -2, "leading_dotdots");

  lua_newtable(L);
  for (int i = 0; i < buffer.m_SegCount; ++i)
  {
    lua_pushinteger(L, buffer.m_SegEnds[i]);
    lua_rawseti(L, -2, i + 1);
  }
  lua_setfield(L, -2, "seg_ends");

  int len = buffer.m_SegCount > 0 ? buffer.m_SegEnds[buffer.m_SegCount - 1] : 0;
  lua_pushlstring(L, buffer.m_Data, len);
  lua_setfield(L, -2, "data");

  return 1;
}

static int LuaDjb2Hash(lua_State* L)
{
  const char* name = luaL_checkstring(L, 1);
  lua_pushinteger(L, Djb2Hash(name));
  return 1;
}

static int LuaDjb2HashPath(lua_State* L)
{
  const char* name = luaL_checkstring(L, 1);
  lua_pushinteger(L, Djb2HashPath(name));
  return 1;
}

static int LuaGetCwd(lua_State* L)
{
  char buf[kMaxPathLength];
  GetCwd(buf, sizeof buf);
  lua_pushstring(L, buf);
  return 1;
}

static int LuaMkdir(lua_State* L)
{
  if (!MakeDirectory(luaL_checkstring(L, 1)))
    return luaL_error(L, "couldn't create %s", lua_tostring(L, 1));
  return 0;
}

#if defined(TUNDRA_WIN32)
static void PushWin32Error(lua_State *L, DWORD err, const char *context)
{
	int chars;
	char buf[1024];
	lua_pushstring(L, context);
	lua_pushstring(L, ": ");
	if (0 != (chars = (int) FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, err, LANG_NEUTRAL, buf, sizeof(buf), NULL)))
		lua_pushlstring(L, buf, chars);
	else
		lua_pushfstring(L, "win32 error %d", (int) err);
	lua_concat(L, 3);
}

static int LuaWin32RegisterQuery(lua_State *L)
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
					PushWin32Error(L, (DWORD) result, "RegQueryValueExA");
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
			PushWin32Error(L, (DWORD) result, "RegOpenKeyExA");
			return 2;
		}

	}


	lua_pushnil(L);
	PushWin32Error(L, ERROR_FILE_NOT_FOUND, "RegOpenKeyExA");
	return 2;
}
#endif

static const luaL_Reg s_LuaFunctions[] = {
  { "exit",             LuaTundraExit },
  { "list_directory",   LuaListDirectory },
  { "digest_guid",      LuaTundraDigestGuid },
  { "digest_guid_raw",  LuaTundraDigestGuidRaw },
  { "getenv",           LuaTundraGetEnv },
  { "stat_file",        LuaStatFile },
  { "make_pathbuf",     LuaMakePathBuf },
  { "djb2_hash",        LuaDjb2Hash },
  { "djb2_hash_path",   LuaDjb2HashPath },
  { "getcwd",           LuaGetCwd },
  { "mkdir",            LuaMkdir },
#ifdef _WIN32
  { "reg_query",        LuaWin32RegisterQuery },
#endif
  { NULL, NULL }
};

void LuaEnvNativeOpen(lua_State* L);
void LuaJsonNativeOpen(lua_State* L);

lua_State* CreateLuaState(MemAllocHeap* lua_heap)
{
  lua_State* L = lua_newstate(LuaAllocFunc, lua_heap);

  if (!L)
    Croak("couldn't create Lua state");

  // Install panic function.
	lua_atpanic(L, OnLuaPanic);

  // Expose standard Lua libraries.
	luaL_openlibs(L);

  // Expose interpolation module
  LuaEnvNativeOpen(L);

  // Expose JSON writer module
  LuaJsonNativeOpen(L);

  luaL_register(L, "tundra.native", s_LuaFunctions);
  lua_pushstring(L, TUNDRA_PLATFORM_STRING);
  lua_setfield(L, -2, "host_platform");

	// setup package.path
	{
    const char* homedir = GetTundraHomeDirectory();

		char ppath[1024];
		snprintf(ppath, sizeof(ppath),
			"%s" TD_PATHSEP_STR "scripts" TD_PATHSEP_STR "?.lua;"
			"%s" TD_PATHSEP_STR "lua" TD_PATHSEP_STR "etc" TD_PATHSEP_STR "?.lua", homedir, homedir);
		lua_getglobal(L, "package");
		CHECK(LUA_TTABLE == lua_type(L, -1));
		lua_pushstring(L, ppath);
		lua_setfield(L, -2, "path");
	}

  /* native table on the top of the stack */
  lua_pop(L, 1);

  Log(kDebug, "Lua initialized successfully");
  return L;
}

static const char s_BootSnippet[] =
	"local m = require 'tundra'\n"
	"return m.main(...)\n";

bool RunBuildScript(lua_State *L, const char* action, const char *script_fn, const char** args, int argc_count)
{
	// Push our error handler on the stack now (before the chunk to run)
	lua_pushcclosure(L, LuaGetTraceback, 0);
  int error_index = lua_gettop(L);

	switch (luaL_loadbuffer(L, s_BootSnippet, sizeof(s_BootSnippet)-1, "boot_snippet"))
	{
	case LUA_ERRMEM    : Croak("out of memory");
	case LUA_ERRSYNTAX : Croak("syntax error\n%s\n", lua_tostring(L, -1));
	}

  lua_pushstring(L, action);
  lua_pushstring(L, script_fn);
  for (int i = 0; i < argc_count; ++i)
  {
    lua_pushstring(L, args[i]);
  }

  int res = lua_pcall(L, /*narg:*/2 + argc_count, /*nres:*/1, /*errorfunc:*/ error_index);

  if (0 == res)
  {
    return true;
  }
  else
  {
    // pcall failed
    if (lua_isstring(L, -1))
    {
      fprintf(stderr, "Lua error running %s\n\n%s\n", script_fn, lua_tostring(L, -1));
    }
    return false;
  }
}

}
