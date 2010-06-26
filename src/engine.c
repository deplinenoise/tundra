#include "engine.h"

#include <lua.h>
#include <lauxlib.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <assert.h>

#include "debug.h"
#include "scanner.h"

enum
{
	TD_STRING_PAGE_SIZE = 1024*1024,
	TD_STRING_PAGE_MAX = 100,
	TD_PASS_MAX = 32
};

static unsigned long
djb2_hash(const char *str_)
{
	unsigned const char *str = (unsigned const char*) str_;
	unsigned long hash = 5381;
	int c;

	while (0 != (c = *str++))
	{
		hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
	}

	return hash;
}

static void
croak(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
}

struct td_file_tag
{
	const char *filename;
	td_node *producer;
	int (*sign_fn)(const char *filename);
	char signature[16];
	struct td_file_tag *bucket_next;
};

struct td_node_tag
{
	const char *annotation;
	const char *action;

	int input_count;
	td_file **inputs;

	int output_count;
	td_file **outputs;

	int pass_index;

	td_scanner* scanner;

	int dep_count;
	td_node **deps;
};

struct td_pass_tag
{
	const char *name;
	int build_order;
};

struct td_engine_tag
{
	int magic_value;

	/* memory allocation */
	int page_index;
	int page_left;
	char* pages[TD_STRING_PAGE_MAX];

	/* file db */
	int file_hash_size;
	td_file **file_hash;

	/* build passes */
	int pass_count;
	td_pass passes[TD_PASS_MAX];
};

void* td_engine_alloc(td_engine *engine, size_t size)
{
	int left = engine->page_left;
	int page = engine->page_index;
	char* addr;

	if (left < (int) size)
	{
		if (page == TD_STRING_PAGE_MAX)
			croak("out of string page memory");

		page = engine->page_index = page + 1;
		left = engine->page_left = TD_STRING_PAGE_SIZE;
		engine->pages[page] = malloc(TD_STRING_PAGE_SIZE);
		if (!engine->pages[page])
			croak("out of memory allocating string page");
	}

	addr = engine->pages[page] + TD_STRING_PAGE_SIZE - left;
	engine->page_left -= (int) size;
	return addr;
}

char *
td_engine_strdup(td_engine *engine, const char* str, size_t len)
{
	char *addr = (char*) td_engine_alloc(engine, len + 1);

	memcpy(addr, str, len);

	/* rather than copying len+1, explicitly store a nul byte so strdup can
	 * work for substrings too. */
	addr[len] = '\0';

	return addr;
}

td_file *td_engine_get_file(td_engine *engine, const char *path)
{
	unsigned long hash;
	int slot;
	td_file *chain;
	td_file *f;

	hash = djb2_hash(path);

	slot = (int) (hash % engine->file_hash_size);
	chain = engine->file_hash[slot];
	
	while (chain)
	{
		if (0 == strcmp(path, chain->filename))
			return chain;
	}

	f = td_engine_alloc(engine, sizeof(td_file));
	memset(f, 0, sizeof(td_file));

	f->filename = td_engine_strdup(engine, path, strlen(path));
	f->bucket_next = engine->file_hash[slot];
	engine->file_hash[slot] = f;
	return f;
}

static int get_int_override(lua_State *L, int index, const char *field_name, int default_value)
{
	int val = default_value;
	lua_getfield(L, index, field_name);
	if (!lua_isnil(L, -1))
	{
		if (lua_isnumber(L, -1))
			val = lua_tointeger(L, -1);
		else
			luaL_error(L, "%s: expected an integer, found %s", field_name, lua_typename(L, lua_type(L, -1)));
	}
	lua_pop(L, 1);
	return val;
}

static int make_engine(lua_State* L)
{
	td_engine* self = (td_engine*) lua_newuserdata(L, sizeof(td_engine));
	memset(self, 0, sizeof(td_engine));
	self->magic_value = 0xcafebabe;
	luaL_getmetatable(L, TUNDRA_ENGINE_MTNAME);
	lua_setmetatable(L, -2);

	self->file_hash_size = 92413;

	/* apply optional overrides */
	if (lua_gettop(L) == 1 && lua_istable(L, 1))
	{
		self->file_hash_size = get_int_override(L, 1, "FileHashSize", self->file_hash_size);
	}

	self->file_hash = (td_file **) calloc(sizeof(td_file*), self->file_hash_size);

	return 1;
}

