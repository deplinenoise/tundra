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

#ifdef _WIN32
#include <malloc.h> /* alloca */
#endif

#if defined(_MSC_VER)
#define snprintf _snprintf
#endif

#define TD_ANCESTOR_FILE ".tundra-ancestors"

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

	++engine->stats.timestamp_sign_count;
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

	++engine->stats.md5_sign_count;
}

static td_signer sign_timestamp = { 0, { td_sign_timestamp } };
static td_signer sign_digest = { 0, { td_sign_digest } };

static void
md5_string(MD5_CTX *context, const char *string)
{
	static unsigned char zero_byte = 0;

	if (string)
		MD5_Update(context, (unsigned char*) string, (int) strlen(string)+1);
	else
		MD5_Update(context, &zero_byte, 1);
}

static void
compute_node_guid(td_engine *engine, td_node *node)
{
	MD5_CTX context;
	MD5_Init(&context);
	md5_string(&context, node->action);
	md5_string(&context, node->annotation);
	md5_string(&context, node->salt);
	MD5_Final(node->guid.data, &context);

	if (td_debug_check(engine, TD_DEBUG_NODES))
	{
		char guidstr[33];
		td_digest_to_string(&node->guid, guidstr);
		printf("%s with guid %s\n", node->annotation, guidstr);
	}
}

static int compare_ancestors(const void* l_, const void* r_)
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

