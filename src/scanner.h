#ifndef TUNDRA_SCANNER_H
#define TUNDRA_SCANNER_H

struct lua_State;
struct td_node;
struct td_engine;
struct td_scanner;

#define TUNDRA_SCANNER_REF_MTNAME "tundra_scanner_ref"

typedef int (*td_scan_fn)(struct td_engine *engine, void *mutex, struct td_node *node, struct td_scanner *state);

typedef struct td_scanner {
	const char *ident;
	td_scan_fn scan_fn;
} td_scanner;

typedef struct td_scanner_ref
{
	td_scanner *scanner;
} td_scanner_ref;

td_scanner *td_alloc_scanner(struct td_engine *engine, struct lua_State* L, int size);

#define td_check_scanner(L,index) (((td_scanner_ref*) luaL_checkudata(L, index, TUNDRA_SCANNER_REF_MTNAME))->scanner)

int td_scanner_open(struct lua_State *L);

#endif
