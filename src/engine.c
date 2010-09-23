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

#include "engine.h"
#include "portable.h"
#include "md5.h"
#include "relcache.h"
#include "scanner.h"
#include "util.h"
#include "ancestors.h"
#include "files.h"
#include "config.h"

#include <lua.h>
#include <lauxlib.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <assert.h>

#if defined(TUNDRA_WIN32)
#include <malloc.h> /* alloca */
#endif

#if defined(_MSC_VER)
#define snprintf _snprintf
#endif

char td_scanner_hook_key;
char td_node_hook_key;
char td_dirwalk_hook_key;

void td_sign_timestamp(td_engine *engine, td_file *f, td_digest *out)
{
	int zero_size;
	const td_stat *stat;

	stat = td_stat_file(engine, f);

	zero_size = sizeof(out->data) - sizeof(stat->timestamp);

	memcpy(&out->data[0], &stat->timestamp, sizeof(stat->timestamp));
	memset(&out->data[sizeof(stat->timestamp)], 0, zero_size);
}

void td_sign_digest(td_engine *engine, td_file *file, td_digest *out)
{
	FILE* f;

	if (NULL != (f = fopen(file->path, "rb")))
	{
		unsigned char buffer[8192];
		MD5_CTX md5;
		int read_count;

		MD5_Init(&md5);

		do {
			read_count = (int) fread(buffer, 1, sizeof(buffer), f);
			MD5_Update(&md5, buffer, read_count);
		} while (read_count > 0);

		fclose(f);

		MD5_Final(out->data, &md5);
	}
	else
	{
		fprintf(stderr, "warning: couldn't open %s for signing\n", file->path);
		memset(out->data, 0, sizeof(out->data));
	}
}

static td_signer sign_timestamp = { 0, { td_sign_timestamp } };
static td_signer sign_digest = { 0, { td_sign_digest } };

int td_compare_ancestors(const void* l_, const void* r_)
{
	const td_ancestor_data *l = (const td_ancestor_data *) l_, *r = (const td_ancestor_data *) r_;
	return memcmp(l->guid.data, r->guid.data, sizeof(l->guid.data));
}

static const char*
find_basename(const char *path, int path_len)
{
	int i;

	/* find the filename part of the path */
	for (i = path_len; i >= 0; --i)
	{
		char ch = path[i];
		if (TD_PATHSEP == ch)
		{
			return &path[i+1];
		}
	}

	return path;
}

typedef struct
{
	char *ptr;
	int len;
	int dotdot;
	int drop;
} strseg;

static int
tokenize_path(char* scratch, strseg segments[], int maxseg)
{
	int segcount = 0;
	char *last = scratch;

	for (;;)
	{
		char ch = *scratch;
		if ('\\' == ch || '/' == ch || '\0' == ch)
		{
			int len = (int) (scratch - last);
			int is_dotdot = 2 == len && 0 == memcmp("..", last, 2);
			int is_dot = 1 == len && '.' == last[0];

			if (segcount == maxseg)
				td_croak("too many segments in path; limit is %d", maxseg);

			segments[segcount].ptr = last;
			segments[segcount].len = len;
			segments[segcount].dotdot = is_dotdot;
			segments[segcount].drop = is_dot;

			last = scratch + 1;
			++segcount;

			if ('\0' == ch)
				break;
		}

		++scratch;
	}

	return segcount;
}

static void
sanitize_path(char *buffer, size_t buffer_size, size_t input_length)
{
	char scratch[512];
	int i;
	int segcount;
	int dotdot_drops = 0;
	strseg segments[64];

	strcpy(scratch, buffer);
	segcount = tokenize_path(scratch, segments, sizeof(segments) / sizeof(segments[0]));

	for (i = segcount - 1; i >= 0; --i)
	{
		if (segments[i].drop)
			continue;

		if (segments[i].dotdot)
		{
			segments[i].drop = 1;
			++dotdot_drops;
		}
		else if (dotdot_drops > 0)
		{
			--dotdot_drops;
			segments[i].drop = 1;
		}
	}

	/* Format the resulting path. It can never get longer by this operation, so
	 * there's no need to check the buffer size. */
	{
		int first = 1;
		char *cursor = buffer;

		/* Emit all leading ".." tokens we've got left */
		for (i = 0; i < dotdot_drops; ++i)
		{
			memcpy(cursor, ".." TD_PATHSEP_STR, 3);
			cursor += 3;
		}

		/* Emit all remaining tokens. */
		for (i = 0; i < segcount; ++i)
		{
			int len = segments[i].len;
			const char *seg = segments[i].ptr;

			if (segments[i].drop)
				continue;

			if (!first)
				*cursor++ = TD_PATHSEP;
			first = 0;
			memcpy(cursor, seg, len);
			cursor += len;
		}
		*cursor = 0;
	}
}