td_file *td_engine_get_file(td_engine *engine, const char *input_path, td_get_file_mode mode)
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

	slot = (int) (hash % engine->file_hash_size);
	chain = engine->file_hash[slot];
	
	while (chain)
	{
		if (chain->hash == hash && 0 == strcmp(out_path, chain->path))
			return chain;
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

static void
load_ancestors(td_engine *engine)
{
	FILE* f;
	int i, count;
	long file_size;
	size_t read_count;

	if (NULL == (f = fopen(TD_ANCESTOR_FILE, "rb")))
	{
		if (td_debug_check(engine, TD_DEBUG_ANCESTORS))
			printf("couldn't open %s; no ancestor information present\n", TD_ANCESTOR_FILE);
		return;
	}

	fseek(f, 0, SEEK_END);
	file_size = ftell(f);
	rewind(f);

	if (file_size % sizeof(td_ancestor_data) != 0)
		td_croak("illegal ancestor file: %d not a multiple of %d bytes",
				(int) file_size, sizeof(td_ancestor_data));

	engine->ancestor_count = count = (int) (file_size / sizeof(td_ancestor_data));
	engine->ancestors = malloc(file_size);
	engine->ancestor_used = (td_node **)calloc(sizeof(td_node *), count);
	read_count = fread(engine->ancestors, sizeof(td_ancestor_data), count, f);

	if (td_debug_check(engine, TD_DEBUG_ANCESTORS))
		printf("read %d ancestors\n", count);

	if (read_count != (size_t) count)
		td_croak("only read %d items, wanted %d", read_count, count);

	for (i = 1; i < count; ++i)
	{
		int cmp = compare_ancestors(&engine->ancestors[i-1], &engine->ancestors[i]);
		if (cmp == 0)
			td_croak("bad ancestor file; duplicate item (%d/%d)", i, count);
		if (cmp > 0)
			td_croak("bad ancestor file; bad sort order on item (%d/%d)", i, count);
	}

	if (td_debug_check(engine, TD_DEBUG_ANCESTORS))
	{
		printf("full ancestor dump on load:\n");
		for (i = 0; i < count; ++i)
		{
			char guid[33], sig[33];
			td_digest_to_string(&engine->ancestors[i].guid, guid);
			td_digest_to_string(&engine->ancestors[i].input_signature, sig);
			printf("%s %s %ld %d\n", guid, sig, (long) engine->ancestors[i].access_time, engine->ancestors[i].job_result);
		}
	}

	fclose(f);
}

static void
update_ancestors(
		td_engine *engine,
		td_node *node,
		time_t now,
		int *cursor,
		td_ancestor_data *output,
		unsigned char *visited)
{
	int i, count, output_index;

	if (node->job.flags & TD_JOBF_ANCESTOR_UPDATED)
		return;

	node->job.flags |= TD_JOBF_ANCESTOR_UPDATED;

	/* If this node had an ancestor record, flag it as visited. This way it
	 * will be disregarded when writing out all the other ancestors that
	 * weren't used this build session. */
	{
		const td_ancestor_data *data;
		if (NULL != (data = node->ancestor_data))
		{
			int index = (int) (data - engine->ancestors);
			assert(index < engine->ancestor_count);
			assert(0 == visited[index]);
			visited[index] = 1;
		}
	}

	output_index = *cursor;
	*cursor += 1;
	memset(&output[output_index], 0, sizeof(td_ancestor_data));

	memcpy(&output[output_index].guid, &node->guid, sizeof(td_digest));
	memcpy(&output[output_index].input_signature, &node->job.input_signature, sizeof(td_digest));
	output[output_index].access_time = now;
	output[output_index].job_result = node->job.state;

	for (i = 0, count = node->dep_count; i < count; ++i)
		update_ancestors(engine, node->deps[i], now, cursor, output, visited);
}

enum {
	TD_ANCESTOR_TTL_DAYS = 7,
	TD_SECONDS_PER_DAY = (60 * 60 * 24),
	TD_ANCESTOR_TTL_SECS = TD_SECONDS_PER_DAY * TD_ANCESTOR_TTL_DAYS,
};

static int
ancestor_timed_out(const td_ancestor_data *data, time_t now)
{
	return data->access_time + TD_ANCESTOR_TTL_SECS < now;
}

static void
save_ancestors(td_engine *engine, td_node *root)
{
	FILE* f;
	int i, count, max_count;
	int output_cursor, write_count;
	td_ancestor_data *output;
	unsigned char *visited;
	time_t now = time(NULL);
	const int dbg = td_debug_check(engine, TD_DEBUG_ANCESTORS);

	if (NULL == (f = fopen(TD_ANCESTOR_FILE ".tmp", "wb")))
	{
		fprintf(stderr, "warning: couldn't save ancestors\n");
		return;
	}

	max_count = engine->node_count + engine->ancestor_count;
	output = (td_ancestor_data *) malloc(sizeof(td_ancestor_data) * max_count);
	visited = (unsigned char *) calloc(engine->ancestor_count, 1);

	output_cursor = 0;
	update_ancestors(engine, root, now, &output_cursor, output, visited);

	if (dbg)
		printf("refreshed %d ancestors\n", output_cursor);

	for (i = 0, count = engine->ancestor_count; i < count; ++i)
	{
		const td_ancestor_data *a = &engine->ancestors[i];
		if (!visited[i] && !ancestor_timed_out(a, now))
			output[output_cursor++] = *a;
	}

	if (dbg)
		printf("%d ancestors to save in total\n", output_cursor);

	qsort(output, output_cursor, sizeof(td_ancestor_data), compare_ancestors);

	if (dbg)
	{
		printf("full ancestor dump on save:\n");
		for (i = 0; i < output_cursor; ++i)
		{
			char guid[33], sig[33];
			td_digest_to_string(&output[i].guid, guid);
			td_digest_to_string(&output[i].input_signature, sig);
			printf("%s %s %ld %d\n", guid, sig, (long) output[i].access_time, output[i].job_result);
		}
	}

	write_count = (int) fwrite(output, sizeof(td_ancestor_data), output_cursor, f);

	fclose(f);
	free(visited);
	free(output);

	if (write_count != output_cursor)
		td_croak("couldn't write %d entries; only wrote %d", output_cursor, write_count);

	if (0 != td_move_file(TD_ANCESTOR_FILE ".tmp", TD_ANCESTOR_FILE))
		td_croak("couldn't rename %s to %s", TD_ANCESTOR_FILE ".tmp", TD_ANCESTOR_FILE);
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
	int debug_signing = 0;
	int use_digest_signing = 1;
	td_engine *self = (td_engine*) lua_newuserdata(L, sizeof(td_engine));
	memset(self, 0, sizeof(td_engine));
	self->magic_value = 0xcafebabe;
	luaL_getmetatable(L, TUNDRA_ENGINE_MTNAME);
	lua_setmetatable(L, -2);

	/* Allow max 100 x 1 MB pages for nodes, filenames and such */
	td_alloc_init(&self->alloc, 100, 1024 * 1024);

	self->file_hash_size = 92413;
	self->relhash_size = 92413;
	self->L = L;
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

	load_ancestors(self);

	td_load_relcache(self);

	return 1;
}

static int engine_gc(lua_State *L)
{
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

	td_alloc_cleanup(&self->alloc);

	return 0;
}

static void
setup_ancestor_data(td_engine *engine, td_node *node)
{
	compute_node_guid(engine, node);

	++engine->stats.ancestor_checks;

	if (engine->ancestors)
	{
		td_ancestor_data key;
		key.guid = node->guid; /* only key field is relevant */

		node->ancestor_data = (td_ancestor_data *)
			bsearch(&key, engine->ancestors, engine->ancestor_count, sizeof(td_ancestor_data), compare_ancestors);

		if (node->ancestor_data)
		{
			int index = (int) (node->ancestor_data - engine->ancestors);
			td_node *other;
			if (NULL != (other = engine->ancestor_used[index]))
				td_croak("node error: nodes \"%s\" and \"%s\" share the same ancestor", node->annotation, other->annotation);
			engine->ancestor_used[index] = node;
			++engine->stats.ancestor_nodes;
		}
		else
		{
			if (td_debug_check(engine, TD_DEBUG_ANCESTORS))
			{
				char guidstr[33];
				td_digest_to_string(&node->guid, guidstr);
				printf("no ancestor for %s with guid %s\n", node->annotation, guidstr);
			}
		}
	}
	else
	{
		node->ancestor_data = NULL;
	}
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
	setup_ancestor_data(engine, result);
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

static const char*
make_env_key(td_engine *engine, lua_State *L, int key, int value)
{
	int count;
	char mapping[2048];
	count = snprintf(mapping, sizeof(mapping), "%s=%s", lua_tostring(L, key), lua_tostring(L, value));

	return td_page_strdup(&engine->alloc, mapping, count);
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
			node->env[index++] = make_env_key(self, L, -2, -1);
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

	setup_ancestor_data(self, node);

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
		int x;
		const char *ext_pos;
		const char *fn = files[i]->path;

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

	chain = td_page_alloc(&engine->alloc, sizeof(td_job_chain));
	chain->node = blocked_node;
	chain->next = blocking_node->job.pending_jobs;
	blocking_node->job.pending_jobs = chain;
	blocked_node->job.block_count++;
}

enum {
	TD_MAX_DEPTH = 1024
};

static void
assign_jobs(td_engine *engine, td_node *root_node, td_node *stack[TD_MAX_DEPTH], int level)
{
	int i, dep_count;
	td_node **deplist = root_node->deps;

	for (i = 0; i < level; ++i)
	{
		if (stack[i] == root_node)
		{
			fprintf(stderr, "cyclic dependency detected:\n");
			for (; i < level; ++i)
			{
				fprintf(stderr, "  \"%s\" depends on\n", stack[i]->annotation);
			}
			fprintf(stderr, "  \"%s\"\n", root_node->annotation);

			td_croak("giving up");
		}
	}

	if (level >= TD_MAX_DEPTH)
		td_croak("dependency graph is too deep; bump TD_MAX_DEPTH");

	stack[level] = root_node;

	dep_count = root_node->dep_count;

	if (0 == (TD_JOBF_SETUP_COMPLETE & root_node->job.flags))
	{
		for (i = 0; i < dep_count; ++i)
		{
			td_node *dep = deplist[i];
			add_pending_job(engine, dep, root_node);
		}

		for (i = 0; i < dep_count; ++i)
		{
			td_node *dep = deplist[i];
			assign_jobs(engine, dep, stack, level+1);
		}
	}

	root_node->job.flags |= TD_JOBF_SETUP_COMPLETE;
}

static int
comp_pass_ptrs(const void *l, const void *r)
{
	const td_pass **lhs = (const td_pass **)l;
	const td_pass **rhs = (const td_pass **)r;
	return (*lhs)->build_order - (*rhs)->build_order;
}

static void add_pass_deps(td_engine *engine, td_pass *prec, td_pass *succ)
{
	int count;
	td_node **dep_array;
	td_job_chain *chain;

	dep_array = td_page_alloc(&engine->alloc, sizeof(td_node*) * prec->node_count);
	count = 0;
	chain = prec->nodes;
	while (chain)
	{
		dep_array[count++] = chain->node;
		chain = chain->next;
	}
	assert(count == prec->node_count);

	succ->barrier_node->dep_count = count;
	succ->barrier_node->deps = dep_array;
}

static void
connect_pass_barriers(td_engine *engine)
{
	int i, count;
	td_pass *passes[TD_PASS_MAX];

	count = engine->pass_count;
	for (i = 0; i < count; ++i)
		passes[i] = &engine->passes[i];

	qsort(&passes[0], count, sizeof(td_pass *), comp_pass_ptrs);

	/* arrange for job barriers to depend on all nodes in the preceding pass */
	for (i = 1; i < count; ++i)
	{
		add_pass_deps(engine, passes[i-1], passes[i]);
	}
}

enum { TD_MAX_CLEAN_DIRS = 4096 };

static void
clean_file(td_engine *engine, td_node *node, td_file **dirs, int *dir_count, td_file *file)
{
	int k;
	const td_stat *stat;
	td_file *dir = td_parent_dir(engine, file);

	/* scan for this directory */
	for (k = *dir_count - 1; k >= 0; --k)
	{
		if (dirs[k] == dir)
			break;
	}

	if (k < 0)
	{
		int index = *dir_count;
		if (index >= TD_MAX_CLEAN_DIRS)
			td_croak("too many dirs to clean! limit is %d", TD_MAX_CLEAN_DIRS);
		*dir_count = index + 1;
		dirs[index] = dir;
	}

	/* Don't delete the output of precious nodes. */
	if (TD_NODE_PRECIOUS & node->flags)
		return;

	stat = td_stat_file(engine, file);

	if (TD_STAT_EXISTS & stat->flags)
	{
		if (td_verbosity_check(engine, 1))
			printf("Clean %s\n", file->path);

		if (0 != remove(file->path))
			fprintf(stderr, "error: couldn't remove %s\n", file->path);

		td_touch_file(file);
	}
}

static void
clean_output_files(td_engine *engine, td_node *root, td_file **dirs, int *dir_count)
{
	int i, count;

	root->job.flags |= TD_JOBF_CLEANED;

	for (i = 0, count = root->output_count; i < count; ++i)
		clean_file(engine, root, dirs, dir_count, root->outputs[i]);

	for (i = 0, count = root->aux_output_count; i < count; ++i)
		clean_file(engine, root, dirs, dir_count, root->aux_outputs[i]);

	for (i = 0, count = root->dep_count; i < count; ++i)
	{
		td_node *dep = root->deps[i];
		if (0 == (dep->job.flags & TD_JOBF_CLEANED))
		{
			clean_output_files(engine, dep, dirs, dir_count);
		}
	}
}

static int
path_separator_count(const char *fn, int len)
{
	int i, result = 0;
	for (i = 0; i < len; ++i)
	{
		char ch = fn[i];
		if ('/' == ch || '\\' == ch)
			++result;
	}
	return result;
}

static int
directory_depth_compare(const void *l, const void *r)
{
	int lc, rc;
	const td_file *lf = *(const td_file **)l;
	const td_file *rf = *(const td_file **)r;

	/* Just count the number of path separators in the path, as it is normalized. */
	lc = path_separator_count(lf->path, lf->path_len);
	rc = path_separator_count(rf->path, rf->path_len);

	return rc - lc;
}

static void
clean_files(td_engine *engine, td_node *root)
{
	int i;
	int dir_clean_count = 0;
	td_file *dirs_to_clean[TD_MAX_CLEAN_DIRS];

	/* First remove as many files as possible. As we pass through directories
	 * with generated files, keep track of those. We assume that directories
	 * where output files are placed are OK to remove when cleaning provided
	 * they are empty.*/
	clean_output_files(engine, root, dirs_to_clean, &dir_clean_count);

	/* Now sort the list of dirs in depth order so that leaves will be removed
	 * first. */
	qsort(&dirs_to_clean, dir_clean_count, sizeof(td_file *), directory_depth_compare);

	/* Now we can try to remove directories if they are empty. */
	for (i = 0; i < dir_clean_count; ++i)
	{
		if (0 == td_rmdir(dirs_to_clean[i]->path))
		{
			if (td_verbosity_check(engine, 1))
				printf("RmDir %s\n", dirs_to_clean[i]->path);
		}
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
	td_build_result build_result;
	int pre_file_count;
	td_engine * self;
	td_node *root;
	double t1, t2, script_end_time;
	extern int global_tundra_exit_code;
	int is_clean = 0;

	/* record this timestamp as the time when Lua ended */
	script_end_time = td_timestamp();

	self = td_check_engine(L, 1);
	root = td_check_noderef(L, 2)->node;

	if (lua_gettop(L) >= 3)
	{
		const char *how = luaL_checkstring(L, 3);
		if (0 == strcmp("clean", how))
		{
			is_clean = 1;
		}
	}

	connect_pass_barriers(self);

	pre_file_count = self->stats.file_count;

	t1 = td_timestamp();
	if (0 == is_clean)
	{
		int jobs_run = 0;
		td_node *stack[TD_MAX_DEPTH];
		assign_jobs(self, root, stack, 0);
		build_result = td_build(self, root, &jobs_run);
		printf("*** build %s, %d jobs run\n", td_build_result_names[build_result], jobs_run);
	}
	else
	{
		clean_files(self, root);
		build_result = TD_BUILD_SUCCESS;
	}
	t2 = td_timestamp();

	if (td_debug_check(self, TD_DEBUG_STATS))
	{
		extern int global_tundra_stats;
		extern double script_call_t1;
		extern int walk_path_count;
		extern double walk_path_time;
		double file_load = 100.0 * self->stats.file_count / self->file_hash_size;
		double relation_load = 100.0 * self->stats.relation_count / self->relhash_size;

		printf("post-build stats:\n");
		printf("  files tracked: %d (%d directly from DAG), file table load %.2f%%\n", self->stats.file_count, pre_file_count, file_load);
		printf("  relations tracked: %d, table load %.2f%%\n", self->stats.relation_count, relation_load);
		printf("  relation cache load: %.3fs save: %.3fs\n", self->stats.relcache_load, self->stats.relcache_save);
		printf("  nodes with ancestry: %d of %d possible\n", self->stats.ancestor_nodes, self->stats.ancestor_checks);
		printf("  time spent in Lua doing setup: %.3fs\n", script_end_time - script_call_t1);
		printf("    - time spent iterating directories (glob): %.3fs over %d calls\n", walk_path_time, walk_path_count);
		printf("  total time spent in build loop: %.3fs\n", t2-t1);
		printf("    - implicit dependency scanning: %.3fs\n", self->stats.scan_time);
		printf("    - output directory creation/mgmt: %.3fs\n", self->stats.mkdir_time);
		printf("    - command execution: %.3fs\n", self->stats.build_time);
		printf("    - stat() time: %.3fs (%d calls out of %d queries)\n", self->stats.stat_time, self->stats.stat_calls, self->stats.stat_checks);
		printf("    - file signing time: %.3fs (md5: %d, timestamp: %d)\n", self->stats.file_signing_time, self->stats.md5_sign_count, self->stats.timestamp_sign_count);
		printf("    - up2date checks time: %.3fs\n", self->stats.up2date_check_time);
		if (t2 > t1)
			printf("  efficiency: %.2f%%\n", (self->stats.build_time * 100.0) / (t2-t1));

		/* have main print total time spent, including script */
		global_tundra_stats = 1;
	}

	if (!self->settings.dry_run)
	{
		save_ancestors(self, root);
		td_save_relcache(self);
	}

	if (TD_BUILD_SUCCESS == build_result)
		global_tundra_exit_code = 0;
	else
		global_tundra_exit_code = 1;
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

static const luaL_Reg engine_mt_entries[] = {
	{ "make_node", make_node },
	{ "build", build_nodes },
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

const td_stat *
td_stat_file(td_engine *engine, td_file *f)
{
	double t1, t2;
	t1 = td_timestamp();
	++engine->stats.stat_checks;
	if (f->stat_dirty)
	{
		++engine->stats.stat_calls;
		if (0 != fs_stat_file(f->path, &f->stat))
		{
			f->stat_dirty = 0;
			f->stat.flags = 0;
			f->stat.size = 0;
			f->stat.timestamp = 0;
		}
		f->stat_dirty = 0;
	}
	t2 = td_timestamp();
	engine->stats.stat_time += t2 - t1;

	return &f->stat;
}

void
td_touch_file(td_file *f)
{
	f->stat_dirty = 1;
	f->signature_dirty = 1;
}

td_digest *
td_get_signature(td_engine *engine, td_file *f)
{
	double t1, t2;

	if (f->signature_dirty)
	{
		t1 = td_timestamp();

		if (!engine->settings.dry_run)
		{
			assert(f->signer);

			if (f->signer->is_lua)
				td_croak("lua signers not implemented yet");
			else
				(*f->signer->function.function)(engine, f, &f->signature);

		}
		else
		{
			memset(&f->signature, 0, sizeof(f->signature));
		}

		f->signature_dirty = 0;
		t2 = td_timestamp();
		engine->stats.file_signing_time += t2 - t1;
	}
	return &f->signature;
}

td_file *td_parent_dir(td_engine *engine, td_file *f)
{
	int i;
	char path_buf[512];

	if (f->path_len >= sizeof(path_buf) - 1)
		td_croak("path too long: %s", f->path);

	strncpy(path_buf, f->path, sizeof(path_buf));

	for (i = f->path_len - 1; i >= 0; --i)
	{
		char ch = path_buf[i];
		if (TD_PATHSEP == ch)
		{
			path_buf[i] = '\0';
			return td_engine_get_file(engine, path_buf, TD_COPY_STRING);
		}
	}

	return NULL;
}
