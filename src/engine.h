#ifndef TUNDRA_ENGINE_H
#define TUNDRA_ENGINE_H

#include <stddef.h>

#define TUNDRA_ENGINE_MTNAME "tundra_engine"
#define TUNDRA_NODEREF_MTNAME "tundra_noderef"

typedef struct td_digest {
	char data[16];
} td_digest;

struct lua_State;

enum
{
	TD_PASS_MAX = 32
};

typedef int (*td_sign_fn)(const char *filename, char digest_out[16]);

typedef struct td_signer_tag
{
	int is_lua;
	union {
		td_sign_fn function;
		int lua_reference;
	} function;
} td_signer;


typedef struct td_file_tag
{
	unsigned int hash;
	const char *path;
	const char *name;
	struct td_node_tag *producer;
	td_signer *signer;
	char signature[16];
	struct td_file_tag *bucket_next;
} td_file;

typedef enum td_jobstate_tag
{
	TD_JOB_INITIAL         = 0,
	TD_JOB_BLOCKED         = 1,
	TD_JOB_SCANNING        = 2,
	TD_JOB_RUNNING         = 3,
	TD_JOB_COMPLETED       = 100,
	TD_JOB_FAILED          = 101,
	TD_JOB_CANCELLED       = 102
} td_jobstate;

typedef struct td_job_chain_tag
{
	struct td_node_tag *node;
	struct td_job_chain_tag *next;
} td_job_chain;

enum
{
	TD_JOBF_QUEUED		= 1 << 0,
	TD_JOBF_ROOT		= 1 << 1
};


typedef struct td_job_tag 
{
	int flags;
	td_jobstate state;
	struct td_node_tag *node;

	/* implicit dependencies, discovered by the node's scanner */
	int idep_count;
	td_file **ideps;

	/* # of jobs that must complete before this job can run */
	int block_count;

	/* # of dependencies that have failed */
	int failed_deps;

	/* list of jobs this job will unblock once completed */
	td_job_chain *pending_jobs;
} td_job;

typedef struct td_node_tag
{
	const char *annotation;
	const char *action;

	int input_count;
	td_file **inputs;

	int output_count;
	td_file **outputs;

	int pass_index;

	struct td_scanner_tag *scanner;

	int dep_count;
	struct td_node_tag **deps;

	td_job job;
} td_node;

typedef struct td_noderef_tag
{
	td_node *node;
} td_noderef;

typedef struct td_pass_tag
{
	const char *name;
	int build_order;
	td_node *barrier_node;
	int node_count;
	td_job_chain *nodes;
} td_pass;

typedef struct td_alloc_tag
{
	/* memory allocation */
	int page_index;
	int page_left;
	int page_size;
	int total_page_count;
	char **pages;
} td_alloc;

/* Caches a relation between a file and set of other files (such as set of
 * included files) */
typedef struct td_relcell_tag
{
	/* source file */
	td_file *file;

	/* a salt value to make this relation unique */
	unsigned int salt;

	/* the related files */
	int count;
	td_file **files;

	/* the digest of file when the relation was cached */
	td_digest file_digest;

	/* for hash table linked list maintenance */
	struct td_relcell_tag *bucket_next;
} td_relcell;

typedef struct td_engine_tag
{
	int magic_value;

	/* memory allocation */
	td_alloc alloc;

	/* file db */
	int file_hash_size;
	td_file **file_hash;

	/* file relation cache */
	int relhash_size;
	td_relcell **relhash;

	/* build passes */
	int pass_count;
	td_pass passes[TD_PASS_MAX];

	td_signer *default_signer;

	int node_count;

	struct {
		int debug_level;
		int thread_count;
	} settings;

	struct lua_State *L;
} td_engine;

#ifndef NDEBUG
#define td_debug_check(engine, level) ((engine)->settings.debug_level >= (level))
#else
#define td_debug_check(engine, level) (0)
#endif

#define td_check_noderef(L, index) ((struct td_noderef_tag *) luaL_checkudata(L, index, TUNDRA_NODEREF_MTNAME))
#define td_check_engine(L, index) ((struct td_engine_tag *) luaL_checkudata(L, index, TUNDRA_ENGINE_MTNAME))

void *td_page_alloc(td_alloc *engine, size_t size);
td_file *td_engine_get_file(td_engine *engine, const char *path);

td_file **
td_engine_get_relations(td_engine *engine, td_file *file, unsigned int salt, int *count_out);

void
td_engine_set_relations(td_engine *engine, td_file *file, unsigned int salt, int count, td_file **files);

#endif
