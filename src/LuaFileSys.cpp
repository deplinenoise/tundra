
#include "LuaFileSys.hpp"
#include "LuaUtil.hpp"
#include "Portable.hpp"

#include <lua.h>
#include <lauxlib.h>

#include <list>
#include <vector>
#include <string>

struct PathWalkerState
{
	PathWalkerState()
		: directory_array_ref(LUA_NOREF)
	{}

	std::string prev_dir;
	std::list<std::string> work_queue;
	int directory_array_ref;
};

static PathWalkerState* check_walker_state(lua_State* L, int arg)
{
	void* ctx = (void*) luaL_checkudata(L, arg, "tundra.path_walker");
	luaL_argcheck(L, ctx != NULL, arg, "path walker object expected");
	return static_cast<PathWalkerState*>(ctx);
}

static int tundra_walk_path_iter(lua_State* L)
{
	PathWalkerState* const state = check_walker_state(L, 1);

	// Push the persistent directory table on the stack
	lua_rawgeti(L, LUA_REGISTRYINDEX, state->directory_array_ref);
	const int dir_stackpos = lua_gettop(L);

	// Push any directories from the last iteration onto the work queue
	for (int i=1, count=int(lua_objlen(L, -1)); i <= count; ++i)
	{
		lua_rawgeti(L, -1, i);
		if (lua_isnil(L, -1)) {
			lua_pop(L, 1);
			break;
		}
		size_t str_size;
		const char* str = luaL_checklstring(L, -1, &str_size);
		std::string path;
		path.reserve(state->prev_dir.size() + 1 + str_size);
		path.append(state->prev_dir);
		path.append("/");
		path.append(str, str+str_size);
		state->work_queue.push_back(path);
		lua_pop(L, 1);
	}

	// If there are no more directories to visit, we're done
	if (state->work_queue.empty())
		return 0;

	const std::string& directory = state->work_queue.front();
	state->prev_dir = directory;

	tundra::DirectoryEnumerator enumerator(directory.c_str());

	// If the dir doesn't exit, complain. luaL_error will propagate a C++
	// exception past here cleaning up the enumerator.
	if (!enumerator.IsValid())
		luaL_error(L, "%s: cannot enumerate directory", directory.c_str());

	// Push the path (first return value)
	lua_pushlstring(L, directory.c_str(), directory.size());

	// Push the dir table (second return value)
	lua_pushvalue(L, dir_stackpos);

	// Create a new table to stick files in (third return value)
	lua_newtable(L);

	int file_index = 1;
	int dir_index = 1;

	while (enumerator.MoveToNext())
	{
		bool isDir;
		const char* path = enumerator.GetPath(&isDir);
		lua_pushstring(L, path);
		// Put it in the file or dir table depending on type
		if (isDir)
			lua_rawseti(L, dir_stackpos, dir_index++);
		else
			lua_rawseti(L, -2, file_index++);
	}

	// terminate the directory list by setting the next index to nil
	lua_pushnil(L);
	lua_rawseti(L, dir_stackpos, dir_index);

	// Remove directory from work queue.
	state->work_queue.pop_front();

	// Return (path, dirs, files)
	return 3;
}

int tundra_pathwalker_gc(lua_State* L)
{
	PathWalkerState* const state = check_walker_state(L, 1);
	luaL_unref(L, LUA_REGISTRYINDEX, state->directory_array_ref);
	state->~PathWalkerState();
	return 0;
}

int tundra_walk_path(lua_State* L)
{
	size_t arglen;
	const char* arg = luaL_checklstring(L, 1, &arglen);

	lua_pushcfunction(L, tundra_walk_path_iter);

	void* stateMem = lua_newuserdata(L, sizeof(PathWalkerState));
	PathWalkerState* state = new (stateMem) PathWalkerState;
	luaL_getmetatable(L, "tundra.path_walker");
	lua_setmetatable(L, -2);

	// Create a new table and ref it to make it persist across calls.
	lua_newtable(L);
	state->directory_array_ref = luaL_ref(L, LUA_REGISTRYINDEX);

	state->work_queue.push_back(std::string(arg, arglen));
	lua_pushnil(L);
	return 3;
}


int tundra_stat_path(lua_State* L)
{
	long size = 0;
	unsigned char digest[16];
	const char* filename = luaL_checkstring(L, 1);

	FILE* f = fopen(filename, "rb");

	if (!f)
	{
		size = -1;
	}
	else
	{
		fseek(f, 0, SEEK_END);
		size = ftell(f);
		fseek(f, 0, SEEK_SET);
	}
	
	if (size < 0)
		memset(digest, 0, sizeof(digest));
	else
		tundra::CalculateDigest(f, digest, size);

	if (f)
		fclose(f);
		
	lua_remove(L, 1);
	lua_pushinteger(L, lua_Integer(size));
	tundra::PushMd5Digest(L, digest);
	return 2;
}

int tundra_normalize_path(lua_State* L)
{
	typedef std::pair<const char*, ptrdiff_t> segment_t;
	typedef std::vector<segment_t> segvec_t;
	typedef std::vector<char> input_t;

	input_t input;
	input.reserve(256);

	segvec_t segments;
	segments.reserve(32);

	// Concatenate all input strings.
	for (int i=1, count=lua_gettop(L); i <= count; ++i)
	{
		size_t size;
		const char* data = luaL_checklstring(L, i, &size);
		if (i > 1)
			input.push_back('/');
		input.insert(input.end(), data, data+size);
	}
	input.push_back(0);

	// Iterate the input string, looking for path separators. [segmentStart]
	// keeps track where the last path segment started. Note that this loop
	// extends to one beyond the string length (that is, the null terminator)
	// to process any segment following the last path separator (the
	// one-past-the-last character of the string is treated as if it were a
	// path separator.)

	const char* segmentStart = &input.front();

	for (const char* i=segmentStart; ; ++i)
	{
		const char ch = *i;

		// Check if we're at a separator (or the end of the string)
		const bool is_separator = (0 == ch) | ('/' == ch) | ('\\' == ch);

		if (is_separator)
		{
			const ptrdiff_t segmentSize = i - segmentStart;

			// Treat ".." as a pop from the segment stack if there is something to pop.
			if (2 == segmentSize && segmentStart[0] == '.' && segmentStart[1] == '.' && !segments.empty())
			{
				segments.pop_back();
			}

			// Collapse empty segments at the start of the path or between
			// double slashes, e.g. in foo//bar. Also collapse single
			// redundant single dots.
			else if (0 == segmentSize || (1 == segmentSize && segmentStart[0] == '.'))
			{
				// nop
			}

			else
			{
				// add the segment to the stack
				segments.push_back(std::make_pair(segmentStart, segmentSize));
			}

			// Prepare the next segment start
			segmentStart = i + 1;

			if (0 == ch)
				break; // We're done.
		}
	}

	// Now reassemble the resulting segments back to a string.
	luaL_Buffer buf;
	luaL_buffinit(L, &buf);
	for (segvec_t::const_iterator first=segments.begin(), i=first, e=segments.end(); i != e; ++i)
	{
		if (i != first)
			luaL_addchar(&buf, '/');
		luaL_addlstring(&buf, i->first, size_t(i->second));
	}
	luaL_pushresult(&buf);
	return 1;
}