static int engine_gc(lua_State* L)
{
	int p;
	td_engine * const self = td_check_engine(L, 1);

	if (self->magic_value != 0xcafebabe)
		luaL_error(L, "illegal userdatum; magic value check fails");

	for (p = self->page_index; p >= 0; --p)
	{
		char* page = self->pages[p];
		if (page)
		{
#ifndef NDEBUG
			memset(page, 0xdd, TD_STRING_PAGE_SIZE);
#endif
			free(page);
			self->pages[p] = NULL;
		}
	}

	self->magic_value = 0xdeadbeef;

	free(self->file_hash);
	self->file_hash = NULL;

	return 0;
}

char *
td_engine_strdup_lua(lua_State* L, td_engine *engine, int index, const char *context)
{
	const char *str;
	size_t len;
	str = lua_tolstring(L, index, &len);
	if (!str)
		luaL_error(L, "%s: expected a string", context);
	return td_engine_strdup(engine, str, len);
}

static int
get_pass_index(lua_State* L, td_engine* engine, int index)
{
	int build_order, i, e;
	size_t name_len;
	const char* name;

	lua_getfield(L, index, "BuildOrder");
	lua_getfield(L, index, "Name");

	name = lua_tolstring(L, -1, &name_len);

	if (lua_isnil(L, -2))
		luaL_error(L, "no build order set for pass %s", name);

	build_order = (int) lua_tointeger(L, -2);

	for (i = 0, e = engine->pass_count; i < e; ++i)
	{
		if (engine->passes[i].build_order == build_order)
		{
			if (0 == strcmp(name, engine->passes[i].name))
			{
				lua_pop(L, 2);
				return i;
			}
		}
	}

	if (TD_PASS_MAX == engine->pass_count)
		luaL_error(L, "too many passes adding pass %s", name);

	i = engine->pass_count++;

	engine->passes[i].name = td_engine_strdup(engine, name, name_len);
	engine->passes[i].build_order = build_order;

	lua_pop(L, 2);
	return i;
}

const char **
td_build_string_array(lua_State* L, td_engine *engine, int index, int *count_out)
{
	int i;
	const int count = (int) lua_objlen(L, index);
	const char **result;

	*count_out = count;
	if (!count)
		return NULL;
   
	result = (const char **) td_engine_alloc(engine, sizeof(const char*) * count);

	for (i = 0; i < count; ++i)
	{
		lua_rawgeti(L, index, i+1);
		result[i] = td_engine_strdup_lua(L, engine, -1, "string array");
		lua_pop(L, 1);
	}

	return result;
}

td_file **
td_build_file_array(lua_State* L, td_engine *engine, int index, int *count_out)
{
	int i;
	const int count = (int) lua_objlen(L, index);
	td_file **result;

	*count_out = count;
	if (!count)
		return NULL;
   
	result = (td_file **) td_engine_alloc(engine, sizeof(td_file*) * count);

	for (i = 0; i < count; ++i)
	{
		lua_rawgeti(L, index, i+1);
		result[i] = td_engine_get_file(engine, lua_tostring(L, -1));
		lua_pop(L, 1);
	}

	return result;
}

static const char*
copy_string_field(lua_State* L, td_engine *engine, int index, const char *field_name)
{
	const char* str;
	lua_getfield(L, index, field_name);
	str = td_engine_strdup_lua(L, engine, -1, field_name);
	lua_pop(L, 1);
	return str;
}

static int
setup_pass(lua_State* L, td_engine* engine, int index)
{
	int pass_index;

	lua_getfield(L, index, "pass");
	if (lua_isnil(L, -1))
		luaL_error(L, "no pass specified");

	pass_index = get_pass_index(L, engine, lua_gettop(L));
	lua_pop(L, 1);

	return pass_index;
}

static void
check_input_files(lua_State* L, td_engine *engine, td_node *node)
{
	int i, e;
	const int my_build_order = engine->passes[node->pass_index].build_order;

	for (i = 0, e=node->input_count; i < e; ++i)
	{
		td_file *f = node->inputs[i];
		td_node *producer = f->producer;
		if (producer)
		{
			td_pass* his_pass = &engine->passes[producer->pass_index];
			if (his_pass->build_order > my_build_order)
			{
				luaL_error(L, "%s: file %s is produced in future pass %s (by %s)",
						node->annotation, f->filename, his_pass->name,
						f->producer->annotation);
			}
		}
	}
}

