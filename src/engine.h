#ifndef TUNDRA_ENGINE_H
#define TUNDRA_ENGINE_H

#include <stddef.h>

#define TUNDRA_ENGINE_MTNAME "tundra_engine"
#define TUNDRA_NODEREF_MTNAME "tundra_noderef"

typedef struct td_digest {
	char data[16];
} td_digest;

struct lua_State;

struct td_pass_tag;
struct td_node_tag;
struct td_noderef_tag;
struct td_job_tag;
struct td_engine_tag;
struct td_file_tag;

typedef struct td_pass_tag td_pass;
typedef struct td_node_tag td_node;
typedef struct td_noderef_tag td_noderef;
typedef struct td_job_tag td_job;
typedef struct td_engine_tag td_engine;
typedef struct td_file_tag td_file;

#define td_check_noderef(L, index) ((struct td_noderef_tag *) luaL_checkudata(L, index, TUNDRA_NODEREF_MTNAME))
#define td_check_engine(L, index) ((struct td_engine_tag *) luaL_checkudata(L, index, TUNDRA_ENGINE_MTNAME))

void *td_engine_alloc(td_engine *engine, size_t size);
char *td_engine_strdup(td_engine *engine, const char* str, size_t len);
char *td_engine_strdup_lua(struct lua_State *L, td_engine *engine, int index, const char *context);
const char ** td_build_string_array(struct lua_State* L, td_engine *engine, int index, int *count_out);
td_file **td_build_file_array(struct lua_State* L, td_engine *engine, int index, int *count_out);

td_file *td_engine_get_file(td_engine *engine, const char *path);

#endif
