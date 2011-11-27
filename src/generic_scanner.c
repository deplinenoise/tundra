/*
   Copyright 2011 Andreas Fredriksson

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

#include "engine.h"
#include "util.h"
#include "scanner.h"

#include <ctype.h>
#include <lua.h>
#include <lauxlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

/* A scanner that can be configured to handle simple include patterns */

enum {
	MAX_KEYWORDS = 8,
	KWSET_FOLLOW = 0, /* for source files */
	KWSET_NOFOLLOW = 1,	/* for binary files */
	KWSET_COUNT = 2
};

typedef struct {
	int keyword_count;
	const char *keywords[MAX_KEYWORDS];
	int keyword_lens[MAX_KEYWORDS];
} keyword_set;

typedef struct {
	td_scanner base;

	/* if true, line must start with whitespace; required by some assembler syntaxes */
	int require_whitespace;
	/* if true, look for " or < separators */
	int use_separators;
	/* if true, and use_separators=0, use system include path for everything picked up */
	int bare_means_system;

	keyword_set kwsets[KWSET_COUNT];

} generic_scanner;

static int
generic_scan_line(td_alloc *scratch, const char *start_in, td_include_data *dest, td_scanner *scanner_)
{
	generic_scanner *scanner = (generic_scanner*) scanner_;
	char separator;
	int kw, kwcount, kwset;
	const char *start = start_in;
	const char *str_start;

	while (isspace(*start))
		++start;

	if (scanner->require_whitespace && start == start_in)
		return 0;

	for (kwset = 0; kwset < KWSET_COUNT; ++kwset)
	{
		const keyword_set *set = &scanner->kwsets[kwset];
		for (kw = 0, kwcount = set->keyword_count; kw < kwcount; ++kw)
		{
			if (0 == strncmp(set->keywords[kw], start, set->keyword_lens[kw]))
				break;
		}

		if (kwcount != kw)
			break;
	}

	if (kwset == KWSET_COUNT)
		return 0;

	start += scanner->kwsets[kwset].keyword_lens[kw];
	
	if (!isspace(*start++))
		return 0;

	while (isspace(*start))
		++start;

	if (scanner->use_separators)
	{
		separator = *start++;
		if ('<' == separator)
		{
			dest->is_system_include = 1;
			separator = '>';
		}
		else
			dest->is_system_include = 0;

		str_start = start;
		for (;;)
		{
			char ch = *start++;
			if (ch == separator)
				break;
			if (!ch)
				return 0;
		}
	}
	else
	{
		const char *p = start;

		/* just grab the next token */
		while (*start && !isspace(*start))
			++start;

		if (p == start)
			return 0;

		dest->is_system_include = scanner->bare_means_system;
	}

	dest->string_len = (unsigned short) (start - str_start - 1);
	dest->string = td_page_strdup(scratch, str_start, dest->string_len);
	dest->should_follow = kwset == KWSET_FOLLOW;
	return 1;
}

static void setup_kwset(lua_State *L, td_engine *engine, const char *key, keyword_set *dest)
{
	int i, numkw;

	lua_getfield(L, 3, key);
	if (lua_isnil(L, -1))
		luaL_error(L, "expected %s parameter");
	
	numkw = (int) lua_objlen(L, -1);

	if (numkw <= 0 || numkw > MAX_KEYWORDS)
		luaL_error(L, "%s: need between 1 and %d keywords, got %d", key, MAX_KEYWORDS, numkw);

	dest->keyword_count = numkw;

	for (i = 0; i < numkw; ++i)
	{
		size_t len;
		const char *txt;
		lua_rawgeti(L, -1, i + 1);
		txt = lua_tolstring(L, -1, &len);
		dest->keywords[i] = td_page_strdup(&engine->alloc, txt, (int)len);
		dest->keyword_lens[i] = (int) len;
		lua_pop(L, 1);
	}

	lua_pop(L, 1);
}

static int make_generic_scanner(lua_State *L)
{
	td_engine *engine = td_check_engine(L, 1);
	generic_scanner *self = (generic_scanner *) td_alloc_scanner(engine, L, sizeof(generic_scanner));

	self->base.ident = "generic";
	self->base.scan_fn = &generic_scan_line;
	self->base.include_paths = td_build_file_array(L, engine, 2, &self->base.path_count);

	setup_kwset(L, engine, "Keywords", &self->kwsets[KWSET_FOLLOW]);
	setup_kwset(L, engine, "KeywordsNoFollow", &self->kwsets[KWSET_NOFOLLOW]);

	self->require_whitespace = td_get_int_override(L, 3, "RequireWhitespace", 0);
	self->use_separators = td_get_int_override(L, 3, "UseSeparators", 0);
	self->bare_means_system = td_get_int_override(L, 3, "BareMeansSystem", 0);

	td_call_cache_hook(L, &td_scanner_hook_key, 2, lua_gettop(L));

	return 1;
}

static const luaL_Reg generic_scanner_entries[] = {
	{ "make_generic_scanner", make_generic_scanner },
	{ NULL, NULL },
};

int td_generic_scanner_open(lua_State *L)
{
	luaL_newmetatable(L, TUNDRA_ENGINE_MTNAME);
	luaL_register(L, NULL, generic_scanner_entries);
	lua_pop(L, 1);
	return 0;
}
