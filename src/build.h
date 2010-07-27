#ifndef TUNDRA_BUILD_H
#define TUNDRA_BUILD_H

struct td_engine;
struct td_node;

int
td_build(struct td_engine *engine, struct td_node *node, int *jobs_run);

#endif
