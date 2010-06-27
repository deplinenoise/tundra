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
#include "util.h"
#include "build.h"

#ifdef _MSC_VER
#include <malloc.h> /* alloca */
#endif

int td_sign_timestamp(const char *filename, char digest_out[16])
{
	return 1;
}

int td_sign_digest(const char *filename, char digest_out[16])
{
	return 1;
}

static td_signer sign_timestamp = { 0, { td_sign_timestamp } };
static td_signer sign_digest = { 0, { td_sign_digest } };

void *td_engine_alloc(td_engine *engine, size_t size)
{
	int left = engine->page_left;
	int page = engine->page_index;
	char *addr;

	if (left < (int) size)
	{
		if (page == TD_STRING_PAGE_MAX)
			td_croak("out of string page memory");

		page = engine->page_index = page + 1;
		left = engine->page_left = TD_STRING_PAGE_SIZE;
		engine->pages[page] = malloc(TD_STRING_PAGE_SIZE);
		if (!engine->pages[page])
			td_croak("out of memory allocating string page");
	}

	addr = engine->pages[page] + TD_STRING_PAGE_SIZE - left;
	engine->page_left -= (int) size;

#ifndef NDEBUG
	memset(addr, 0xcc, size);
#endif

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
	f->signer = engine->default_signer;
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
			val = (int) lua_tointeger(L, -1);
		else
			luaL_error(L, "%s: expected an integer, found %s", field_name, lua_typename(L, lua_type(L, -1)));
	}
	lua_pop(L, 1);
	return val;
}

static int make_engine(lua_State *L)
{
	td_engine *self = (td_engine*) lua_newuserdata(L, sizeof(td_engine));
	memset(self, 0, sizeof(td_engine));
	self->magic_value = 0xcafebabe;
	luaL_getmetatable(L, TUNDRA_ENGINE_MTNAME);
	lua_setmetatable(L, -2);

	self->file_hash_size = 92413;
	self->L = L;

	/* apply optional overrides */
	if (lua_gettop(L) == 1 && lua_istable(L, 1))
	{
		self->file_hash_size = get_int_override(L, 1, "FileHashSize", self->file_hash_size);
	}

	self->file_hash = (td_file **) calloc(sizeof(td_file*), self->file_hash_size);
	self->default_signer = &sign_digest;
	self->node_count = 0;

	return 1;
}

