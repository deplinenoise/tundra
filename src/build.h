#ifndef TUNDRA_BUILD_H
#define TUNDRA_BUILD_H

struct td_engine;
struct td_node;

typedef enum {
	TD_BUILD_SUCCESS,
	TD_BUILD_FAILED,
	TD_BUILD_ABORTED
} td_build_result;

extern const char * const td_build_result_names[];

td_build_result
td_build(struct td_engine *engine, struct td_node *node, int *jobs_run);

#endif
