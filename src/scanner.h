#ifndef TUNDRA_SCANNER_H
#define TUNDRA_SCANNER_H

struct lua_State;
struct td_node_tag;
struct td_engine_tag;
struct td_scanner_tag;

#define TUNDRA_SCANNER_REF_MTNAME "tundra_scanner_ref"

typedef int (*td_scan_fn)(struct td_engine_tag *engine, void *mutex, struct td_node_tag *node, struct td_scanner_tag *state);

typedef struct td_scanner_tag {
	const char *ident;
	td_scan_fn scan_fn;
} td_scanner;

typedef struct td_scanner_ref
{
	td_scanner *scanner;
} td_scanner_ref;

td_scanner *td_alloc_scanner(struct td_engine_tag *engine, struct lua_State* L, int size);

#define td_check_scanner(L,index) (((td_scanner_ref*) luaL_checkudata(L, index, TUNDRA_SCANNER_REF_MTNAME))->scanner)

int td_scanner_open(struct lua_State *L);

#endif
