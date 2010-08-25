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
#include "build.h"
#include "clean.h"
#include "ancestors.h"
#include "relcache.h"

#include <lua.h>
#include <lauxlib.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

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

int
td_build_nodes(lua_State* L)
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
		td_clean_files(self, root);
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
		printf("    - (parallel) stat() time: %.3fs (%d calls out of %d queries)\n", self->stats.stat_time, self->stats.stat_calls, self->stats.stat_checks);
		printf("    - (parallel) file signing time: %.3fs (md5: %d, timestamp: %d)\n", self->stats.file_signing_time, self->stats.md5_sign_count, self->stats.timestamp_sign_count);
		printf("    - up2date checks time: %.3fs\n", self->stats.up2date_check_time);
		if (t2 > t1)
			printf("  efficiency: %.2f%%\n", (self->stats.build_time * 100.0) / (t2-t1));

		/* have main print total time spent, including script */
		global_tundra_stats = 1;
	}

	if (!self->settings.dry_run)
	{
		td_save_ancestors(self, root);
		td_save_relcache(self);
	}

	if (TD_BUILD_SUCCESS == build_result)
		global_tundra_exit_code = 0;
	else
		global_tundra_exit_code = 1;

	/* Finally remove our table of environment mappings. */
	lua_pushvalue(L, 1);
	lua_pushnil(L);
	lua_settable(L, LUA_REGISTRYINDEX);

	return 0;
}
