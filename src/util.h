#ifndef TUNDRA_UTIL_H
#define TUNDRA_UTIL_H

#include <stddef.h>

struct lua_State;
struct td_alloc_tag;
struct td_engine_tag;
struct td_file_tag;

unsigned long djb2_hash(const char *str);

void td_croak(const char *fmt, ...);

void td_alloc_init(struct td_alloc_tag *alloc, int page_count_max, int page_size);
void td_alloc_cleanup(struct td_alloc_tag *alloc);

char *td_page_strdup(struct td_alloc_tag *alloc, const char* str, size_t len);
char *td_page_strdup_lua(struct lua_State *L, struct td_alloc_tag *alloc, int index, const char *context);
const char ** td_build_string_array(struct lua_State* L, struct td_alloc_tag *alloc, int index, int *count_out);
struct td_file_tag **td_build_file_array(struct lua_State* L, struct td_engine_tag *alloc, int index, int *count_out);
const char *td_indent(int level);

typedef enum {
	TD_BUILD_REPLACE_NAME,
	TD_BUILD_CONCAT,
} td_build_path_mode;

void td_build_path(
		char *buffer,
		int buffer_size,
		const struct td_file_tag *base,
		const char *subpath,
		int subpath_len,
		td_build_path_mode mode);

#endif
