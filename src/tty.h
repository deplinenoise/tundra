#ifndef TUNDRA_TTY_H
#define TUNDRA_TTY_H

/*
   Copyright 2010-2011 Andreas Fredriksson

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

int tty_init(void);

/* Emit data to the TTY - Either print directly, or buffer or failing that block.
 * - job_id The job printing
 * - is_stderr Whether to print to stdout or stderr
 * - sort_key Specifies sort order for buffering, caller should e.g. inc a number for each call
 * - data Data to emit, must be null terminated!
 * - len Length of data
 */
void tty_emit(int job_id, int is_stderr, int sort_key, const char *data, int len);

void tty_printf(int job_id, int sort_key, const char *format, ...);

void tty_job_exit(int job_id);

#endif