int td_sanitize_lua_path(lua_State *L)
{
	char buffer[512];
	size_t path_len;
	const char *path = luaL_checklstring(L, 1, &path_len);
	if (path_len >= sizeof(buffer))
		return luaL_error(L, "path too long; %d > %d", (int) path_len, (int) sizeof(buffer));
	strcpy(buffer, path);
	sanitize_path(buffer, sizeof(buffer), path_len);
	lua_pushstring(L, buffer);
	return 1;
}

td_file *
td_engine_get_file(td_engine *engine, const char *input_path, td_get_file_mode mode)
{
	unsigned int hash;
	int slot;
	td_file *chain;
	td_file *f;
	int path_len;
	char path[512];
	const char *out_path;
	size_t input_path_len = strlen(input_path);

	if (input_path_len >= sizeof(path))
		td_croak("path too long: %s", input_path);

	if (TD_COPY_STRING == mode)
	{
		strcpy(path, input_path);
		sanitize_path(path, sizeof(path), input_path_len);
		hash = (unsigned int) djb2_hash(path);
		out_path = path;
	}
	else
	{
		hash = (unsigned int) djb2_hash(input_path);
		out_path = input_path;
	}

	td_mutex_lock_or_die(engine->lock);

	slot = (int) (hash % engine->file_hash_size);
	chain = engine->file_hash[slot];
	
	while (chain)
	{
		if (chain->hash == hash && 0 == strcmp(out_path, chain->path))
		{
			td_mutex_unlock_or_die(engine->lock);
			return chain;
		}

		chain = chain->bucket_next;
	}

	++engine->stats.file_count;
	f = td_page_alloc(&engine->alloc, sizeof(td_file));
	memset(f, 0, sizeof(td_file));

	f->path_len = path_len = (int) strlen(out_path);
	if (TD_COPY_STRING == mode)
		f->path = td_page_strdup(&engine->alloc, out_path, path_len);
	else
		f->path = out_path;
	f->hash = hash;
	f->name = find_basename(f->path, path_len);
	f->bucket_next = engine->file_hash[slot];
	f->signer = engine->default_signer;
	f->stat_dirty = 1;
	f->signature_dirty = 1;
	f->frozen_relstring_index = ~(0u);
	engine->file_hash[slot] = f;
	td_mutex_unlock_or_die(engine->lock);
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

static void configure_from_env(td_engine *engine)
{
	const char *tmp;

	if (NULL != (tmp = getenv("TUNDRA_DEBUG")))
		engine->settings.debug_flags = atoi(tmp);

	if (NULL != (tmp = getenv("TUNDRA_THREADS")))
		engine->settings.thread_count = atoi(tmp);
}

static const char*
copy_string_field(lua_State *L, td_engine *engine, int index, const char *field_name)
{
	const char* str;
	lua_getfield(L, index, field_name);
	if (!lua_isstring(L, -1))
		luaL_error(L, "%s: expected a string", field_name);
	str = td_page_strdup_lua(L, &engine->alloc, -1, field_name);
	lua_pop(L, 1);
	return str;
}

static int make_engine(lua_State *L)
{
	int i;
	int debug_signing = 0;
	int use_digest_signing = 1;
	td_engine *self;

	self = lua_newuserdata(L, sizeof(td_engine));
	memset(self, 0, sizeof(td_engine));
	self->magic_value = 0xcafebabe;
	luaL_getmetatable(L, TUNDRA_ENGINE_MTNAME);
	lua_setmetatable(L, -2);

	/* Allow max 100 x 1 MB pages for nodes, filenames and such */
	td_alloc_init(&self->alloc, 100, 1024 * 1024);

	self->lock = td_page_alloc(&self->alloc, sizeof(pthread_mutex_t));
	td_mutex_init_or_die(self->lock, NULL);

	self->stats_lock = td_page_alloc(&self->alloc, sizeof(pthread_mutex_t));
	td_mutex_init_or_die(self->stats_lock, NULL);

	self->file_hash_size = 92413;
	self->relhash_size = 92413;
	self->start_time = time(NULL);
	self->settings.thread_count = td_get_processor_count();

	/* apply optional overrides */
	if (1 <= lua_gettop(L) && lua_istable(L, 1))
	{
		self->file_hash_size = get_int_override(L, 1, "FileHashSize", self->file_hash_size);
		self->relhash_size = get_int_override(L, 1, "RelationHashSize", self->relhash_size);
		self->settings.debug_flags = get_int_override(L, 1, "DebugFlags", 0);
		self->settings.verbosity = get_int_override(L, 1, "Verbosity", 0);
		self->settings.thread_count = get_int_override(L, 1, "ThreadCount", self->settings.thread_count);
		self->settings.dry_run = get_int_override(L, 1, "DryRun", 0);
		self->settings.continue_on_error = get_int_override(L, 1, "ContinueOnError", 0);
		use_digest_signing = get_int_override(L, 1, "UseDigestSigning", 1);
		debug_signing = get_int_override(L, 1, "DebugSigning", 0);
	}

	self->file_hash = (td_file **) calloc(sizeof(td_file*), self->file_hash_size);
	self->relhash = (struct td_relcell **) calloc(sizeof(struct td_relcell*), self->relhash_size);
	self->node_count = 0;

	if (use_digest_signing)
		self->default_signer = &sign_digest;
	else
		self->default_signer = &sign_timestamp;

	if (debug_signing)
		self->sign_debug_file = fopen("tundra-sigdebug.txt", "w");

	configure_from_env(self);

	td_load_ancestors(self);

	td_load_relcache(self);

	for (i = 0; i < TD_OBJECT_LOCK_COUNT; ++i)
		td_mutex_init_or_die(&self->object_locks[i], NULL);

	/* Finally, associate this engine with a table in the registry. */
	lua_pushvalue(L, -1);
	lua_newtable(L);
	lua_settable(L, LUA_REGISTRYINDEX);

	return 1;
}

static int engine_gc(lua_State *L)
{
	int i;
	td_engine *const self = td_check_engine(L, 1);

	if (self->magic_value != 0xcafebabe)
		luaL_error(L, "illegal userdatum; magic value check fails");

	self->magic_value = 0xdeadbeef;

	td_relcache_cleanup(self);

	if (self->sign_debug_file)
	{
	   fclose((FILE *)self->sign_debug_file);
	   self->sign_debug_file = NULL;
	}

	free(self->file_hash);
	self->file_hash = NULL;

	free(self->relhash);
	self->relhash = NULL;

	free(self->ancestor_used);
	self->ancestor_used = NULL;

	free(self->ancestors);
	self->ancestors = NULL;

	for (i = TD_OBJECT_LOCK_COUNT-1; i >= 0; --i)
		td_mutex_destroy_or_die(&self->object_locks[i]);

	td_mutex_destroy_or_die(self->stats_lock);
	td_mutex_destroy_or_die(self->lock);

	td_alloc_cleanup(&self->alloc);
	return 0;
}

static td_node *
make_pass_barrier(td_engine *engine, const td_pass *pass)
{
	char name[256];
	td_node *result;

	snprintf(name, sizeof(name), "<<pass barrier '%s'>>", pass->name);
	name[sizeof(name)-1] = '\0';

	result = td_page_alloc(&engine->alloc, sizeof(td_node));
	memset(result, 0, sizeof(*result));
	result->annotation = td_page_strdup(&engine->alloc, name, strlen(name));
	td_setup_ancestor_data(engine, result);
	++engine->node_count;
	return result;
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
	if (!name)
		luaL_error(L, "no name set for pass");

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

	engine->passes[i].name = td_page_strdup(&engine->alloc, name, name_len);
	engine->passes[i].build_order = build_order;
	engine->passes[i].barrier_node = make_pass_barrier(engine, &engine->passes[i]);

	lua_pop(L, 2);
	return i;
}

static int
setup_pass(lua_State *L, td_engine *engine, int index, td_node *node)
{
	td_pass *pass;
	td_job_chain *chain;
	int pass_index;

	lua_getfield(L, index, "pass");
	if (lua_isnil(L, -1))
		luaL_error(L, "no pass specified");

	pass_index = get_pass_index(L, engine, lua_gettop(L));
	lua_pop(L, 1);

	pass = &engine->passes[pass_index];

	/* link this node into the node list of the pass */
	chain = td_page_alloc(&engine->alloc, sizeof(td_job_chain));
	chain->node = node;
	chain->next = pass->nodes;
	pass->nodes = chain;
	++pass->node_count;

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
						node->annotation, f->path, his_pass->name,
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
					node->annotation, f->path, f->producer->annotation);
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

	++max_deps; /* for the pass barrier */

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

	/* always depend on the pass barrier */
	{
		td_pass *pass = &engine->passes[node->pass_index];
		deps[count++] = pass->barrier_node;
	}

	/* sort the dependency set to easily remove duplicates */
	qsort(deps, count, sizeof(td_node*), compare_ptrs);

	/* allocate a new scratch set to write the unique deps into */
	uniq_deps = (td_node **) alloca(count * sizeof(td_node*));

	if (!uniq_deps)
		luaL_error(L, "out of stack memory allocating %d bytes", count * sizeof(td_node*));

	/* filter deps into uniq_deps by merging adjecent duplicates */
	result_count = uniqize_deps(deps, count, uniq_deps);

	/* allocate and fill final result array as a copy of uniq_deps */
	result = td_page_alloc(&engine->alloc, sizeof(td_node*) * result_count);
	memcpy(result, uniq_deps, sizeof(td_node*) * result_count);

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
			signer = td_page_alloc(&engine->alloc, sizeof(td_signer));
			signer->is_lua = 1;
			signer->function.lua_reference = luaL_ref(L, -1); /* pops the value */
		}
		else
		{
			luaL_error(L, "signers must be either builtins (strings) or functions");
		}

		file = td_engine_get_file(engine, filename, TD_COPY_STRING);

		if (file->producer != node)
			luaL_error(L, "%s isn't produced by this node; can't sign it", filename);

		file->signer = signer;
	}

