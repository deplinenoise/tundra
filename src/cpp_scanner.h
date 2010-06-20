#ifndef TUNDRA_CPP_SCANNER_H
#define TUNDRA_CPP_SCANNER_H

struct lua_State;

typedef struct td_cpp_scanner_cookie_tag
{
	int include_path_count;
	const char **include_path;
} td_cpp_scanner_cookie;

int td_cpp_scanner_open(lua_State *L);

int td_cpp_scanner(const struct td_node_tag *n, struct td_job_tag *job, void *cookie);

#endif
