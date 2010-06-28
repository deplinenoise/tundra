#ifndef TUNDRA_SCANNER_H
#define TUNDRA_SCANNER_H

struct lua_State;
struct td_node_tag;
struct td_engine_tag;
struct td_scanner_tag;

#define TUNDRA_SCANNER_MTNAME "tundra_scanner"

typedef int (*td_scan_fn)(struct td_engine_tag *engine, struct td_node_tag *node, struct td_scanner_tag *state);

typedef struct td_scanner_tag {
	const char *ident;
	td_scan_fn scan_fn;
} td_scanner;

td_scanner *td_alloc_scanner(struct lua_State* L, int size);

#define td_check_scanner(L,index) ((td_scanner*) luaL_checkudata(L, index, TUNDRA_SCANNER_MTNAME))

int td_scanner_open(struct lua_State *L);

#endif
