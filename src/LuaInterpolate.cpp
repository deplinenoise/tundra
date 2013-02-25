
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

class LuaEnvLookup
{
private:
  lua_State *m_LuaState;
  int        m_FunctionIndex;

public:
  LuaEnvLookup(lua_State* L, int func_index)
  {
    m_LuaState      = L;
    m_FunctionIndex = func_index;
  }

private:
  friend class LuaEnvLookupScope;
};

class LuaEnvLookupScope
{
private:
  lua_State* m_LuaState;
  bool       m_Valid;
  size_t     m_Count;

public:
  explicit LuaEnvLookupScope(LuaEnvLookup& lookup, const char* key, size_t len)
  {
    m_LuaState = lookup.m_LuaState;
    m_Valid    = false;

    lua_State* L = lookup.m_LuaState;
    lua_checkstack(L, 5);
    lua_pushvalue(L, lookup.m_FunctionIndex);
    lua_pushlstring(L, key, len);
    if (0 == lua_pcall(L, 1, 1, 0))
    {
      if (LUA_TTABLE == lua_type(L, -1))
      {
        m_Count = lua_objlen(L, -1);
        m_Valid = true;
      }
      else
      {
        fprintf(stderr, "env resolve failed: didn't return a table: %s\n", lua_typename(L, lua_type(L, -1)));
        lua_pop(L, 1);
      }
    }
    else
    {
      fprintf(stderr, "env resolve failed: %s\n", lua_tostring(L, -1));
      lua_pop(L, 1);
    }
  }

  size_t GetCount()
  {
    return m_Count;
  }

  const char* GetValue(size_t index, size_t* len_out)
  {
    lua_State* L = m_LuaState;
    CHECK(lua_type(L, -1) == LUA_TTABLE);
    lua_rawgeti(L, -1, int(index + 1));
    if (lua_type(L, -1) != LUA_TSTRING)
    {
      fprintf(stderr, "env lookup failed: elem %d is not a string: %s\n", (int) index + 1, lua_typename(L, lua_type(L, -1)));
      return nullptr;
    }
    // This is technically not allowed but we do it anyway for performance.
    // We check that the value is already a string, and we know it's being
    // kept alive by the table which is on our stack.
    const char* str = lua_tolstring(L, -1, len_out);
    lua_pop(L, 1);
    return str;
  }

  ~LuaEnvLookupScope()
  {
    if (m_Valid)
    {
      lua_pop(m_LuaState, 1);
    }
  }

  bool Valid()
  {
    return m_Valid;
  }
  
private:
  LuaEnvLookupScope(const LuaEnvLookupScope&);
  LuaEnvLookupScope& operator=(const LuaEnvLookupScope&);
};

class StringBuffer
{
private:
  enum
  {
    kInternalBufferSize = 64
  };

  MemAllocHeap *m_Heap;
  size_t        m_InternalBufferUsed;
  char          m_InternalBuffer[kInternalBufferSize];
  Buffer<char>  m_Buffer;

public:
  explicit StringBuffer(MemAllocHeap* heap)
  {
    m_Heap               = heap;
    m_InternalBufferUsed = 0;
    BufferInit(&m_Buffer);
  }

  ~StringBuffer()
  {
    BufferDestroy(&m_Buffer, m_Heap);
  }

  void Add(const char* data, size_t len)
  {
    char* dest = Grow(len);
    memcpy(dest, data, len);
  }

  char* Grow(size_t len)
  {
    if (m_Buffer.m_Size)
    {
      return BufferAlloc(&m_Buffer, m_Heap, len);
    }
    else if (len + m_InternalBufferUsed <= kInternalBufferSize)
    {
      size_t offset = m_InternalBufferUsed;
      m_InternalBufferUsed += len;
      return m_InternalBuffer + offset;
    } 
    else
    {
      BufferAppend(&m_Buffer, m_Heap, m_InternalBuffer, m_InternalBufferUsed);
      return BufferAlloc(&m_Buffer, m_Heap, len);
    }
  }

  void Shrink(size_t size)
  {
    CHECK(size <= GetSize());
    if (m_Buffer.m_Size)
    {
      m_Buffer.m_Size = size;
    }
    else
    {
      CHECK(size <= kInternalBufferSize);
      m_InternalBufferUsed = size;
    }
  }

  void NullTerminate()
  {
    Add("", 1);
  }

  MemAllocHeap* GetHeap()
  {
    return m_Heap;
  }

  size_t GetSize()
  {
    if (size_t sz = m_Buffer.m_Size)
      return sz;
    else
      return m_InternalBufferUsed;
  }

