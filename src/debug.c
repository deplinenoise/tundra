#include <lua.h>
#include <lauxlib.h>
#include <stdio.h>

static void dumpvalue(lua_State* L, int i, int max_depth)
{
	int type = lua_type(L, i);

	switch (type)
	{
		case LUA_TSTRING:
			printf("\"%s\"", lua_tostring(L, i));
			break;

		case LUA_TNUMBER:
			printf("%g", (double) lua_tonumber(L, i));
			break;

		case LUA_TTABLE:
			{
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
						dumpvalue(L, top-1, max_depth-1);
						printf("=");
						dumpvalue(L, top, max_depth-1);
						lua_pop(L, 1);
						first = 0;
					}
					printf("}");
				}
				else
				{
					printf("{...}");
				}
			}
			break;

		default:
			printf("{%s}", lua_typename(L, type));
			break;
	}
}

void td_debug_dump(lua_State* L)
{
	int i, top;

	top = lua_gettop(L);
	printf("stack top: %d\n", top);

	for (i = 1; i <= top; ++i)
	{
		printf("#%d: ", i);
		dumpvalue(L, i, 2);
		puts("");
	}
}

