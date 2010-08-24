#ifndef TD_GEN_LUA_DATA_H
#define TD_GEN_LUA_DATA_H

typedef struct {
	const char *filename;
	const char *data;
	unsigned int size;
} td_lua_file;

extern const int td_lua_file_count;
extern const td_lua_file td_lua_files[];

#endif
