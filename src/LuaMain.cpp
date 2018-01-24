#include "LuaInterface.hpp"
#include "LuaProfiler.hpp"
#include "MemAllocHeap.hpp"

extern "C"
{
#include "lua.h"
}

#include <stdio.h>

#if !defined(NDEBUG)
static void DumpValue(lua_State* L, int i, int max_depth)
{
	int type = lua_type(L, i);

	switch (type)
	{
		case LUA_TSTRING:
			printf("\"%s\"", lua_tostring(L, i));
			break;

		case LUA_TNUMBER:
			printf(LUA_NUMBER_FMT, lua_tonumber(L, i));
			break;

		case LUA_TTABLE:
			if (max_depth > 0)
			{
				int first = 1;
				printf("{ ");
				lua_pushnil(L);
				while (lua_next(L, i))
				{
					int top = lua_gettop(L);
					if (!first)
						printf(", ");
					DumpValue(L, top-1, max_depth-1);
					printf("=");
					DumpValue(L, top, max_depth-1);
					lua_pop(L, 1);
					first = 0;
				}
				printf("}");
			}
			else
			{
				printf("{...}");
			}
			break;

		case LUA_TUSERDATA:
			printf("<userdata>");
			break;

		case LUA_TNIL:
			printf("<nil>");
			break;

		default:
			printf("[%s]", lua_typename(L, type));
			break;
	}
}

void LuaDump(lua_State* L)
{
  int i, top;

  top = lua_gettop(L);
  printf("stack top: %d\n", top);

  for (i = 1; i <= top; ++i)
  {
    printf("#%d: ", i);
    DumpValue(L, i, 2);
    puts("");
  }
}
#endif

int main(int argc, char** argv)
{
  using namespace t2;

  InitCommon();

  MemAllocHeap heap;
  HeapInit(&heap);

  bool profile = false;

  int script_arg_start = 1;

  // Eat a secret --profile argument if it's first on the command line.
  if (argc > 1 && 0 == strcmp("--profile", argv[1]))
  {
    profile = true;
    ++script_arg_start;
  }

  lua_State* L = CreateLuaState(&heap, profile);

  bool success = RunBuildScript(L, (const char**) &argv[script_arg_start], argc - script_arg_start);

  if (profile)
    LuaProfilerReport();

  DestroyLuaState(L);

  HeapDestroy(&heap);

	return success ? 0 : 1;
}

