#include "LuaProfiler.hpp"
#include "Common.hpp"
#include "HashTable.hpp"
#include "MemAllocHeap.hpp"
#include "MemAllocLinear.hpp"

#include <stdio.h>

#if defined(_MSC_VER)
#define snprintf _snprintf
#endif

extern "C" {
#include <lua.h>
}

namespace t2
{

// Data about our idea of a Lua/C function.
// To save on heap space we try to store these uniquely in a hash table.
struct FunctionMeta
{
};

const char s_TopLevelString[] = "toplevel;global;;0";
static FunctionMeta s_TopLevel;

// Data about an invocation of a function.
//
// Because functions can be called in different contexts, it is important to
// worry about what callstack lead up to this invocation.
//
// For example, you might have three functions A, B and C:
//
// A calls C will yield one invocation for C with the stack [A, C]
// A calls B calls C will yield another invocation for C [A, B, C]
struct Invocation
{
  uint32_t        m_Hash;
  FunctionMeta*   m_Function;
  Invocation*     m_Parent;
  int32_t         m_CallCount;
  uint64_t        m_Ticks;
  uint64_t        m_StartTick;
  Invocation*     m_Next;
};

static struct LuaProfilerState
{
  lua_State*      m_LuaState;
  MemAllocHeap*   m_Heap;
  MemAllocLinear* m_Allocator;

  // We stash functions in here as we first encounter them.
  HashTable<FunctionMeta*, kFlagCaseSensitive> m_Functions;

  // A custom hash table of invocations
  uint32_t        m_InvocationCount;
  uint32_t        m_InvocationTableSize;
  Invocation**    m_InvocationTable;

