#ifndef LUAPROFILER_HPP
#define LUAPROFILER_HPP

struct lua_State;

namespace t2
{
  struct MemAllocHeap;
  struct MemAllocLinear;

  void LuaProfilerInit(MemAllocHeap* heap, MemAllocLinear* alloc, lua_State* L);
  void LuaProfilerReport();
  void LuaProfilerDestroy();
}

#endif