  char* GetBuffer()
  {
    if (m_Buffer.m_Size)
      return m_Buffer.m_Storage;
    else
      return m_InternalBuffer;
  }

  // Transformation functions invoked by interpolation options
  void Prepend(const char* data, size_t len)
  {
    Grow(len);

    char   *buffer = GetBuffer();
    size_t  buflen = GetSize();

    memmove(buffer + len, buffer, buflen - len);
    memcpy(buffer, data, len);
  }

  void Append(const char* data, size_t len)
  {
    Add(data, len);
  }

  void PrependUnless(const char* data, size_t len)
  {
    if (len > GetSize() || 0 != memcmp(GetBuffer(), data, len))
    {
      Prepend(data, len);
    }
  }

  void AppendUnless(const char* data, size_t data_len)
  {
    size_t len = GetSize();
    char* buf = GetBuffer();

    if (len < data_len || 0 != memcmp(buf + len - data_len, data, data_len))
    {
      Append(data, data_len);
    }
  }

  void ToForwardSlashes()
  {
    size_t len = GetSize();
    char* buf = GetBuffer();
    for (size_t i = 0; i < len; ++i)
    {
      if (buf[i] == '\\')
        buf[i] = '/';
    }
  }

  void ToBackSlashes()
  {
    size_t len = GetSize();
    char* buf = GetBuffer();
    for (size_t i = 0; i < len; ++i)
    {
      if (buf[i] == '/')
        buf[i] = '\\';
    }
  }

  void ToUppercase()
  {
    size_t len = GetSize();
    char* buf = GetBuffer();
    for (size_t i = 0; i < len; ++i)
    {
      buf[i] = (char) toupper(buf[i]);
    }
  }

  void ToLowercase()
  {
    size_t len = GetSize();
    char* buf = GetBuffer();
    for (size_t i = 0; i < len; ++i)
    {
      buf[i] = (char) tolower(buf[i]);
    }
  }

  void DropSuffix()
  {
    size_t len = GetSize();
    char* buf = GetBuffer();
    for (int i = (int) len - 1; i >= 0; --i)
    {
      if ('.' == buf[i])
      {
        Shrink(i);
        break;
      }
    }
  }

  void GetFilename()
  {
    abort();
  }

  void GetFilenameDir()
  {
    abort();
  }

  void EscapeForCmdlineDefine()
  {
    // Drop trailing backslash - can't be escaped on windows
    {
      size_t len = GetSize();
      char* buf = GetBuffer();
      if (len > 0 && buf[len - 1] == '\\')
        Shrink(len - 1);
    }

    size_t bs_count = 0;
    {
      size_t len = GetSize();
      char* buf = GetBuffer();

      for (size_t i = 0; i < len; ++i)
      {
        if (buf[i] == '\\')
          ++bs_count;
      }
    }

    if (bs_count > 0)
    {
      size_t orig_len = GetSize();
      Grow(bs_count);
      char* data = GetBuffer();
      memmove(data + bs_count, data, orig_len);

      size_t r = bs_count;
      size_t w = 0;
      while (r < orig_len)
      {
        char ch = data[r++];
        data[w++] = ch;

        if (ch == '\\')
        {
          data[w++] = '\\';
        }
      }
    }

    Prepend("\\\"", 2);
    Append("\\\"", 2);
  }

private:
  StringBuffer(const StringBuffer&);
  StringBuffer& operator=(const StringBuffer&);
};

static bool DoInterpolate(StringBuffer& output, const char* str, size_t len, LuaEnvLookup& lookup);

static void UnescapeOption(char* p)
{
  int r = 0;
  int w = 0;
  while (char ch = p[r++])
  {
    if ('\\' == ch)
    {
      if (p[r] == ':')
      {
        ++r;
        p[w++] = ':';
        continue;
      }
    }
    p[w++] = ch;
  }
  p[w++] = '\0';
}

