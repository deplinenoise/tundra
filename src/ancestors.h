#ifndef TD_ANCESTORS_H
#define TD_ANCESTORS_H

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
struct td_node;

/* Load ancestor data (populates the ancestors array). */
void
td_load_ancestors(struct td_engine *engine);

/* Save updated ancestor data. */
void
td_save_ancestors(struct td_engine *engine, struct td_node *root);

/* Compute node GUID and attach ancestor data to node. */
void
td_setup_ancestor_data(struct td_engine *engine, struct td_node *node);

#endif
