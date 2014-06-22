#include "PathUtil.hpp"

extern "C" {
#include "lua.h"
#include "lauxlib.h"
}

namespace t2
{

static int LuaPathSplit(lua_State* L)
{
  PathBuffer pbuf;
  PathInit(&pbuf, luaL_checkstring(L, 1));

  if (pbuf.m_SegCount > 1)
  {
    char dir[kMaxPathLength];
    char file[kMaxPathLength];
    PathFormatPartial(dir, &pbuf, 0, pbuf.m_SegCount-2);
    PathFormatPartial(file, &pbuf, pbuf.m_SegCount-1, pbuf.m_SegCount-1);
    lua_pushstring(L, dir);
    lua_pushstring(L, file);
  }
  else
  {
    char file[kMaxPathLength];
    PathFormat(file, &pbuf);
    lua_pushstring(L, ".");
    lua_pushstring(L, file);
  }
  return 2;
}

static int LuaPathNormalize(lua_State* L)
{
  size_t input_len;
  const char* input = luaL_checklstring(L, 1, &input_len);

  PathBuffer buf;
  PathInit(&buf, input);

  char fmt[kMaxPathLength];
  PathFormat(fmt, &buf);

  lua_pushstring(L, fmt);
  return 1;
}

static int LuaPathJoin(lua_State* L)
{
  const char* p1 = luaL_checkstring(L, 1);
  const char* p2 = luaL_checkstring(L, 2);

  PathBuffer buf;
  PathInit(&buf, p1);
  PathConcat(&buf, p2);

  char fmt[kMaxPathLength];
  PathFormat(fmt, &buf);

  lua_pushstring(L, fmt);
  return 1;
}

static int LuaPathGetFilenameDir(lua_State* L)
{
  const char* path = luaL_checkstring(L, 1);

  PathBuffer buf;
  PathInit(&buf, path);
  if (PathStripLast(&buf))
  {
    char fmt[kMaxPathLength];
    PathFormat(fmt, &buf);
    lua_pushstring(L, fmt);
    return 1;
  }
  else
  {
    lua_pushstring(L, ".");
    return 1;
  }
}

static int LuaPathGetFilename(lua_State* L)
{
  PathBuffer pbuf;
  PathInit(&pbuf, luaL_checkstring(L, 1));
  if (pbuf.m_SegCount > 0)
  {
    char buf[kMaxPathLength];
    PathFormatPartial(buf, &pbuf, pbuf.m_SegCount-1, pbuf.m_SegCount-1);
    lua_pushstring(L, buf);
  }
  else
  {
    lua_pushstring(L, "");
  }
  return 1;
}

static int LuaPathGetExtension(lua_State* L)
{
  const char* input = luaL_checkstring(L, 1);
  
  // Make sure to look at the last path segment only.
  if (const char* pslash = strrchr(input, '/'))
  {
    input = pslash;
  }

  if (const char* pslash = strrchr(input, '\\'))
  {
    if (pslash > input)
      input = pslash;
  }

  if (const char* dot = strrchr(input, '.'))
    lua_pushstring(L, dot);
  else
    lua_pushstring(L, "");
  return 1;
}

static int LuaPathDropSuffix(lua_State* L)
{
  const char* input = luaL_checkstring(L, 1);
  if (const char* dot = strrchr(input, '.'))
    lua_pushlstring(L, input, size_t(dot - input));
  else
    lua_pushvalue(L, 1);
  return 1;
}

static int LuaPathGetFilenameBase(lua_State* L)
{
  PathBuffer pbuf;
  PathInit(&pbuf, luaL_checkstring(L, 1));
  if (pbuf.m_SegCount > 0)
  {
    char buf[kMaxPathLength];
    PathFormatPartial(buf, &pbuf, pbuf.m_SegCount-1, pbuf.m_SegCount-1);
    if (char* dot = strrchr(buf, '.'))
      *dot = '\0';
    lua_pushstring(L, buf);
  }
  else
  {
    lua_pushstring(L, "");
  }
  return 1;
}

static int LuaPathIsAbsolute(lua_State* L)
{
  PathBuffer pbuf;
  PathInit(&pbuf, luaL_checkstring(L, 1));
  lua_pushboolean(L, PathIsAbsolute(&pbuf) ? 1 : 0);
  return 1;
}

void LuaPathNativeOpen(lua_State* L)
{
  static luaL_Reg functions[] =
  {
    { "split",                LuaPathSplit },
    { "normalize",            LuaPathNormalize },
    { "join",                 LuaPathJoin },
    { "get_filename_dir",     LuaPathGetFilenameDir },
    { "get_filename",         LuaPathGetFilename },
    { "get_extension",        LuaPathGetExtension },
    { "drop_suffix",          LuaPathDropSuffix },
    { "get_filename_base",    LuaPathGetFilenameBase },
    { "is_absolute",          LuaPathIsAbsolute },
    { nullptr,                      nullptr },
  };

  luaL_register(L, "tundra.native.path", functions);
  lua_pop(L, 1);
}

}
