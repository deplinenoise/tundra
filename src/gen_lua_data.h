#ifndef TD_GEN_LUA_DATA_H
#define TD_GEN_LUA_DATA_H

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

typedef struct {
	const char *filename;
	const char *data;
	unsigned int size;
} td_lua_file;

#if defined(TD_STANDALONE)
extern const int td_lua_file_count;
extern const td_lua_file td_lua_files[];
#endif

#endif
