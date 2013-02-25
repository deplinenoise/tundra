#include "MemAllocHeap.hpp"
#include "Buffer.hpp"
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

extern "C" {
#include "lua.h"
#include "lauxlib.h"
}

namespace t2
{

struct LuaJsonWriter
{
  FILE *m_File;
  bool  m_FirstElem;
};

static int LuaJsonWriterNew(lua_State* L)
{
  const char* filename = luaL_checkstring(L, 1);
  LuaJsonWriter* self = (LuaJsonWriter*) lua_newuserdata(L, sizeof(LuaJsonWriter));

  self->m_File      = nullptr;
  self->m_FirstElem = true;

  if (FILE* f = fopen(filename, "w"))
  {
    self->m_File = f;

    luaL_getmetatable(L, "tundra_jsonw");
    lua_setmetatable(L, -2);
    return 1;
  }
  else
  {
    return luaL_error(L, "couldn't open %s for writing", filename);
  }
}

static int LuaJsonGc(lua_State* L)
{
  LuaJsonWriter* self = (LuaJsonWriter*) luaL_checkudata(L, 1, "tundra_jsonw");
  if (FILE* f = self->m_File)
  {
    self->m_File = nullptr;
    fclose(f);
  }

  return 0;
}

static void WriteEscapedString(FILE* f, const char* str)
{
  fputc('"', f);

  while (char ch = *str++)
  {
    char escape_code = 0;

    switch (ch)
    {
      case '\r': escape_code = 'r'; break;
      case '\n': escape_code = 'n'; break;
      case '\t': escape_code = 't'; break;
      case '\v': escape_code = 'v'; break;
      case '\f': escape_code = 'f'; break;
      case '\\': escape_code = '\\'; break;
      case '"': escape_code = '"'; break;
    }

    if (escape_code)
    {
      fputc('\\', f);
      fputc(escape_code, f);
    }
    else
    {
      fputc(ch, f);
    }
  }

  fputc('"', f);
}

static void WriteComma(LuaJsonWriter* self)
{
  if (self->m_FirstElem)
  {
    self->m_FirstElem = false;
  }
  else
  {
    fputc(',', self->m_File);
  }
}

static void WriteCommon(lua_State* L, LuaJsonWriter* self, int name_index)
{
  WriteComma(self);
  if (lua_gettop(L) >= name_index)
  {
    WriteEscapedString(self->m_File, luaL_checkstring(L, name_index));
    fputc(':', self->m_File);
  }
}

static int LuaJsonWriteNumber(lua_State* L)
{
  LuaJsonWriter* self = (LuaJsonWriter*) luaL_checkudata(L, 1, "tundra_jsonw");
  lua_Integer num = luaL_checkinteger(L, 2);
  WriteCommon(L, self, 3);
  FILE* f = self->m_File;
#ifndef _MSC_VER
  fprintf(f, "%lld", (long long) num);
#else
  fprintf(f, "%I64d", (__int64) num);
#endif
  return 0;
}

static int LuaJsonWriteBool(lua_State* L)
{
  LuaJsonWriter* self = (LuaJsonWriter*) luaL_checkudata(L, 1, "tundra_jsonw");
  int value = lua_toboolean(L, 2);
  WriteCommon(L, self, 3);
  fputs(value ? "true" : "false", self->m_File);
  return 0;
}

static int LuaJsonWriteString(lua_State* L)
{
  LuaJsonWriter* self = (LuaJsonWriter*) luaL_checkudata(L, 1, "tundra_jsonw");
  const char* value = lua_tostring(L, 2);
  WriteCommon(L, self, 3);
  WriteEscapedString(self->m_File, value);
  return 0;
}

static int LuaJsonBeginObject(lua_State* L)
{
  LuaJsonWriter* self = (LuaJsonWriter*) luaL_checkudata(L, 1, "tundra_jsonw");
  WriteCommon(L, self, 2);
  fputc('{', self->m_File);
  self->m_FirstElem = true;
  return 0;
}

static int LuaJsonEndObject(lua_State* L)
{
  LuaJsonWriter* self = (LuaJsonWriter*) luaL_checkudata(L, 1, "tundra_jsonw");
  fputc('}', self->m_File);
  self->m_FirstElem = false;
  return 0;
}

static int LuaJsonBeginArray(lua_State* L)
{
  LuaJsonWriter* self = (LuaJsonWriter*) luaL_checkudata(L, 1, "tundra_jsonw");
  WriteCommon(L, self, 2);
  fputc('[', self->m_File);
  self->m_FirstElem = true;
  return 0;
}

static int LuaJsonEndArray(lua_State* L)
{
  LuaJsonWriter* self = (LuaJsonWriter*) luaL_checkudata(L, 1, "tundra_jsonw");
  fputc(']', self->m_File);
  self->m_FirstElem = false;
  return 0;
}

void LuaJsonNativeOpen(lua_State* L)
{
  static luaL_Reg functions[] =
  {
    { "new",                        LuaJsonWriterNew },
    { nullptr,                      nullptr }
  };

  static luaL_Reg meta_table[] =
  {
    { "write_number",               LuaJsonWriteNumber },
    { "write_string",               LuaJsonWriteString },
    { "write_bool",                 LuaJsonWriteBool },
    { "begin_object",               LuaJsonBeginObject },
    { "end_object",                 LuaJsonEndObject },
    { "begin_array",                LuaJsonBeginArray },
    { "end_array",                  LuaJsonEndArray },
    { "close",                      LuaJsonGc },
    { "__gc",                       LuaJsonGc },
    { nullptr,                      nullptr }
  };

  luaL_register(L, "tundra.native.json", functions);
  lua_pop(L, 1);

  luaL_newmetatable(L, "tundra_jsonw");
  lua_pushvalue(L, -1);
  lua_setfield(L, -2, "__index");
  luaL_register(L, nullptr, meta_table);
  lua_pop(L, 1);
}

}
