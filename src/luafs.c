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

#include <lua.h>
#include <lauxlib.h>

#include <string.h>
#include <assert.h>
#include <stdio.h>

#include "portable.h"
#include "engine.h"
#include "config.h"

#if defined(TUNDRA_WIN32)
#include <windows.h>
#elif defined(TUNDRA_UNIX)
#include <errno.h>
#include <sys/stat.h>
#include <dirent.h>
#endif

#if defined(TUNDRA_WIN32)
static void scan_directory(lua_State* L, const char* path)
{
	int i;
	HANDLE h;
	WIN32_FIND_DATAA find_data;
	char scan_path[MAX_PATH];

	_snprintf(scan_path, sizeof(scan_path), "%s/*", path);
	for (i=0; i<MAX_PATH; ++i)
	{
		char ch = scan_path[i];
		if ('/' == ch)
			scan_path[i] = '\\';
		else if ('\0' == ch)
			break;
	}

	/* FIXME: should protect handle for e.g. OOM termination by wrapping it in
	 * a userdata with gc metatable, or wrap pushing into a local function
	 * called with xpcall, but that's probably much slower. */
	h = FindFirstFileA(scan_path, &find_data);
	if (INVALID_HANDLE_VALUE == h)
		luaL_error(L, "no such directory: %s (error: %d)", path, GetLastError());

	lua_newtable(L);
	lua_newtable(L);

	do
	{
		if (0 != strcmp(".", find_data.cFileName) && 0 != strcmp("..", find_data.cFileName))
		{
			int target_table = -2;
			lua_pushstring(L, find_data.cFileName);

			if (FILE_ATTRIBUTE_DIRECTORY & find_data.dwFileAttributes)
				target_table = -3;

			lua_rawseti(L, target_table, (int) lua_objlen(L, target_table) + 1);
		}
	} while (FindNextFileA(h, &find_data));

	if (!FindClose(h))
		luaL_error(L, "%s: couldn't close file handle: %d", path, GetLastError());
}

#elif defined(TUNDRA_UNIX)

static void scan_directory(lua_State* L, const char* path)
{
	char full_fn[512];
	DIR* dir;
	struct stat statbuf;
	struct dirent entry;
	struct dirent* result;
	const size_t path_len = strlen(path);

	if (!(dir = opendir(path)))
		luaL_error(L, "can't opendir \"%s\": %s", path, strerror(errno));

	if (path_len + 1 > sizeof(full_fn))
	{
		fprintf(stderr, "%s: path\n", path);
		return;
	}

	strcpy(full_fn, path);

	lua_newtable(L);
	lua_newtable(L);

	while (0 == readdir_r(dir, &entry, &result) && result)
	{
		size_t namlen;
		int target_table;

		if (0 == strcmp(".", entry.d_name) || 0 == strcmp("..", entry.d_name))
			continue;

		if (strlen(entry.d_name) + path_len + 2 >= sizeof(full_fn))
		{
			fprintf(stderr, "%s: name too long\n", entry.d_name);
			continue;
		}

		full_fn[path_len] = '/';
		strcpy(full_fn + path_len + 1, entry.d_name);

		if (0 != stat(full_fn, &statbuf))
		{
			fprintf(stderr, "%s: couldn't stat file\n", full_fn);
			continue;
		}
#ifdef __APPLE__
		namlen = entry.d_namlen;
#else
		namlen = strlen(entry.d_name);
#endif
		lua_pushlstring(L, entry.d_name, namlen);

		target_table = (S_IFDIR & statbuf.st_mode) ? -3 : -2;
		lua_rawseti(L, target_table, (int) lua_objlen(L, target_table) + 1);
	}

	closedir(dir);
}
#endif

int walk_path_count = 0;
double walk_path_time = 0.0;

static void
walk_dirs(lua_State* L, int path_index, int callback_index)
{
	size_t dir_count;
	int dir_table, file_table;

	/* create a new table to store the work queue of directories in */
	lua_newtable(L);
	dir_table = lua_gettop(L);

	/* put the initial path in the dir table */
	lua_pushvalue(L, path_index);
	lua_rawseti(L, dir_table, 1);

	/* create a new table to store the resulting files in */
	lua_newtable(L);
	file_table = lua_gettop(L);

	/* loop until the directory stack has been exhausted */
	while (0 != (dir_count = lua_objlen(L, dir_table)))
	{
		int new_dir_table, new_file_table, path_index;
		size_t i, e;
		const char *path;

		/* pick off last dir and remove from stack */
		lua_rawgeti(L, dir_table, (int) dir_count);
		lua_pushnil(L);
		lua_rawseti(L, dir_table, (int) dir_count);

		path = lua_tostring(L, -1);
		path_index = lua_gettop(L);

		/* pushes two tables on the stack, files and directories */
		scan_directory(L, path);
		new_file_table = lua_gettop(L);
		new_dir_table = new_file_table - 1;

		/* sort the file table (in-place) so we get consistent results */
		lua_getglobal(L, "table");
		lua_getfield(L, -1, "sort");
		lua_pushvalue(L, new_file_table);
		lua_call(L, 1, 0);

		/* Call out with the results of this directory query to be saved with
		 * the DAG cache. When the DAG cache is read back in, the queries will
		 * re-run to make sure the cache is still valid. */
		td_call_cache_hook(L, &td_dirwalk_hook_key, path_index, new_file_table);

		/* see what directories to keep */
		for (i = 1, e = lua_objlen(L, new_dir_table); i <= e; ++i)
		{
			/* push three pieces <parent> </> <dir> in anticipation of success
			 * so we don't have to reshuffle the stack */
			lua_pushvalue(L, path_index);
			lua_pushstring(L, "/");
			lua_rawgeti(L, new_dir_table, (int) i);

			/* If there's a callback function to filter directories, call it
			 * now. Otherwise, just include everything. */
			if (callback_index)
			{
				lua_pushvalue(L, callback_index);
				lua_pushvalue(L, -2); /* push a copy of the new dir name */
	
				lua_call(L, 1, 1); /* stack -2, +1 */
				if (!lua_toboolean(L, -1))
				{
					lua_pop(L, 4);
					continue;
				}

				/* just pop the boolean */
				lua_pop(L, 1);
			}

			lua_concat(L, 3);
			lua_rawseti(L, dir_table, (int) (lua_objlen(L, dir_table) + 1));
		}

		/* add all files */
		for (i = 1, e = lua_objlen(L, new_file_table); i <= e; ++i)
		{
			/* push three pieces <parent> </> <dir> in anticipation of success
			 * so we don't have to reshuffle the stack */
			lua_pushvalue(L, path_index);
			lua_pushstring(L, "/");
			lua_rawgeti(L, new_file_table, (int) i);
			lua_concat(L, 3);
			lua_rawseti(L, file_table, (int) (lua_objlen(L, file_table) + 1));
		}

		/* pop the two new tables */
		lua_pop(L, 2);
	}

	lua_pushvalue(L, file_table);
}

int tundra_walk_path(lua_State* L)
{
	int dir_cb_index = 0;
	double t1;

	t1 = td_timestamp();

	if (lua_gettop(L) > 1)
	{
		luaL_checktype(L, 2, LUA_TFUNCTION);
		dir_cb_index = 2;
	}

	walk_dirs(L, 1, dir_cb_index);

	++walk_path_count;
	walk_path_time += td_timestamp() - t1;

	return 1;
}