  // The current call stack, as an invocation record.
  Invocation*     m_CurrentInvocation;
} s_Profiler;

static FunctionMeta* FindFunction(lua_State* L, lua_Debug* ar, uint32_t* hash_out)
{
  // This is slow, it involves string formatting. It's mostly OK, because we're
  // careful not to include this in the timings. It will of course affect cache
  // and other things. Not that we can make a lot of informed decisions about
  // that in an interpreted language anyway.
  if (!lua_getinfo(L, "Sn", ar))
    Croak("couldn't get debug info for function call");

  char buffer[1024];
  snprintf(buffer, sizeof(buffer), "%s;%s;%s;%d",
      ar->name ? ar->name : "", ar->namewhat ? ar->namewhat : "", ar->source, ar->linedefined);
  buffer[(sizeof buffer)-1] = 0;

  const uint32_t hash = Djb2Hash(buffer);
  *hash_out = hash;
  FunctionMeta** r = HashTableLookup(&s_Profiler.m_Functions, hash, buffer);

  if (!r) {
    FunctionMeta* meta = LinearAllocate<FunctionMeta>(s_Profiler.m_Allocator);
    HashTableInsert(&s_Profiler.m_Functions, hash, StrDup(s_Profiler.m_Allocator, buffer), meta);
    return meta;
  }
  
  return *r;
}

static void RehashInvocationTable(uint32_t new_size)
{
  const uint32_t old_size = s_Profiler.m_InvocationTableSize;

  Invocation** new_table = HeapAllocateArrayZeroed<Invocation*>(s_Profiler.m_Heap, new_size);

  for (uint32_t i = 0; i < old_size; ++i)
  {
    Invocation* chain = s_Profiler.m_InvocationTable[i];
    while (chain)
    {
      Invocation* next = chain->m_Next;

      uint32_t new_index = chain->m_Hash & (new_size - 1);
      chain->m_Next = new_table[new_index];
      new_table[new_index] = chain;

      chain = next;
    }
  }

  HeapFree(s_Profiler.m_Heap, s_Profiler.m_InvocationTable);

  // Commit.
  s_Profiler.m_InvocationTable     = new_table;
  s_Profiler.m_InvocationTableSize = new_size;
}

static bool MatchInvocation(Invocation* l, Invocation* r)
{
  while (l && l == r)
  {
    l = l->m_Parent;
    r = r->m_Parent;
  }

  return l == r;
}

static Invocation* PushInvocation(FunctionMeta* f, uint32_t hash)
{
  Invocation* parent = s_Profiler.m_CurrentInvocation;
  if (parent)
    hash ^= parent->m_Hash;

  uint32_t index = hash & (s_Profiler.m_InvocationTableSize - 1);

  Invocation* chain = s_Profiler.m_InvocationTable[index];
  while (chain)
  {
    if (chain->m_Hash == hash && chain->m_Function == f && MatchInvocation(chain->m_Parent, s_Profiler.m_CurrentInvocation))
    {
      return chain;
    }

    chain = chain->m_Next;
  }

  // Create a new invocation.

  // See if we need to rehash.
  if (double(s_Profiler.m_InvocationCount + 1) / double(s_Profiler.m_InvocationTableSize) > 0.75)
  {
    RehashInvocationTable(s_Profiler.m_InvocationTableSize * 2);
  }

  // Recompute insertion index.
  index = hash & (s_Profiler.m_InvocationTableSize - 1);

  // Allocate and populate invocation record.
  Invocation* i = LinearAllocate<Invocation>(s_Profiler.m_Allocator);
  i->m_Hash      = hash;
  i->m_Function  = f;
  i->m_Parent    = parent;
  i->m_CallCount = 0;
  i->m_Ticks     = 0;
  i->m_StartTick = 0;
  i->m_Next      = s_Profiler.m_InvocationTable[index];

  // Insert
  s_Profiler.m_InvocationTable[index] = i;

  return i;
}

static void ProfilerLuaEvent(lua_State* L, lua_Debug* ar)
{
  Invocation* i = s_Profiler.m_CurrentInvocation;

  // Update the time for this leaf, which will now go away.
  const uint64_t now = TimerGet();
  i->m_Ticks += now - i->m_StartTick;

  if (LUA_HOOKCALL == ar->event)
  {
    // We're calling a new function.
    // Figure out what function it is.

    uint32_t hash;
    FunctionMeta* f = FindFunction(L, ar, &hash);

    s_Profiler.m_CurrentInvocation = PushInvocation(f, hash);
    s_Profiler.m_CurrentInvocation->m_CallCount += 1;
  }
  else
  {
    // We're returning to our parent.
    s_Profiler.m_CurrentInvocation = i->m_Parent;
  }

  // Start counting from here on out. This is the last thing we do, so time
  // spent inside this hook will not be counted.
  s_Profiler.m_CurrentInvocation->m_StartTick = TimerGet();
}

void LuaProfilerInit(MemAllocHeap* heap, MemAllocLinear* alloc, lua_State* L)
{
  LuaProfilerState* self = &s_Profiler;

  memset(self, 0, sizeof *self);

  self->m_LuaState            = L;
  self->m_Heap                = heap;
  self->m_Allocator           = alloc;

  HashTableInit(&self->m_Functions, heap);

  RehashInvocationTable(1024);

  uint32_t toplevel_hash = Djb2Hash(s_TopLevelString);

  s_Profiler.m_CurrentInvocation = PushInvocation(&s_TopLevel, toplevel_hash);
  s_Profiler.m_CurrentInvocation->m_StartTick = TimerGet();

  // Install debug hook.
  lua_sethook(L, ProfilerLuaEvent, LUA_MASKCALL|LUA_MASKRET, 0);
}

void LuaProfilerDestroy()
{
  LuaProfilerState* self = &s_Profiler;

  lua_sethook(self->m_LuaState, nullptr, 0, 0);

  MemAllocHeap* heap = self->m_Heap;

  HeapFree(heap, self->m_InvocationTable);

  HashTableDestroy(&self->m_Functions);
}

static void DumpReport(FILE* f)
{
  fprintf(f, "Functions:\n");

  fprintf(f, " %p %s\n", &s_TopLevel, s_TopLevelString);

  HashTableWalk(&s_Profiler.m_Functions, [=](uint32_t, uint32_t, const char* str, FunctionMeta* p) -> void
  {
    fprintf(f, " %p %s\n", p, str);
  });

  fprintf(f, "Invocations:\n");

  for (uint32_t i = 0, max = s_Profiler.m_InvocationTableSize; i < max; ++i)
  {
    Invocation* chain = s_Profiler.m_InvocationTable[i];
    while (chain)
    {
      Invocation *stack = chain;
      while (stack)
      {
        fprintf(f, "%p%s", stack->m_Function, stack->m_Parent ? "<=" : "");
        stack = stack->m_Parent;
      }

      fprintf(f, ": calls=%d time=%.7f\n", chain->m_CallCount, TimerToSeconds(chain->m_Ticks));

      chain = chain->m_Next;
    }
  }
}

void LuaProfilerReport()
{
  printf("Generating profiling report..\n");

  if (FILE* f = fopen("tundra.prof", "w"))
  {
    DumpReport(f);

    fclose(f);
  }
  else
  {
    fprintf(stderr, "Failed to write tundra.prof\n");
  }
}

}