leave:
	lua_pop(L, 1);
}

/*
 * Concat "<key>=<value>" as the result string. We store away the string in a
 * Lua table so it will not be garbage collected. This procedure has the
 * advantage of sharing environment strings between all nodes, as Lua
 * internally store only one copy of each string.
 */
static const char*
record_env_mapping(td_engine *engine, lua_State *L, int engine_pos, int key, int value)
{
	const char* result;

	lua_pushvalue(L, engine_pos);
	lua_gettable(L, LUA_REGISTRYINDEX);

	lua_pushvalue(L, key);
	lua_pushlstring(L, "=", 1);
	lua_pushvalue(L, value);
	lua_concat(L, 3);

	result = lua_tostring(L, -1);

	lua_pushboolean(L, 1);
	lua_settable(L, -3);
	lua_pop(L, 1);

	return result;
}

void
td_call_cache_hook(lua_State *L, void *key, int spec_idx, int result_idx)
{
	lua_pushlightuserdata(L, key);
	lua_gettable(L, LUA_REGISTRYINDEX);
	if (lua_isnil(L, -1))
	{
		lua_pop(L, 1);
		return;
	}
	else
	{
		lua_pushvalue(L, spec_idx);
		lua_pushvalue(L, result_idx);
		lua_call(L, 2, 0);
	}
}

