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

struct lua_State;

enum
{
	TD_PASS_MAX = 32
};

extern char td_scanner_hook_key;
extern char td_node_hook_key;
extern char td_dirwalk_hook_key;

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
	/* Indicates that the job structure is on the job queue. */
	TD_JOBF_QUEUED            = 1 <<  0,

	/* This job is the root job. Only one job should have this set. When this
	 * job completes, the build queue exists. */
	TD_JOBF_ROOT              = 1 <<  1,

	/* Housekeeping flag used when serializing ancestor information. */
	TD_JOBF_ANCESTOR_UPDATED  = 1 << 16,

	/* Housekeeping flag used when setting up dependencies before a build. */
	TD_JOBF_SETUP_COMPLETE    = 1 << 17,

	/* Housekeeping flag used when cleaning nodes. */
	TD_JOBF_CLEANED           = 1 << 18
};


typedef struct td_job
{
	/* See TD_JOBF_* */
	int flags;

	/* State of this job in the job queue; >= 100 means job has completed in one way or another. */
	td_jobstate state;

	/* implicit dependencies, discovered by the node's scanner */
	int idep_count;
	td_file **ideps;

	/* # of jobs that must complete before this job can run */
	int block_count;

	/* # of dependencies that have failed */
	int failed_deps;

	/* List of jobs this job will unblock once completed. */
	td_job_chain *pending_jobs;

	/* Contains input signature of this job when state >= TD_JOB_RUNNING. Saved
	 * as ancestor data. */
	td_digest input_signature;
} td_job;

/* Ancestor data for a node. An ancestor records for a node guid the input
 * signature and job result of the last time the node was updated. There's also
 * housekeeping information to garbage collect these records when they haven't
 * been accessed for a long time. These are the basis of the rebuild checks.
 */
typedef struct td_ancestor_data
{
	td_digest guid;
	td_digest input_signature;
	int job_result;
	time_t access_time;
} td_ancestor_data;

enum {
	/* Don't delete node outputs on build error or when cleaning. Useful for
	 * build jobs that take a very long time to run. */
	TD_NODE_PRECIOUS = 1 << 0,

	/* Don't bother deleting node outputs before running actions for this node. */
	TD_NODE_OVERWRITE = 1 << 1,
};

/* The DAG node structure. With the exception of the `job' sub-structure this
 * structure is read-only during the build process and may be read freely
 * by any build thread.
 *
 * Nodes are allocated from the engine linear allocator when Lua calls
 * make_node, and are then wrapped with a td_noderef structure which Lua can
 * decide to throw away later if the script doesn't keep a reference to it.
 */
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

	/* Implicit dependency scanner */
	struct td_scanner *scanner;

	/* Direct dependencies */
	int dep_count;
	struct td_node **deps;

	/* GUID of this node, computed from action, annotation, inputs and so on.
	 * This value is used to find ancestor data from previous runs. */
	td_digest guid;

	/* Ancestor data, if present */
	const td_ancestor_data *ancestor_data;

	/* Node flags, one of TD_NODE_PRECIOUS or TD_NODE_OVERWRITE */
	int flags;

	/* Dynamic job structure */
	td_job job;
} td_node;

/* Lua wrapper for a td_node. These can be GC'd, but we don't want the nodes
 * themeselves to be GC'd from Lua because we store internal pointers to them
 * in the td_node structure. */
typedef struct td_noderef
{
	td_node *node;
} td_noderef;

/* A build pass. Passes are create on-demand based on the pass data received in
 * make_node calls. */
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

enum {
	TD_OBJECT_LOCK_COUNT = 64
};

struct td_relcell;
struct td_frozen_reldata;

/* "Build engine" state as seen from Lua. Stores global data for a build
 * session accumulated through Lua calls. */
typedef struct td_engine
{
	int magic_value;

	/* Linear memory allocator. Nodes, files and so on are allocated here. */
	td_alloc alloc;

	/* Hash table of files. All mentioned files map to a unique td_file
	 * structure; this hash table serves as a place to look up files on name. */
	int file_hash_size;
	td_file **file_hash;

	/* File relation cache hash table. Maps a (salted) file to a list of other
	 * files. In the common case of header dependencies, the key file would be
	 * a c or h file and the salt would be a hash of the CPPPATH. This is so
	 * different uses of the same header/source file can have different
	 * dependency lists depending on include path. */
	int relhash_size;
	struct td_relcell **relhash;

	/* Build passes set up implicitly through pass data in make_node calls. */
	int pass_count;
	td_pass passes[TD_PASS_MAX];

	/* Default signer object for nodes created with a nil signer. Can be set to
	 * either timestamp or MD5 via the "UseDigestSigning" option when creating
	 * the engine. */
	td_signer *default_signer;

	/* For stats; total # DAG nodes created. */
	int node_count;

	/* Settings picked up from Lua creation call. */
	struct {
		int verbosity;
		int debug_flags;
		int thread_count;
		int dry_run;
		int continue_on_error;
	} settings;

	/* Stats; lock `stats_lock' to modify these. */
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

	/* # anctors in array. Read directly from disk. */
	int ancestor_count;
	struct td_ancestor_data *ancestors;

	/* Array of pointers to nodes that are associated with ancestors in the
	 * `ancestors' array. Array is exactly the same length as `ancestors'. This
	 * is so we can track what ancestors have already been attached to nodes.
	 */
	struct td_node **ancestor_used;

	/* A start time; saved with newly created ancestor records for future GC. */
	time_t start_time;

	/* Deserialized relation cache data. */
	struct td_frozen_reldata *relcache_data;

	/* Lock for general modification; for example of the file table, relation
	 * cache and so on. */
	pthread_mutex_t *lock;

	/* Lock for modifying stats. */
	pthread_mutex_t *stats_lock;

	/* Really a pointer to FILE; but we avoid including stdio.h here. If set,
	 * points to a FILE where signature debug info can be output .*/
	void *sign_debug_file;

	/* Small collision table of mutexes to lock file objects e.g. when
	 * computing input signatures and stat() data. Files index into this array
	 * using their name hash value modulo he table size. See files.c */
	pthread_mutex_t object_locks[TD_OBJECT_LOCK_COUNT];
} td_engine;

#define td_verbosity_check(engine, level) ((engine)->settings.verbosity >= (level))
#define td_debug_check(engine, flags) ((engine)->settings.debug_flags & (flags))

#define td_check_noderef(L, index) ((struct td_noderef *) luaL_checkudata(L, index, TUNDRA_NODEREF_MTNAME))
#define td_check_engine(L, index) ((struct td_engine *) luaL_checkudata(L, index, TUNDRA_ENGINE_MTNAME))

void
td_call_cache_hook(struct lua_State *L, void* key, int spec_idx, int result_idx);

int td_compare_ancestors(const void* l, const void* r);

void td_sign_timestamp(td_engine *engine, td_file *f, td_digest *out);
void td_sign_digest(td_engine *engine, td_file *file, td_digest *out);

#endif
