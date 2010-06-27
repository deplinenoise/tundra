#ifndef TUNDRA_UTIL_H
#define TUNDRA_UTIL_H

#include <stddef.h>

struct lua_State;
struct td_engine_tag;
struct td_file_tag;

unsigned long djb2_hash(const char *str);

void td_croak(const char *fmt, ...);

char *td_engine_strdup(struct td_engine_tag *engine, const char* str, size_t len);
char *td_engine_strdup_lua(struct lua_State *L, struct td_engine_tag *engine, int index, const char *context);
const char ** td_build_string_array(struct lua_State* L, struct td_engine_tag *engine, int index, int *count_out);
struct td_file_tag **td_build_file_array(struct lua_State* L, struct td_engine_tag *engine, int index, int *count_out);
const char *td_indent(int level);

#endif