static bool InterpolateVar(StringBuffer& output, const char* str, size_t len, LuaEnvLookup& lookup)
{
  StringBuffer var_name(output.GetHeap());

  if (!DoInterpolate(var_name, str, len, lookup))
    return false;

  var_name.NullTerminate();

  enum
  {
    kMaxOptions = 10
  };

  size_t      data_size                = var_name.GetSize() - 1;
  char       *data                     = var_name.GetBuffer();
  size_t      option_count             = 0;
  char       *option_ptrs[kMaxOptions];

  if (char *options = strchr(data, ':'))
  {
    data_size = options - data;

    while (nullptr != (options = strchr(options, ':')))
    {
      if (options[-1] == '\\')
      {
        ++options;
        continue;
      }

      if (option_count == kMaxOptions)
        return false;

      *options = '\0';
      option_ptrs[option_count++] = options + 1;
      ++options;
    }

    for (size_t i = 0; i < option_count; ++i)
    {
      UnescapeOption(option_ptrs[i]);
    }
  }

  LuaEnvLookupScope scope(lookup, data, data_size);
  if (!scope.Valid())
    return false;

  const char *join_string = " ";
  size_t      first_index = 0;
  size_t      max_index   = scope.GetCount();

  for (size_t oi = 0; oi < option_count; ++oi)
  {
    const char* option = option_ptrs[oi];
    switch (option[0])
    {
      case 'j':
        join_string = &option[1];
        break;
      case '[':
        first_index = atoi(&option[1]) - 1;
        max_index = first_index + 1;
        if (first_index >= scope.GetCount())
          return false;
        break;
    }
  }

  const size_t  join_len    = strlen(join_string);

  for (size_t i = first_index; i < max_index; ++i)
  {
    size_t item_len;
    const char* item_text = scope.GetValue(i, &item_len);
    if (!item_text)
      return false;

    StringBuffer item(output.GetHeap());

    if (!DoInterpolate(item, item_text, item_len, lookup))
      return false;

    for (size_t oi = 0; oi < option_count; ++oi)
    {
      const char* option = option_ptrs[oi];
      switch (option[0])
      {
        case 'p': item.Prepend(&option[1], strlen(&option[1])); break;
        case 'a': item.Append(&option[1], strlen(&option[1])); break;
        case 'P': item.PrependUnless(&option[1], strlen(&option[1])); break;
        case 'A': item.AppendUnless(&option[1], strlen(&option[1])); break;
        case 'f': item.ToForwardSlashes(); break;
        case 'b': item.ToBackSlashes(); break;
        case 'u': item.ToUppercase(); break;
        case 'l': item.ToLowercase(); break;
        case 'B': item.DropSuffix(); break;
        case 'F': item.GetFilename(); break;
        case 'D': item.GetFilenameDir(); break;
        case '#': item.EscapeForCmdlineDefine(); break;

        // Ignore these here
        case '[': case 'j':
          break;
        default:
          fprintf(stderr, "bad interpolation option: \"%s\"\n", option);
          return false;
      }
    }

    if (i > first_index)
      output.Add(join_string, join_len);

    output.Add(item.GetBuffer(), item.GetSize());
  }

  return true;
}

static const char* FindEndParen(const char* str, size_t len)
{
  int nesting = 1;
  for (size_t i = 0; i < len; ++i)
  {
    switch (str[i])
    {
      case '(':
        ++nesting;
        break;

      case ')':
        if (--nesting == 0)
          return str + i;
        break;
    }
  }
  return 0;
}

static bool DoInterpolate(StringBuffer& output, const char* str, size_t len, LuaEnvLookup& lookup)
{
  const char* end = str + len;

  while (str < end)
  {
    char ch = *str++;

    if ('$' == ch)
    {
      if (end != str && '(' == str[0])
      {
        ++str;
        const char* end_paren = FindEndParen(str, end - str);
        if (!end_paren)
        {
          fprintf(stderr, "unbalanced parens\n");
          return false;
        }
        if (!InterpolateVar(output, str, end_paren - str, lookup))
          return false;
        str = end_paren + 1;
        continue;
      }
    }

    output.Add(&ch, 1);
  }

  return true;
}

// Interpolate a string with respect to a set of lookup tables.
//
// Calling interface:
//  Arg 1 - String to interpolate
//  Arg 2 - A function to call to map a string to an table of expansions
static int LuaInterpolate(lua_State* L)
{
  size_t input_len;
  const char* input = luaL_checklstring(L, 1, &input_len);
  luaL_checktype(L, 2, LUA_TFUNCTION);

  {
    MemAllocHeap* heap;
    lua_getallocf(L, (void**) &heap);

    LuaEnvLookup lookup(L, 2);
    StringBuffer output(heap);

    if (DoInterpolate(output, input, input_len, lookup))
    {
      lua_pushlstring(L, output.GetBuffer(), output.GetSize());
      return 1;
    }
  }
  
  return luaL_error(L, "interpolation failed: %s", input);
}

void LuaEnvNativeOpen(lua_State* L)
{
  static luaL_Reg functions[] =
  {
    { "interpolate",                LuaInterpolate },
    { nullptr,                      nullptr },
  };

  luaL_register(L, "tundra.environment.native", functions);
  lua_pop(L, 1);
}

}
