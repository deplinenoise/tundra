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
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include "util.h"
#include "portable.h"

#ifdef _WIN32
#include <malloc.h>
#define snprintf _snprintf
#endif

enum {
	MAX_DEPTH = 1024,
	LOC_TABLE_SIZE = 7919,
	INVOCATION_TABLE_SIZE = 7919,
	MAX_CHILDREN = 32,
};

static const double s_to_ms = 1000.0;

struct loc_t;

typedef struct invocation_t {
	uint32_t hash;
	int index;
	struct loc_t *location;
	struct invocation_t *parent;
	int call_count;
	double time;
	struct invocation_t *bucket_next;
} invocation_t;

typedef struct loc_t {
	const char *name;
	uint32_t hash;
	int index;
	struct loc_t *bucket_next;
} loc_t;

static int loc_count;
static int inv_count;
static int ignored_returns;
static td_alloc alloc;
static int stack_depth;
static invocation_t* invocation_table[LOC_TABLE_SIZE];
static loc_t* loc_table[LOC_TABLE_SIZE];
static loc_t* root_loc;

static struct {
	invocation_t *invocation;
	double start_time;
} call_stack[MAX_DEPTH];

static loc_t* location_stack[MAX_DEPTH];

static loc_t *
resolve_loc(const char *name)
{
	loc_t *chain;
	uint32_t hash;
	uint32_t bucket;

	hash = (uint32_t) djb2_hash(name);
	bucket = hash % LOC_TABLE_SIZE;

	chain = loc_table[bucket];
	while (chain)
	{
		if (chain->hash == hash && 0 == strcmp(chain->name, name))
			return chain;
		else
			chain = chain->bucket_next;
	}

	chain = td_page_alloc(&alloc, sizeof(loc_t));
	chain->name = td_page_strdup(&alloc, name, strlen(name));
	chain->hash = hash;
	chain->index = loc_count++;
	chain->bucket_next = loc_table[bucket];
	loc_table[bucket] = chain;
	return chain;
}

static invocation_t *
resolve_invocation(loc_t *location)
{
	int cur_depth = stack_depth;
	uint32_t hash = 0;
	uint32_t index;
	invocation_t *chain, *parent = NULL;

	if (cur_depth > 0)
	{
		parent = call_stack[cur_depth-1].invocation;
		hash = parent->hash;
	}

	hash ^= location->hash;
	index = hash % INVOCATION_TABLE_SIZE;

	chain = invocation_table[index];

	while (chain)
	{
		if (chain->hash == hash && chain->parent == parent && chain->location == location)
			return chain;

		chain = chain->bucket_next;
	}

	chain = td_page_alloc(&alloc, sizeof(invocation_t));
	chain->hash = hash;
	chain->location = location;
	chain->parent = parent;
	chain->call_count = 0;
	chain->time = 0.0;
	chain->index = inv_count++;
	chain->bucket_next = invocation_table[index];
	invocation_table[index] = chain;
	return chain;
}

static void
on_call(lua_State *L, lua_Debug *debug)
{
	char loc_name[512];
	int depth = stack_depth;
	loc_t *where;
	invocation_t *inv;

	if (MAX_DEPTH == depth)
		td_croak("too deep call stack; limit is %d", MAX_DEPTH);

	if (0 == lua_getinfo(L, "Sn", debug))
		td_croak("couldn't look up debug info");

	snprintf(loc_name, sizeof(loc_name), "%s, %s, %s, %d", debug->name ? debug->name : "", debug->namewhat ? debug->namewhat : "", debug->source, debug->linedefined);
	loc_name[sizeof(loc_name)-1] = '\0';

	where = resolve_loc(loc_name);

	inv = resolve_invocation(where);
	++inv->call_count;

	location_stack[depth] = where;
	call_stack[depth].invocation = inv;
	call_stack[depth].start_time = td_timestamp();

	++stack_depth;
}

static void
on_return(lua_State *L, lua_Debug *debug)
{
	if (stack_depth > 0)
	{
		int depth = --stack_depth;
		invocation_t *inv = call_stack[depth].invocation;
		double time = td_timestamp() - call_stack[depth].start_time;
		inv->time += time;
	}
}

static void
luaprof_hook(lua_State *L, lua_Debug *debug)
{
	switch (debug->event)
	{
		case LUA_HOOKCALL:
			on_call(L, debug);
			break;

		case LUA_HOOKRET:
		case LUA_HOOKTAILRET:
			if (0 == ignored_returns)
				on_return(L, debug);
			else
				--ignored_returns;
			break;
	}
}

int td_luaprof_install(lua_State *L)
{
	td_alloc_init(&alloc, 8, 1024 * 1024);
	lua_sethook(L, luaprof_hook, LUA_MASKCALL|LUA_MASKRET, 0);
	root_loc = resolve_loc("<root>, , , -1");
	call_stack[0].invocation = resolve_invocation(root_loc);
	call_stack[0].start_time = td_timestamp();
	call_stack[0].invocation->call_count = 1;
	location_stack[0] = root_loc;
	stack_depth = 1;
	ignored_returns = 1;
	return 0;
}

static void
dump_data(FILE* out)
{
	int i;
	fprintf(out, ".locations %d\n", loc_count);
	for (i = 0; i < LOC_TABLE_SIZE; ++i)
	{
		loc_t *chain = loc_table[i];
		while (chain)
		{
			fprintf(out, "%d %s\n", chain->index, chain->name);
			chain = chain->bucket_next;
		}
	}

	fprintf(out, ".invocations %d\n", inv_count);
	for (i = 0; i < INVOCATION_TABLE_SIZE; ++i)
	{
		invocation_t *chain = invocation_table[i];
		while (chain)
		{
			fprintf(out, "%d %d %d %d %.10f\n",
					chain->index,
					chain->parent ? chain->parent->index : -1,
					chain->location->index,
					chain->call_count,
					chain->time);
			chain = chain->bucket_next;
		}
	}
}

int td_luaprof_report(lua_State *L)
{
	const char *filename;
	FILE* out;

	if (lua_gettop(L) > 0)
	{
		filename = lua_tostring(L, 1);
		if (NULL == (out = fopen(filename, "w")))
			return luaL_error(L, "couldn't open \"%s\" for writing", filename);
	}
	else
		out = stdout;

	dump_data(out);

	if (stdout != out)
		fclose(out);

	lua_sethook(L, NULL, 0, 0);
	td_alloc_cleanup(&alloc);
	return 0;
}
