#include "LuaInterface.hpp"
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

int main(int argc, char* argv[])
{
  using namespace t2;

  InitCommon();

  MemAllocHeap heap;
  HeapInit(&heap, MB(256), HeapFlags::kDefault);

  const char* action = nullptr;
  const char* build_script = nullptr;

  if (argc >= 3)
  {
    action       = argv[1];
    build_script = argv[2];
  }

  if (!build_script || !action)
  {
    fprintf(stderr, "usage: t2-lua <action> <build-script>");
  }

  lua_State* L = CreateLuaState(&heap);
  bool success = RunBuildScript(L, action, build_script, (const char**) &argv[3], argc - 3);
    
  HeapDestroy(&heap);

	return success ? 0 : 1;
}

