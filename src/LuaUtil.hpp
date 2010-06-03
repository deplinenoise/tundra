#ifndef TUNDRA_LUAUTIL_HPP
#define TUNDRA_LUAUTIL_HPP

#include "Portable.hpp"

struct lua_State;

namespace tundra
{

void DumpLuaStack(lua_State* L, int count, const char* label = 0);

bool CalculateDigest(void* file_handle, u8* digest, long size);

int PushMd5Digest(lua_State* L, const u8* digest);

}

int tundra_hasher_new(lua_State* L);
int tundra_hasher_add_string(lua_State* L);
int tundra_hasher_get_digest(lua_State* L);
int tundra_get_digest(lua_State* L);



#endif
