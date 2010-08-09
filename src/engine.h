#ifndef TUNDRA_ENGINE_H
#define TUNDRA_ENGINE_H

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

#include <stddef.h>
#include <time.h>
#include "portable.h"
#include "util.h"

#define TUNDRA_ENGINE_MTNAME "tundra_engine"
#define TUNDRA_NODEREF_MTNAME "tundra_noderef"

struct td_engine;
struct td_file;

typedef struct td_digest {
	unsigned char data[16];
} td_digest;

struct lua_State;

enum
{
	TD_PASS_MAX = 32
};

typedef void (*td_sign_fn)(struct td_engine *engine, struct td_file *f, td_digest *out);

typedef struct td_signer
{
	int is_lua;
	union {
		td_sign_fn function;
		int lua_reference;
	} function;
} td_signer;

enum {
	TD_STAT_DIR = 1 << 0,
	TD_STAT_EXISTS = 1 << 1
};

typedef struct td_stat {
	int flags;
	uint64_t size;
	time_t timestamp;
} td_stat;

typedef struct td_file
{
	struct td_file *bucket_next;

	unsigned int hash;
	const char *path;
	const char *name; /* points into path */
	int path_len; /* # characters in path string */

	struct td_node *producer;
	td_signer* signer;

	int signature_dirty;
	td_digest signature;

	int stat_dirty;
	struct td_stat stat;

	uint32_t frozen_relstring_index;
} td_file;

typedef enum td_jobstate
{
	TD_JOB_INITIAL         = 0,
	TD_JOB_BLOCKED         = 1,
	TD_JOB_SCANNING        = 2,
	TD_JOB_RUNNING         = 3,
	TD_JOB_COMPLETED       = 100,
	TD_JOB_FAILED          = 101,
	TD_JOB_CANCELLED       = 102,
	TD_JOB_UPTODATE        = 103
} td_jobstate;

typedef struct td_job_chain
{
	struct td_node *node;
	struct td_job_chain *next;
} td_job_chain;

enum
{
	TD_JOBF_QUEUED            = 1 <<  0,
	TD_JOBF_ROOT              = 1 <<  1,
	TD_JOBF_ANCESTOR_UPDATED  = 1 << 16,
	TD_JOBF_SETUP_COMPLETE    = 1 << 17,
	TD_JOBF_CLEANED           = 1 << 18
};


typedef struct td_job
{
	int flags;
	td_jobstate state;

	/* implicit dependencies, discovered by the node's scanner */
	int idep_count;
	td_file **ideps;

	/* # of jobs that must complete before this job can run */
	int block_count;

	/* # of dependencies that have failed */
	int failed_deps;

	/* list of jobs this job will unblock once completed */
	td_job_chain *pending_jobs;

	td_digest input_signature;
} td_job;

typedef struct td_ancestor_data
{
	td_digest guid;
	td_digest input_signature;
	int job_result;
	time_t access_time;
} td_ancestor_data;

typedef struct td_node
{
	/* An annotation that should print when building this node.
	 * Something like "Cc file.o" is typical */
	const char *annotation;

	/* The shell command to run to update this node. */
	const char *action;

	/* A salt string used to make this node's identity * unique among the
	 * ancestors. This is usually the build id. */
	const char *salt;

	/* Array of input files. */
	int input_count;
	td_file **inputs;

	/* Array of output files. These are signed. */
	int output_count;
	td_file **outputs;

	/* Array of auxiliary output files. These are not tracked, but will be cleaned. */
	int aux_output_count;
	td_file **aux_outputs;

	/* Array of environment bindings */
	int env_count;
	const char **env;

	/* An index into the engine's pass array. */
	int pass_index;

	struct td_scanner *scanner;

	int dep_count;
	struct td_node **deps;

	td_digest guid;
	const td_ancestor_data *ancestor_data;

	td_job job;
} td_node;

typedef struct td_noderef
{
	td_node *node;
} td_noderef;

typedef struct td_pass
{
	const char *name;
	int build_order;
	td_node *barrier_node;
	int node_count;
	td_job_chain *nodes;
} td_pass;

enum
{
	TD_DEBUG_QUEUE = 1 << 0,
	TD_DEBUG_NODES = 1 << 1,
	TD_DEBUG_ANCESTORS = 1 << 2,
	TD_DEBUG_STATS = 1 << 3,
	TD_DEBUG_REASON = 1 << 4,
	TD_DEBUG_SCAN = 1 << 5
};

struct td_relcell;
struct td_frozen_reldata;

typedef struct td_engine
{
	int magic_value;

	/* memory allocation */
	td_alloc alloc;

	/* file db */
	int file_hash_size;
	td_file **file_hash;

	/* file relation cache */
	int relhash_size;
	struct td_relcell **relhash;

	/* build passes */
	int pass_count;
	td_pass passes[TD_PASS_MAX];

	td_signer *default_signer;

	int node_count;

	struct {
		int verbosity;
		int debug_flags;
		int thread_count;
		int dry_run;
		int continue_on_error;
	} settings;

	struct {
		int relation_count;
		int file_count;
		int stat_calls;
		int stat_checks;
		int ancestor_checks;
		int ancestor_nodes;
		int md5_sign_count;
		int timestamp_sign_count;
		double scan_time;
		double build_time;
		double mkdir_time;
		double stat_time;
		double up2date_check_time;
		double file_signing_time;
		double relcache_load;
		double relcache_save;
		int build_called;
	} stats;

	int ancestor_count;
	struct td_ancestor_data *ancestors;
	struct td_node **ancestor_used;

	struct lua_State *L;

	time_t start_time;

	struct td_frozen_reldata *relcache_data;
} td_engine;

#define td_verbosity_check(engine, level) ((engine)->settings.verbosity >= (level))
#define td_debug_check(engine, flags) ((engine)->settings.debug_flags & (flags))

#define td_check_noderef(L, index) ((struct td_noderef *) luaL_checkudata(L, index, TUNDRA_NODEREF_MTNAME))
#define td_check_engine(L, index) ((struct td_engine *) luaL_checkudata(L, index, TUNDRA_ENGINE_MTNAME))

typedef enum {
	TD_BORROW_STRING = 0,
	TD_COPY_STRING = 1,
} td_get_file_mode;

td_file *
td_engine_get_file(td_engine *engine, const char *path, td_get_file_mode mode);

const td_stat*
td_stat_file(td_engine *engine, td_file *f);

void
td_touch_file(td_file *f);

td_digest *
td_get_signature(td_engine *engine, td_file *f);

td_file *
td_parent_dir(td_engine *engine, td_file *f);

#endif
