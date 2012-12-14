#ifndef TUNDRA_BUILD_H
#define TUNDRA_BUILD_H

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

#define TD_MAX_THREADS (32)

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
