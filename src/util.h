#ifndef TUNDRA_UTIL_H
#define TUNDRA_UTIL_H

/*
   Copyright 2010 Andreas Fredriksson

   This file is part of Tundra.

   Tundra is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Tundra is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Tundra.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <stddef.h>

struct lua_State;
struct td_alloc;
struct td_engine;
struct td_file;

unsigned long djb2_hash(const char *str);

void td_croak(const char *fmt, ...);

void td_alloc_init(struct td_alloc *alloc, int page_count_max, int page_size);
void td_alloc_cleanup(struct td_alloc *alloc);

char *td_page_strdup(struct td_alloc *alloc, const char* str, size_t len);
char *td_page_strdup_lua(struct lua_State *L, struct td_alloc *alloc, int index, const char *context);
const char ** td_build_string_array(struct lua_State* L, struct td_alloc *alloc, int index, int *count_out);
struct td_file **td_build_file_array(struct lua_State* L, struct td_engine *alloc, int index, int *count_out);
const char *td_indent(int level);

typedef enum {
	TD_BUILD_REPLACE_NAME,
	TD_BUILD_CONCAT,
} td_build_path_mode;

void td_build_path(
		char *buffer,
		int buffer_size,
		const struct td_file *base,
		const char *subpath,
		int subpath_len,
		td_build_path_mode mode);

#endif
