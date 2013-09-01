#ifndef LUAINTERFACE_HPP
#define LUAINTERFACE_HPP

struct lua_State;

namespace t2
{

struct MemAllocHeap;

lua_State* CreateLuaState(MemAllocHeap* heap, bool profile);

void DestroyLuaState(lua_State* L);

bool RunBuildScript(lua_State *L, const char** args, int argc_count);

}

#endif
