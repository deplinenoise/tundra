
#include "LuaUtil.hpp"
#include "md5.h"

#include <lua.h>
#include <lauxlib.h>
#include <cstdio>
#include <cassert>
#include <algorithm>

void tundra::DumpLuaStack(lua_State* L, int count, const char* label)
{
	const int max_stack = lua_gettop(L);

	if (count > max_stack)
		count = max_stack;

	fprintf(stderr, "Lua stack (%d top elements) %s\n", count, label ? label : "");
	fprintf(stderr, "Pos Type       Value\n");

	for (int i=1; i<=count; ++i)
	{
		const int typecode = lua_type(L, -i);
		const char* const type = lua_typename(L, typecode);
		fprintf(stderr, "% 3d %-10s ", -i, type);

		if (LUA_TNUMBER == typecode)
		{
			fprintf(stderr, LUA_NUMBER_FMT "\n", lua_tonumber(L, -i));
		}
		else if (LUA_TTABLE == typecode)
		{
			int size = int(lua_objlen(L, -i));
			fprintf(stderr, "table(%d) { .. }\n", size);
		}
		else
		{
			fprintf(stderr, "<%s>\n", lua_tostring(L, -i));
		}
	}
}

bool tundra::CalculateDigest(void* f_, u8* digest, long size)
{
	FILE* const f = static_cast<FILE*>(f_);
	MD5_CTX context;
	unsigned char buffer[1024];

	MD5Init(&context);

	while (size > 0)
	{
		long read_size = std::min(size, long(sizeof(buffer)));
		read_size = (long) fread(&buffer[0], 1, read_size, f);
		MD5Update(&context, buffer, read_size);
		size -= read_size;
	}

	MD5Final(&digest[0], &context);
	return true;
}

int tundra::PushMd5Digest(lua_State* L, const u8* digest)
{
	luaL_Buffer buf;
	luaL_buffinit(L, &buf);
	for (int i=0; i<16; ++i)
	{
		static const char hextab[] = "0123456789abcdef";
		luaL_addchar(&buf, hextab[digest[i] >> 4]);
		luaL_addchar(&buf, hextab[digest[i] & 0xf]);
	}
	luaL_pushresult(&buf);
	return 1;
}

int tundra_hasher_new(lua_State* L)
{
	MD5_CTX* ctx = (MD5_CTX*) lua_newuserdata(L, sizeof(MD5_CTX));
	luaL_getmetatable(L, "tundra.hasher");
	assert(!lua_isnil(L, -1));
	lua_setmetatable(L, -2);
	MD5Init(ctx);
	return 1;
}

static MD5_CTX* check_hasher(lua_State* L, int arg)
{
	MD5_CTX* ctx = (MD5_CTX*) luaL_checkudata(L, arg, "tundra.hasher");
	luaL_argcheck(L, ctx != NULL, arg, "hasher object expected");
	return ctx;
}

int tundra_hasher_add_string(lua_State* L)
{
	MD5_CTX* ctx = check_hasher(L, 1);
	size_t len;
	const char* str = luaL_checklstring(L, 2, &len);
	MD5Update(ctx, (unsigned char*)(str), (unsigned int)len);
	return 0;
}

int tundra_hasher_get_digest(lua_State* L)
{
	MD5_CTX* ctx = check_hasher(L, 1);

	unsigned char digest[16];
	MD5Final(&digest[0], ctx);

	return tundra::PushMd5Digest(L, digest);
}

int tundra_get_digest(lua_State* L)
{
	MD5_CTX ctx;
	MD5Init(&ctx);
	for (int arg=1, arg_count=lua_gettop(L); arg <= arg_count; ++arg)
	{
		size_t len;
		const char* str = luaL_checklstring(L, arg, &len);
		MD5Update(&ctx, (unsigned char*)(str), (unsigned int)len);
	}
	unsigned char digest[16];
	MD5Final(&digest[0], &ctx);
	return tundra::PushMd5Digest(L, digest);
}