static void
tag_output_files(lua_State* L, td_node *node)
{
	int i, e;
	for (i = 0, e=node->output_count; i < e; ++i)
	{
		td_file *f = node->outputs[i];
		if (f->producer)
		{
			luaL_error(L, "%s: file %s is already an output of %s",
					node->annotation, f->filename, f->producer->annotation);
		}
		f->producer = node;
	}
}

static int
compare_ptrs(const void *l, const void *r)
{
	const td_node *lhs = *(const td_node * const *) l;
	const td_node *rhs = *(const td_node * const *) r;
	return lhs - rhs;
	if (lhs < rhs)
		return -1;
	else if (lhs > rhs)
		return 1;
	else
		return 0;
}

static int
uniqize_deps(td_node *const *source, int count, td_node **dest)
{
	int i, unique_count = 0;
	td_node *current = NULL;
	for (i = 0; i < count; ++i)
	{
		td_node *candidate = source[i];
		if (current != candidate)
		{
			dest[unique_count++] = candidate;
			current = candidate;
		}
	}

	assert(unique_count <= count);
	return unique_count;
}

static td_node**
setup_deps(lua_State* L, td_engine *engine, td_node *node, int *count_out)
{
	int i, e;
	int count = 0, result_count = 0;
	int max_deps = 0, dep_array_size = 0;
	td_node **deps = NULL, **uniq_deps = NULL, **result = NULL;

	/* compute and allocate worst case space for dependencies */
	lua_getfield(L, 2, "deps");
	dep_array_size = max_deps = lua_objlen(L, -1);

	max_deps += node->input_count;

	if (0 == max_deps)
		goto leave;

	deps = (td_node **) alloca(max_deps * sizeof(td_node*));

	/* gather deps */
	if (lua_istable(L, -1))
	{
		for (i = 1, e = dep_array_size; i <= e; ++i)
		{
			lua_rawgeti(L, -1, i);
			deps[count++] = td_check_node(L, -1);
			lua_pop(L, 1);
		}
	}

	for (i = 0, e = node->input_count; i < e; ++i)
	{
		td_node *producer = node->inputs[i]->producer;
		if (producer)
			deps[count++] = producer;
	}

	if (0 == count)
		goto leave;

	/* sort the dependency set to easily remove duplicates */
	qsort(deps, count, sizeof(td_node*), compare_ptrs);

	/* allocate a new scratch set to write the unique deps into */
	uniq_deps = (td_node **) alloca(count * sizeof(td_node*));

	/* filter deps into uniq_deps by merging adjecent duplicates */
	result_count = uniqize_deps(deps, count, uniq_deps);

	/* allocate and fill final result array as a copy of uniq_deps */
	result = td_engine_alloc(engine, sizeof(td_node*) * result_count);
	memcpy(result, uniq_deps, sizeof(td_node*) * result_count);

leave:
	*count_out = result_count;
	lua_pop(L, 1);
	return result;
}

static int
make_node(lua_State* L)
{
	td_engine * const self = td_check_engine(L, 1);
	td_node *node = (td_node*) lua_newuserdata(L, sizeof(td_node));

	node->annotation = copy_string_field(L, self, 2, "annotation");
	node->action = copy_string_field(L, self, 2, "action");
	node->pass_index = setup_pass(L, self, 2);

	lua_getfield(L, 2, "inputs");
	node->inputs = td_build_file_array(L, self, lua_gettop(L), &node->input_count);
	lua_pop(L, 1);
	check_input_files(L, self, node);

	lua_getfield(L, 2, "outputs");
	node->outputs = td_build_file_array(L, self, lua_gettop(L), &node->output_count);
	lua_pop(L, 1);
	tag_output_files(L, node);

	lua_getfield(L, 2, "scanner");
	if (!lua_isnil(L, -1))
		node->scanner = td_check_scanner(L, -1);
	lua_pop(L, 1);

	node->deps = setup_deps(L, self, node, &node->dep_count);

	luaL_getmetatable(L, TUNDRA_NODE_MTNAME);
	lua_setmetatable(L, -2);
	return 1;
}

/*
 * Filter a list of files by extensions, appending matches to a lua array table.
 *
 * file_count - #files in array
 * files - filenames to filter
 * lua arg 1 - self (a node userdata)
 * lua arg 2 - array table to append results to
 * lua arg 3 - array table of extensions
 */
#define TD_MAX_EXTS 4 
#define TD_EXTLEN 16 

