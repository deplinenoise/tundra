#include "util.h"
#include "engine.h"

#include <string.h>
#include <stdlib.h>

#include <lua.h>
#include <lauxlib.h>

void
td_croak(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
	exit(1);
}

unsigned long
djb2_hash(const char *str_)
{
	unsigned const char *str = (unsigned const char*) str_;
	unsigned long hash = 5381;
	int c;

	while (0 != (c = *str++))
	{
		hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
	}

	return hash;
}

char *
td_engine_strdup(td_engine *engine, const char *str, size_t len)
{
	char *addr = (char*) td_engine_alloc(engine, len + 1);

	memcpy(addr, str, len);

	/* rather than copying len+1, explicitly store a nul byte so strdup can
	 * work for substrings too. */
	addr[len] = '\0';

	return addr;
}

char *
td_engine_strdup_lua(lua_State *L, td_engine *engine, int index, const char *context)
{
	const char *str;
	size_t len;
	str = lua_tolstring(L, index, &len);
	if (!str)
		luaL_error(L, "%s: expected a string", context);
	return td_engine_strdup(engine, str, len);
}

const char **
td_build_string_array(lua_State *L, td_engine *engine, int index, int *count_out)
{
	int i;
	const int count = (int) lua_objlen(L, index);
	const char **result;

	*count_out = count;
	if (!count)
		return NULL;
   
	result = (const char **) td_engine_alloc(engine, sizeof(const char*) * count);

	for (i = 0; i < count; ++i)
	{
		lua_rawgeti(L, index, i+1);
		result[i] = td_engine_strdup_lua(L, engine, -1, "string array");
		lua_pop(L, 1);
	}

	return result;
}

td_file **
td_build_file_array(lua_State *L, td_engine *engine, int index, int *count_out)
{
	int i;
	const int count = (int) lua_objlen(L, index);
	td_file **result;

	*count_out = count;
	if (!count)
		return NULL;
   
	result = (td_file **) td_engine_alloc(engine, sizeof(td_file*) * count);

	for (i = 0; i < count; ++i)
	{
		lua_rawgeti(L, index, i+1);
		result[i] = td_engine_get_file(engine, lua_tostring(L, -1));
		lua_pop(L, 1);
	}

	return result;
}

const char *
td_indent(int level)
{
	static const char spaces[] =
		"                                                     "
		"                                                     "
		"                                                     ";

	int adjust = sizeof(spaces) - 1 - level * 2;
	if (adjust < 0)
		adjust = 0;

	return spaces + adjust;
}

