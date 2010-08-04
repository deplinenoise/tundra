/*
   Copyright 2010 Andreas Fredriksson

   This file is part of Tundra.

   Tundra is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Tundra is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Tundra.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <lua.h>
#include <lauxlib.h>
#include <stdio.h>

#include "util.h"
#include "engine.h"

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

void
td_dump_node(const td_node *n, int level, int outer_index)
{
	int x;
	const char *indent = td_indent(level);

	if (outer_index >= 0)
		printf("%s%d: {\n", indent, outer_index);
	else
		printf("%s {\n", indent);

	printf("%s  annotation: %s\n", indent, n->annotation);
	printf("%s  action: %s\n", indent, n->action);

	for (x = 0; x < n->input_count; ++x)
		printf("%s  input(%d): %s\n", indent, x+1, n->inputs[x]->path);

	for (x = 0; x < n->output_count; ++x)
		printf("%s  output(%d): %s\n", indent, x+1, n->outputs[x]->path);

	if (n->dep_count)
	{
  		printf("%s  deps:\n", indent);
		for (x = 0; x < n->dep_count; ++x)
		{
			td_dump_node(n->deps[x], level+1, x);
		}
	}
	printf("%s}\n", indent);
}

