#ifndef TUNDRA_DEBUG_H
#define TUNDRA_DEBUG_H

struct lua_State;
struct td_node_tag;

void td_debug_dump(struct lua_State* L);

void td_dump_node(const struct td_node_tag *n, int level, int outer_index);

#endif
