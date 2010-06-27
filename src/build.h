#ifndef TUNDRA_BUILD_H
#define TUNDRA_BUILD_H

struct td_engine_tag;
struct td_node_tag;

int
td_build(struct td_engine_tag *engine, struct td_node_tag *node, int thread_count);

#endif