static int engine_gc(lua_State *L)
{
	int p;
	td_engine *const self = td_check_engine(L, 1);

	if (self->magic_value != 0xcafebabe)
		luaL_error(L, "illegal userdatum; magic value check fails");

	for (p = self->page_index; p >= 0; --p)
	{
		char *page = self->pages[p];
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

static int
get_pass_index(lua_State *L, td_engine *engine, int index)
{
	int build_order, i, e;
	size_t name_len;
	const char *name;

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

static const char*
copy_string_field(lua_State *L, td_engine *engine, int index, const char *field_name)
{
	const char* str;
	lua_getfield(L, index, field_name);
	if (!lua_isstring(L, -1))
		luaL_error(L, "%s: expected a string", field_name);
	str = td_engine_strdup_lua(L, engine, -1, field_name);
	lua_pop(L, 1);
	return str;
}

static int
setup_pass(lua_State *L, td_engine *engine, int index)
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
check_input_files(lua_State *L, td_engine *engine, td_node *node)
{
	int i, e;
	const int my_build_order = engine->passes[node->pass_index].build_order;

	for (i = 0, e=node->input_count; i < e; ++i)
	{
		td_file *f = node->inputs[i];
		td_node *producer = f->producer;
		if (producer)
		{
			td_pass *his_pass = &engine->passes[producer->pass_index];
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
tag_output_files(lua_State *L, td_node *node)
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
setup_deps(lua_State *L, td_engine *engine, td_node *node, int *count_out)
{
	int i, e;
	int count = 0, result_count = 0;
	int max_deps = 0, dep_array_size = 0;
	td_node **deps = NULL, **uniq_deps = NULL, **result = NULL;

	/* compute and allocate worst case space for dependencies */
	lua_getfield(L, 2, "deps");
	dep_array_size = max_deps = (int) lua_objlen(L, -1);

	max_deps += node->input_count;

	if (0 == max_deps)
		goto leave;

	deps = (td_node **) alloca(max_deps * sizeof(td_node*));
	if (!deps)
		luaL_error(L, "out of stack memory allocating %d bytes", max_deps * sizeof(td_node*));

	/* gather deps */
	if (lua_istable(L, -1))
	{
		for (i = 1, e = dep_array_size; i <= e; ++i)
		{
			lua_rawgeti(L, -1, i);
			deps[count++] = td_check_noderef(L, -1)->node;
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

	if (!uniq_deps)
		luaL_error(L, "out of stack memory allocating %d bytes", count * sizeof(td_node*));

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

static void
setup_file_signers(lua_State *L, td_engine *engine, td_node *node)
{
	lua_getfield(L, 2, "signers");
	if (lua_isnil(L, -1))
		goto leave;

	lua_pushnil(L);
	while (lua_next(L, -2))
	{
		const char *filename;
		td_signer *signer = NULL;
		td_file *file;

		if (!lua_isstring(L, -2))
			luaL_error(L, "file signer keys must be strings");

		filename = lua_tostring(L, -2);

		if (lua_isstring(L, -1))
		{
			const char *builtin_name = lua_tostring(L, -1);
			if (0 == strcmp("digest", builtin_name))
				signer = &sign_digest;
			else if (0 == strcmp("timestamp", builtin_name))
				signer = &sign_timestamp;
			else
				luaL_error(L, "%s: unsupported builtin sign function", builtin_name);

			lua_pop(L, 1);
		}
		else if(lua_isfunction(L, -1))
		{
			/* save the lua closure in the registry so we can call it later */
			signer = (td_signer*) td_engine_alloc(engine, sizeof(td_signer));
			signer->is_lua = 1;
			signer->function.lua_reference = luaL_ref(L, -1); /* pops the value */
		}
		else
		{
			luaL_error(L, "signers must be either builtins (strings) or functions");
		}

		file = td_engine_get_file(engine, filename);

		if (file->producer != node)
			luaL_error(L, "%s isn't produced by this node; can't sign it", filename);

		file->signer = signer;
	}

leave:
	lua_pop(L, 1);
}

static int
make_node(lua_State *L)
{
	td_engine * const self = td_check_engine(L, 1);
	td_node *node = (td_node *) td_engine_alloc(self, sizeof(td_node));

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
	else
		node->scanner = NULL;
	lua_pop(L, 1);

	node->deps = setup_deps(L, self, node, &node->dep_count);

	setup_file_signers(L, self, node);

	memset(&node->job, 0, sizeof(node->job));

	td_noderef *noderef = (td_noderef*) lua_newuserdata(L, sizeof(td_noderef));
	noderef->node = node;
	luaL_getmetatable(L, TUNDRA_NODEREF_MTNAME);
	lua_setmetatable(L, -2);

	++self->node_count;

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
		const char *ext_pos;
		const char *fn = files[i]->filename;

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
insert_input_files(lua_State *L)
{
	td_node *const self = td_check_noderef(L, 1)->node;
	return insert_file_list(L, self->input_count, self->inputs);
}

static int
insert_output_files(lua_State *L)
{
	td_node *const self = td_check_noderef(L, 1)->node;
	return insert_file_list(L, self->output_count, self->outputs);
}

static void
add_pending_job(td_engine *engine, td_node *blocking_node, td_node *blocked_node)
{
	td_job_chain *chain;

	chain = blocking_node->job.pending_jobs;
	while (chain)
	{
		if (chain->node == blocked_node)
			return;
		chain = chain->next;
	}

	chain = (td_job_chain *) td_engine_alloc(engine, sizeof(td_job_chain));
	chain->node = blocked_node;
	chain->next = blocking_node->job.pending_jobs;
	blocking_node->job.pending_jobs = chain;
	blocked_node->job.block_count++;
}

static void
assign_jobs(td_engine *engine, td_node *root_node)
{
	int i, dep_count;
	td_node **deplist = root_node->deps;

	dep_count = root_node->dep_count;

	for (i = 0; i < dep_count; ++i)
	{
		td_node *dep = deplist[i];
		add_pending_job(engine, dep, root_node);
	}

	for (i = 0; i < dep_count; ++i)
	{
		td_node *dep = deplist[i];
		assign_jobs(engine, dep);
	}
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

	narg = lua_gettop(L);

	for (i = 2; i <= narg; ++i)
	{
		td_noderef *nref = (td_noderef *) luaL_checkudata(L, i, TUNDRA_NODEREF_MTNAME);
		assign_jobs(self, nref->node);
		/*td_dump_node(nref->node, 0, -1);*/
		td_build(self, nref->node, 1);
	}

	return 0;
}

static int
is_node(lua_State *L)
{
	int status = 0;
	if (lua_getmetatable(L, 1))
	{
		lua_getfield(L, LUA_REGISTRYINDEX, TUNDRA_NODEREF_MTNAME);
		status = lua_rawequal(L, -1, -2);
	}
	lua_pushboolean(L, status);
	return 1;
}

static int
node_str(lua_State *L)
{
	lua_pushlstring(L, "{", 1);
	lua_pushstring(L, td_check_noderef(L, 1)->node->annotation);
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

static void create_mt(lua_State *L, const char *name, const luaL_Reg entries[])
{
	luaL_newmetatable(L, name);
	lua_pushvalue(L, -1);
	lua_setfield(L, -2, "__index");
	luaL_register(L, NULL, entries);
	lua_pop(L, 1);
}

void td_engine_open(lua_State *L)
{
	/* use the table passed in to add functions defined here */
	luaL_register(L, NULL, engine_entries);

	/* set up engine and node object metatable */
	create_mt(L, TUNDRA_ENGINE_MTNAME, engine_mt_entries);
	create_mt(L, TUNDRA_NODEREF_MTNAME, node_mt_entries);
}
