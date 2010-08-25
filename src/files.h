#ifndef TD_FILES_H
#define TD_FILES_H

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

struct td_engine;
struct td_file;
struct td_stat;
struct td_digest;

typedef enum {
	TD_BORROW_STRING = 0,
	TD_COPY_STRING = 1,
} td_get_file_mode;

struct td_file *
td_engine_get_file(struct td_engine *engine, const char *path, td_get_file_mode mode);

const struct td_stat*
td_stat_file(struct td_engine *engine, struct td_file *f);

void
td_touch_file(struct td_engine *engine, struct td_file *f);

struct td_digest *
td_get_signature(struct td_engine *engine, struct td_file *f);

struct td_file *
td_parent_dir(struct td_engine *engine, struct td_file *f);


#endif