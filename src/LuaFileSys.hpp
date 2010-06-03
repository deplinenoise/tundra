#ifndef TUNDRA_LUAFILESYS_H
#define TUNDRA_LUAFILESYS_H

struct lua_State;

int tundra_stat_path(lua_State* L);
int tundra_walk_path(lua_State* L);
int tundra_normalize_path(lua_State* L);
int tundra_pathwalker_gc(lua_State* L);


#endif
