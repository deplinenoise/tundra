#ifndef TUNDRA_SCANNER_H
#define TUNDRA_SCANNER_H

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

struct lua_State;
struct td_node;
struct td_engine;
struct td_scanner;
struct td_file;
struct td_alloc;

#define TUNDRA_SCANNER_REF_MTNAME "tundra_scanner_ref"

typedef struct td_include_data_tag {
	const char *string;
	unsigned short string_len;
	unsigned char is_system_include;
	unsigned char should_follow;
} td_include_data;

struct td_scanner;

/* Callback to scan each line of the file. */
typedef int (td_scan_fn)(
		struct td_alloc *scratch,
		const char *start,
		td_include_data *dest,
		struct td_scanner *scanner);

typedef struct td_scanner {
	const char *ident;
	td_scan_fn *scan_fn;
	struct td_file **include_paths;
	int path_count;
} td_scanner;

typedef struct td_scanner_ref
{
	td_scanner *scanner;
} td_scanner_ref;

td_scanner *td_alloc_scanner(struct td_engine *engine, struct lua_State* L, int size);

#define td_check_scanner(L,index) (((td_scanner_ref*) luaL_checkudata(L, index, TUNDRA_SCANNER_REF_MTNAME))->scanner)

int td_scanner_open(struct lua_State *L);

int td_scan_includes(struct td_engine *engine, struct td_node *node, struct td_scanner *state);

#endif