static int
check_flag(lua_State *L, int args_index, const char *name, int value)
{
	int result = 0;
	lua_getfield(L, 2, name);
	if (lua_toboolean(L, -1))
		result = value;
	lua_pop(L, 1);
	return result;
}

static int
make_node(lua_State *L)
{
	td_engine * const self = td_check_engine(L, 1);
	td_node *node = td_page_alloc(&self->alloc, sizeof(td_node));
	td_noderef *noderef;

	node->annotation = copy_string_field(L, self, 2, "annotation");
	node->action = copy_string_field(L, self, 2, "action");
	node->salt = copy_string_field(L, self, 2, "salt");
	node->pass_index = setup_pass(L, self, 2, node);

	lua_getfield(L, 2, "inputs");
	node->inputs = td_build_file_array(L, self, lua_gettop(L), &node->input_count);
	lua_pop(L, 1);
	check_input_files(L, self, node);

	lua_getfield(L, 2, "outputs");
	node->outputs = td_build_file_array(L, self, lua_gettop(L), &node->output_count);
	lua_pop(L, 1);
	tag_output_files(L, node);

	lua_getfield(L, 2, "aux_outputs");
	node->aux_outputs = td_build_file_array(L, self, lua_gettop(L), &node->aux_output_count);
	lua_pop(L, 1);

	lua_getfield(L, 2, "scanner");
	if (!lua_isnil(L, -1))
		node->scanner = td_check_scanner(L, -1);
	else
		node->scanner = NULL;
	lua_pop(L, 1);

	lua_getfield(L, 2, "env");
	if (!lua_isnil(L, -1))
	{
		int index = 0, count = 0;

		lua_pushnil(L);
		while (lua_next(L, -2))
		{
			++count;
			lua_pop(L, 1);
		}

		node->env_count = count;
		node->env = td_page_alloc(&self->alloc, sizeof(const char *) * (count));
		lua_pushnil(L);
		while (lua_next(L, -2))
		{
			int top = lua_gettop(L);
			node->env[index++] = record_env_mapping(self, L, 1, top-1, top);
			assert(top == lua_gettop(L));
			lua_pop(L, 1);
		}
	}
	else
	{
		node->env_count = 0;
		node->env = NULL;
	}
	lua_pop(L, 1);

	node->flags = 0;
	node->flags |= check_flag(L, 2, "is_precious", TD_NODE_PRECIOUS);
	node->flags |= check_flag(L, 2, "overwrite_outputs", TD_NODE_OVERWRITE);

	node->deps = setup_deps(L, self, node, &node->dep_count);

	setup_file_signers(L, self, node);

	memset(&node->job, 0, sizeof(node->job));

	noderef = (td_noderef*) lua_newuserdata(L, sizeof(td_noderef));
	noderef->node = node;
	luaL_getmetatable(L, TUNDRA_NODEREF_MTNAME);
	lua_setmetatable(L, -2);

	td_setup_ancestor_data(self, node);

	++self->node_count;

	td_call_cache_hook(L, &td_node_hook_key, 2, lua_gettop(L));

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
#define TD_MAX_EXTS 16 
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
		int x, emit = 0;
		const char *ext_pos;
		const char *fn = files[i]->path;

		ext_pos = strrchr(fn, '.');
		if (!ext_pos)
			ext_pos = "";

		if (ext_count > 0)
		{
			for (x = 0; x < ext_count; ++x)
			{
				if (0 == strcmp(ext_pos, exts[x]))
				{
					emit = 1;
					break;
				}
			}
		}
		else
			emit = 1;
		
		if (emit)
		{
			lua_pushstring(L, fn);
			lua_rawseti(L, 2, ++table_size);
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

/*
 * Execute actions needed to update a dependency graph.
 *
 * Input:
 * A list of dag nodes to build.
 */

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

static int
set_callback(lua_State *L, void* key)
{
	luaL_checktype(L, 2, LUA_TFUNCTION);
	lua_pushlightuserdata(L, key);
	lua_pushvalue(L, 2);
	lua_settable(L, LUA_REGISTRYINDEX);
	return 0;
}

static int
set_scanner_cache_hook(lua_State *L) { return set_callback(L, &td_scanner_hook_key); }

static int
set_node_cache_hook(lua_State *L) { return set_callback(L, &td_node_hook_key); }

static int
set_dirwalk_cache_hook(lua_State *L) { return set_callback(L, &td_dirwalk_hook_key); }

extern int td_build_nodes(lua_State* L);

static const luaL_Reg engine_mt_entries[] = {
	{ "make_node", make_node },
	{ "build", td_build_nodes }, /* in build_setup.c */
	{ "set_scanner_cache_hook", set_scanner_cache_hook },
	{ "set_node_cache_hook", set_node_cache_hook },
	{ "set_dirwalk_cache_hook", set_dirwalk_cache_hook },
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
