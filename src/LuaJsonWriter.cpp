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

struct BufferedWriter
{
  size_t m_Used;
  char m_Buffer[8192];
  FILE* m_File;
};

static void BufferedWriterInit(BufferedWriter* w, FILE* f)
{
  w->m_Used = 0;
  w->m_File = f;
}

static void BufferedWriterFlush(BufferedWriter* w)
{
  size_t remain = w->m_Used;
  const char* src = w->m_Buffer;
  while (remain)
  {
    size_t count = fwrite(src, 1, remain, w->m_File);
    src += count;
    remain -= count;
  }
  w->m_Used = 0;
}

inline void BufferedWriterAppend(BufferedWriter* w, int ch)
{
  size_t pos = w->m_Used;
  if (pos == ARRAY_SIZE(w->m_Buffer))
  {
    BufferedWriterFlush(w);
    pos = 0;
  }
  w->m_Buffer[pos] = (char) ch;
  w->m_Used = pos + 1;
}

static void BufferedWriterAppendString(BufferedWriter* w, const char* str)
{
  for (const char* p = str; *p; ++p)
  {
    BufferedWriterAppend(w, *p);
  }
}


struct LuaJsonWriter
{
  bool           m_FirstElem;
  BufferedWriter m_Writer;
};

static int LuaJsonWriterNew(lua_State* L)
{
  const char* filename = luaL_checkstring(L, 1);
  LuaJsonWriter* self = (LuaJsonWriter*) lua_newuserdata(L, sizeof(LuaJsonWriter));

  memset(self, 0, sizeof *self);

  self->m_FirstElem = true;

  if (FILE* f = fopen(filename, "w"))
  {
    BufferedWriterInit(&self->m_Writer, f);

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
  if (FILE* f = self->m_Writer.m_File)
  {
    BufferedWriterFlush(&self->m_Writer);
    self->m_Writer.m_File = nullptr;
    fclose(f);
  }

  return 0;
}

static char s_EscapeTable[256] = { 0 };

static void WriteEscapedString(BufferedWriter* w, const char* str, size_t len)
{
  BufferedWriterAppend(w, '"');

  for (size_t i = 0; i < len; ++i)
  {
    char ch = str[i];
    char escape_code = s_EscapeTable[(uint32_t) ch];

    if (escape_code)
    {
      BufferedWriterAppend(w, '\\');
      BufferedWriterAppend(w, escape_code);
    }
    else
    {
      BufferedWriterAppend(w, ch);
    }
  }

  BufferedWriterAppend(w, '"');
}

static void WriteComma(LuaJsonWriter* self)
{
  if (self->m_FirstElem)
  {
    self->m_FirstElem = false;
  }
  else
  {
    BufferedWriterAppend(&self->m_Writer, ',');
  }
}

static void WriteCommon(lua_State* L, LuaJsonWriter* self, int name_index)
{
  WriteComma(self);
  if (lua_gettop(L) >= name_index)
  {
    size_t len;
    const char* str = luaL_checklstring(L, name_index, &len);
    WriteEscapedString(&self->m_Writer, str, len);
    BufferedWriterAppend(&self->m_Writer, ':');
  }
}

static int LuaJsonWriteNumber(lua_State* L)
{
  LuaJsonWriter* self = (LuaJsonWriter*) luaL_checkudata(L, 1, "tundra_jsonw");
  lua_Number num = luaL_checknumber(L, 2);
  WriteCommon(L, self, 3);

  char buffer[64];
  lua_number2str(buffer, num);
  BufferedWriterAppendString(&self->m_Writer, buffer);
  return 0;
}

static int LuaJsonWriteBool(lua_State* L)
{
  LuaJsonWriter* self = (LuaJsonWriter*) luaL_checkudata(L, 1, "tundra_jsonw");
  int value = lua_toboolean(L, 2);
  WriteCommon(L, self, 3);
  BufferedWriterAppendString(&self->m_Writer, value ? "true" : "false");
  return 0;
}

static int LuaJsonWriteString(lua_State* L)
{
  LuaJsonWriter* self = (LuaJsonWriter*) luaL_checkudata(L, 1, "tundra_jsonw");
  size_t value_len;
  const char* value = lua_tolstring(L, 2, &value_len);
  WriteCommon(L, self, 3);
  WriteEscapedString(&self->m_Writer, value, value_len);
  return 0;
}

static int LuaJsonBeginObject(lua_State* L)
{
  LuaJsonWriter* self = (LuaJsonWriter*) luaL_checkudata(L, 1, "tundra_jsonw");
  WriteCommon(L, self, 2);
  BufferedWriterAppend(&self->m_Writer, '{');
  self->m_FirstElem = true;
  return 0;
}

static int LuaJsonEndObject(lua_State* L)
{
  LuaJsonWriter* self = (LuaJsonWriter*) luaL_checkudata(L, 1, "tundra_jsonw");
  BufferedWriterAppend(&self->m_Writer, '}');
  self->m_FirstElem = false;
  return 0;
}

static int LuaJsonBeginArray(lua_State* L)
{
  LuaJsonWriter* self = (LuaJsonWriter*) luaL_checkudata(L, 1, "tundra_jsonw");
  WriteCommon(L, self, 2);
  BufferedWriterAppend(&self->m_Writer, '[');
  self->m_FirstElem = true;
  return 0;
}

static int LuaJsonEndArray(lua_State* L)
{
  LuaJsonWriter* self = (LuaJsonWriter*) luaL_checkudata(L, 1, "tundra_jsonw");
  BufferedWriterAppend(&self->m_Writer, ']');
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

  s_EscapeTable[uint32_t('\n')] = 'n';
  s_EscapeTable[uint32_t('\r')] = 'r';
  s_EscapeTable[uint32_t('\t')] = 't';
  s_EscapeTable[uint32_t('\v')] = 'v';
  s_EscapeTable[uint32_t('\f')] = 'f';
  s_EscapeTable[uint32_t('\\')] = '\\';
  s_EscapeTable[uint32_t('"')] = '"';
}

}
