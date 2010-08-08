#ifndef TUNDRA_RELCACHE_H
#define TUNDRA_RELCACHE_H

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

#include <time.h>
#include "portable.h"

struct td_engine;
struct td_file;

struct td_file **
td_engine_get_relations(struct td_engine *engine, struct td_file *file, unsigned int salt, int *count_out);

void
td_engine_set_relations(struct td_engine *engine, struct td_file *file, unsigned int salt, int count, struct td_file **files);

void
td_save_relcache(struct td_engine *engine);

void
td_load_relcache(struct td_engine *engine);

void
td_relcache_cleanup(struct td_engine *engine);

#endif
