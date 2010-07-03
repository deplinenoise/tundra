#include "engine.h"
#include "util.h"
#include "scanner.h"

#include <lua.h>
#include <lauxlib.h>
#include <stdio.h>

typedef struct td_cpp_scanner_tag
{
	td_scanner head;
	int path_count;
	const char **paths;
} td_cpp_scanner;

typedef struct
{
	/* files left to visit */
	int fqueue_size;
	int fqueue_max;
	td_file *fqueue;
	
} cpp_scan_queue;

static unsigned int relation_salt_cpp(const td_cpp_scanner *scanner)
{
	int i, count;
	unsigned int hash = 0;
	for (i = 0, count = scanner->path_count; i < count; ++i)
	{
		hash ^= djb2_hash(scanner->paths[i]);
	}
	return hash;
}

/* A simple c preprocessor #include scanner */

/*static int get_includes(td_file *file, const char ** */

static int scan_cpp(td_engine *engine, void *mutex, td_node *node, td_scanner *state)
{
	int i, count;
	td_cpp_scanner *self = (td_cpp_scanner *) state;
	unsigned int salt = relation_salt_cpp(self);
	(void) salt;

	for (i = 0, count = node->input_count; i < count; ++i)
	{
		td_file *input = node->inputs[i];

		FILE* f = fopen(input->filename, "r");
		if (!f)
			return 1;

		fclose(f);
	}

	return 1;
}

static int make_cpp_scanner(lua_State *L)
{
	td_engine *engine = td_check_engine(L, 1);
	td_cpp_scanner *self = (td_cpp_scanner *) td_alloc_scanner(L, sizeof(td_cpp_scanner));

	self->head.ident = "cpp";
	self->head.scan_fn = &scan_cpp;
	self->paths = td_build_string_array(L, &engine->alloc, 2, &self->path_count);

	return 1;
}

static const luaL_Reg cpp_scanner_entries[] = {
	{ "make_cpp_scanner", make_cpp_scanner },
	{ NULL, NULL },
};

int td_cpp_scanner_open(lua_State *L)
{
	luaL_newmetatable(L, TUNDRA_ENGINE_MTNAME);
	luaL_register(L, NULL, cpp_scanner_entries);
	lua_pop(L, 1);

	return 0;
}
