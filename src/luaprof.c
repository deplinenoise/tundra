
#include <lua.h>
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
	TABLE_SIZE = 7919,
	MAX_CHILDREN = 32,
};

static const double s_to_ms = 1000.0;

struct loc_t;

typedef struct child_t {
	struct loc_t *child_loc;
	int call_count;
	double time;
	struct child_t *next;
} child_t;

typedef struct loc_t {
	const char *name;
	uint32_t hash;
	double total_time;
	int call_count;
	int child_count;
	child_t *child_list;
	struct loc_t *bucket_next;
} loc_t;

typedef struct arec_t {
	loc_t *loc;
	child_t *my_child_rec;
	double t1;
} arec_t;

static int loc_count;
static int ignored_returns;
static td_alloc alloc;
static int stack_depth;
static arec_t stack[MAX_DEPTH];
static loc_t* loc_table[TABLE_SIZE];
static loc_t* root_loc;

static child_t *
record_child(loc_t *parent, loc_t *child)
{
	child_t *chain = parent->child_list;
	while (chain)
	{
		if (chain->child_loc == child)
			break;
		else
			chain = chain->next;
	}

	if (!chain)
	{
		chain = td_page_alloc(&alloc, sizeof(child_t));
		chain->child_loc = child;
		chain->call_count = 0;
		chain->time = 0.0;
		chain->next = parent->child_list;
		parent->child_list = chain;
		++parent->child_count;
	}

	++chain->call_count;
	return chain;
}

static loc_t *
resolve_loc(const char *name)
{
	loc_t *chain;
	uint32_t hash;
	uint32_t bucket;

	hash = (uint32_t) djb2_hash(name);
	bucket = hash % TABLE_SIZE;

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
	chain->total_time = 0.0;
	chain->call_count = 0;
	chain->bucket_next = loc_table[bucket];
	chain->child_count = 0;
	chain->child_list = NULL;
	loc_table[bucket] = chain;
	++loc_count;
	return chain;
}

static void
on_call(lua_State *L, lua_Debug *debug)
{
	char location[512];
	int depth = stack_depth++;
	arec_t *rec;

	if (MAX_DEPTH == depth)
		td_croak("too deep call stack; limit is %d", MAX_DEPTH);

	rec = &stack[depth];

	if (0 == lua_getinfo(L, "Sn", debug))
		td_croak("couldn't look up debug info");

	snprintf(location, sizeof(location), "%s (%s:%d)",
			debug->name ? debug->name : "",
			debug->source, debug->linedefined);

	rec->loc = resolve_loc(location);
	rec->loc->call_count++;
	rec->t1 = td_timestamp();

	assert(depth > 0);
	rec->my_child_rec = record_child(stack[depth-1].loc, rec->loc);
}

static void
on_return(lua_State *L, lua_Debug *debug)
{
	if (stack_depth > 0)
	{
		int depth = --stack_depth;
		arec_t *rec = &stack[depth];
		double time = td_timestamp() - rec->t1;
		rec->loc->total_time += time;
		rec->my_child_rec->time += time;
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
	root_loc = resolve_loc("<root>");
	stack[0].loc = root_loc;
	stack[0].t1 = td_timestamp();
	stack_depth = 1;
	ignored_returns = 1;
	loc_count = 1;
	return 0;
}

static loc_t **
make_loc_array(void)
{
	int i, ni;
	loc_t **locs = td_page_alloc(&alloc, sizeof(loc_t *) * loc_count);

	for (i = 0, ni = 0; i < TABLE_SIZE; ++i)
	{
		loc_t *chain = loc_table[i];
		while (chain)
		{
			assert(ni < loc_count);
			locs[ni++] = chain;
			chain = chain->bucket_next;
		}
	}
	assert(ni == loc_count);
	return locs;
}

typedef int (*compare_fn)(const void *l, const void *r);

static int
cmp_total_time(const void *l, const void *r)
{
	double lt = (*(const loc_t **)l)->total_time;
	double rt = (*(const loc_t **)r)->total_time;

	if (lt < rt)
		return 1;
	else if (lt > rt)
		return -1;
	else
		return 0;
}

static double
self_time(const loc_t *loc)
{
	double t = loc->total_time;
	const child_t *chain = loc->child_list;
	while (chain)
	{
		t -= chain->time;
		chain = chain->next;
	}
	if (t < 0.0)
		t = 0.0;
	return t;
}

static int
cmp_self_time(const void *l, const void *r)
{
	double lt = self_time(*(const loc_t **)l);
	double rt = self_time(*(const loc_t **)r);

	if (lt < rt)
		return 1;
	else if (lt > rt)
		return -1;
	else
		return 0;
}

static void
report(loc_t *loc, compare_fn compare, int level)
{
	int count = printf("%s%s", td_indent(level), loc->name);
	printf("%s", td_spaces(60 - count));
	printf(" % 7d", loc->call_count);
	printf(" % 10.3fms", loc->total_time * s_to_ms);
	printf(" % 10.3fms", self_time(loc));
	if (loc->child_count)
		printf(" (%d children)", loc->child_count);

	if (level < 10)
	{
		printf("\n");
		if (loc->child_count)
		{
			int i;
			child_t *child;
			loc_t **children;

			children = alloca(sizeof(loc_t *) * loc->child_count);

			if (!children)
				td_croak("out of stack space");

			i = 0;
			for (child = loc->child_list; child; child = child->next)
				children[i++] = child->child_loc;

			qsort(children, loc->child_count, sizeof(loc_t *), compare);

			for (i = 0; i < loc->child_count; ++i)
				report(children[i], compare, level + 1);
		}
	}
	else
	{
		if (loc->child_count)
			printf(" ...");
		printf("\n");
	}
}

int td_luaprof_report(lua_State *L)
{
	/*int i;*/
	loc_t **locations;
	lua_sethook(L, NULL, 0, 0);

	locations = make_loc_array();
	qsort(locations, loc_count, sizeof(loc_t *), cmp_total_time);
	/*
	printf("top 20 functions by total time:\n");
	for (i = 0; i < 20 && i < node_count; ++i)
	{
		int off = printf("%s (%s)", nodes[i]->name, nodes[i]->kind);
		printf("%s%-5d%-.3fms\n", td_spaces(60-off), nodes[i]->call_count, s_to_ms * nodes[i]->total_time);
	}

	report(root_node, cmp_total_time, 0);
	*/
	report(root_loc, cmp_self_time, 0);

	td_alloc_cleanup(&alloc);
	return 0;
}
