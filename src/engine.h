#ifndef TUNDRA_ENGINE_H
#define TUNDRA_ENGINE_H

#include <stddef.h>

#define TUNDRA_ENGINE_MTNAME "tundra_engine"
#define TUNDRA_NODE_MTNAME "tundra_node"

struct lua_State;

struct td_pass_tag;
struct td_node_tag;
struct td_job_tag;
struct td_engine_tag;

typedef struct td_pass_tag td_pass;
typedef struct td_node_tag td_node;
typedef struct td_job_tag td_job;
typedef struct td_engine_tag td_engine;

#define td_check_node(L, index) ((struct td_node_tag *) luaL_checkudata(L, index, TUNDRA_NODE_MTNAME))
#define td_check_engine(L, index) ((struct td_engine_tag *) luaL_checkudata(L, index, TUNDRA_ENGINE_MTNAME))

void *td_engine_alloc(td_engine *engine, size_t size);
char *td_engine_strdup(td_engine *engine, const char* str, size_t len);
char *td_engine_strdup_lua(struct lua_State *L, td_engine *engine, int index, const char *context);
const char ** td_build_string_array(struct lua_State* L, td_engine *engine, int index, int *count_out);

#endif
