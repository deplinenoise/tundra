
#include <lua.h>
#include <lauxlib.h>

#include <string.h>
#include <assert.h>
#include <stdio.h>

#ifdef _WIN32
#include <windows.h>
#elif defined(__APPLE__) || defined(linux)
#include <sys/stat.h>
#include <dirent.h>
#else
#error jaha du
#endif

#ifdef _WIN32
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
#endif

#if defined(__APPLE__) || defined(linux)
static void scan_directory(lua_State* L, const char* path)
{
	char full_fn[512];
	DIR* dir;
	struct stat statbuf;
	struct dirent entry;
	struct dirent* result;
	const size_t path_len = strlen(path);

	if (!(dir = opendir(path)))
		return;

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

		lua_pushlstring(L, entry.d_name, entry.d_namlen);

		int target_table = (S_IFDIR & statbuf.st_mode) ? -3 : -2;
		lua_rawseti(L, target_table, (int) lua_objlen(L, target_table) + 1);
	}

	closedir(dir);
}
#endif

enum
{
	WALK_UPV_CWD = lua_upvalueindex(1),
	WALK_UPV_DIRS = lua_upvalueindex(2),
	WALK_UPV_QUEUE = lua_upvalueindex(3)
};

static int tundra_walk_path_iter(lua_State* L)
{
	const char* current_dir = "";
	const char* path = ""; 

	if (!lua_isnil(L, WALK_UPV_CWD))
		current_dir = lua_tostring(L, WALK_UPV_CWD);

	/* push any directories from the last iteration onto the work queue, we are
	 * going to enter into these directories */
	{
		int i;
		int count = (int) lua_objlen(L, WALK_UPV_DIRS);
		for (i=1; i <= count; ++i)
		{
			/*
			 * this loop does the equivalent of
			 *
			 * work_queue[#work_queue+1] = prev_dir .. '/' .. dirs[i]
			 */
			lua_pushstring(L, current_dir);

			/* try to fetch the next directory in the directory array */
			lua_rawgeti(L, WALK_UPV_DIRS, i);

			if (lua_isnil(L, -1)) {
				lua_pop(L, 2);
				break;
			}

			/* concatenate parent path with directory name to yield new path */
			lua_concat(L, 2);

			/* stick the new path in the work queue */
			lua_rawseti(L, WALK_UPV_QUEUE, (int) lua_objlen(L, WALK_UPV_QUEUE) + 1);
		}
	}

	/* if there are no more directories to visit, we're done */
	if (0 == lua_objlen(L, WALK_UPV_QUEUE))
		return 0;

	/* retval #1: the directory; pick the first directory of the work queue */
	lua_rawgeti(L, WALK_UPV_QUEUE, 1);
	path = lua_tostring(L, -1);

	/* set the directory (with a trailing slash) as the new current directory upvalue */
	lua_pushvalue(L, -1);
	lua_pushstring(L, "/");
	lua_concat(L, 2);
	lua_replace(L, WALK_UPV_CWD);

	/* pushes two tables on the stack, files and directories */
	scan_directory(L, path);

	/* use this directory list next time */
	lua_pushvalue(L, -2);
	lua_replace(L, WALK_UPV_DIRS);

	/* we're done with this directory, so drop it off the work queue,
	 * unfortunately this being lua we have to shift down all array members */
	{
		int i;
		int count = (int) lua_objlen(L, WALK_UPV_QUEUE);
		for (i=1, count; i < count; ++i)
		{
			lua_rawgeti(L, WALK_UPV_QUEUE, i+1);
			lua_rawseti(L, WALK_UPV_QUEUE, i);
		}

		/* terminate array with a nil */
		lua_pushnil(L);
		lua_rawseti(L, WALK_UPV_QUEUE, i);
	}

	return 3;
}

int tundra_walk_path(lua_State* L)
{
	/* upvalue 1: parent directory path */
	lua_pushnil(L);

	/* upvalue 2: work queue = { arg1 } */
	lua_newtable(L);
	lua_pushvalue(L, 1);
	lua_rawseti(L, -2, 1);

	/* upvalue 3: directories = {} */
	lua_newtable(L);

	lua_pushcclosure(L, tundra_walk_path_iter, 3);
	return 1;
}