static int
insert_file_list(lua_State *L, int file_count, td_file **files)
{
	int i, table_size;
	const int ext_count = (int) lua_objlen(L, 3);
	char exts[TD_MAX_EXTS][TD_EXTLEN] = { { 0 } };

	if (ext_count > TD_MAX_EXTS)
		luaL_error(L, "only %d extensions supported; %d is too many", TD_MAX_EXTS, ext_count);

	/* construct a lookup table of all extensions on the stack for speedy access */
	for (i = 0; i < ext_count; ++i)
	{
		lua_rawgeti(L, 3, i+1);
		strncpy(exts[i], lua_tostring(L, -1), TD_EXTLEN);
		exts[i][TD_EXTLEN-1] = '\0';
	}

	table_size = (int) lua_objlen(L, 2);
	for (i = 0; i < file_count; ++i)
	{
		int x;
		const char* ext_pos;
		const char* fn = files[i]->filename;

		ext_pos = strrchr(fn, '.');
		if (!ext_pos)
			ext_pos = "";

		for (x = 0; x < ext_count; ++x)
		{
			if (0 == strcmp(ext_pos, exts[x]))
			{
				lua_pushstring(L, fn);
				lua_rawseti(L, 2, ++table_size);
				break;
			}
		}
	}
	return 0;
}

static int
insert_input_files(lua_State* L)
{
	td_node * const self = td_check_node(L, 1);
	return insert_file_list(L, self->input_count, self->inputs);
}

static int
insert_output_files(lua_State* L)
{
	td_node * const self = td_check_node(L, 1);
	return insert_file_list(L, self->output_count, self->outputs);
}

static void dump_node(const td_node *n)
{
	int x;
	printf("annotation: %s\n", n->annotation);
	printf("action: %s\n", n->action);

	for (x = 0; x < n->input_count; ++x)
		printf("input(%d): %s\n", x+1, n->inputs[x]->filename);

	for (x = 0; x < n->output_count; ++x)
		printf("output(%d): %s\n", x+1, n->outputs[x]->filename);

	for (x = 0; x < n->dep_count; ++x)
		printf("dep(%d): %s\n", x+1, n->deps[x]->annotation);
}

/*
 * Execute actions needed to update a dependency graph.
 *
 * Input:
 * A list of dag nodes to build.
 */
static int
build_nodes(lua_State* L)
{
	int i, narg;
	td_engine * const self = td_check_engine(L, 1);
	(void) self;

	narg = lua_gettop(L);

	for (i=2; i<=narg; ++i)
	{
		td_node *node = (td_node*) luaL_checkudata(L, i, TUNDRA_NODE_MTNAME);
		dump_node(node);
	}

	return 0;
}

static int
is_node(lua_State* L)
{
	int status = 0;
	if (lua_getmetatable(L, 1))
	{
		lua_getfield(L, LUA_REGISTRYINDEX, TUNDRA_NODE_MTNAME);
		status = lua_rawequal(L, -1, -2);
	}
	lua_pushboolean(L, status);
	return 1;
}

static int
node_str(lua_State *L)
{
	lua_pushlstring(L, "{", 1);
	lua_pushstring(L, td_check_node(L, 1)->annotation);
	lua_pushlstring(L, "}", 1);
	lua_concat(L, 3);
	return 1;
}

static const luaL_Reg engine_mt_entries[] = {
	{ "make_node", make_node },
	{ "build", build_nodes },
	{ "__gc", engine_gc },
	{ NULL, NULL }
};

static const luaL_Reg node_mt_entries[] = {
	{ "insert_input_files", insert_input_files },
	{ "insert_output_files", insert_output_files },
	{ "__tostring", node_str },
	{ NULL, NULL }
};

static const luaL_Reg engine_entries[] = {
	{ "make_engine", make_engine },
	{ "is_node", is_node },
	{ NULL, NULL }
};

static void create_mt(lua_State* L, const char *name, const luaL_Reg entries[])
{
	luaL_newmetatable(L, name);
	lua_pushvalue(L, -1);
	lua_setfield(L, -2, "__index");
	luaL_register(L, NULL, entries);
	lua_pop(L, 1);
}

void td_engine_open(lua_State* L)
{
	/* use the table passed in to add functions defined here */
	luaL_register(L, NULL, engine_entries);

	/* set up engine and node object metatable */
	create_mt(L, TUNDRA_ENGINE_MTNAME, engine_mt_entries);
	create_mt(L, TUNDRA_NODE_MTNAME, node_mt_entries);
}
