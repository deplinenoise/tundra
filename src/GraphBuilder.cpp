#include "GraphBuilder.hpp"
#include "StreamUtil.hpp"
#include "LuaUtil.hpp"
#include <memory>
#include <fstream>
#include <iostream>
#include <cassert>

#include <lualib.h>
#include <lauxlib.h>

namespace tundra
{

GraphBuilder::GraphBuilder()
{}

GraphBuilder::~GraphBuilder()
{
	for (NodeVec::iterator i=mNonCached.begin(), e=mNonCached.end(); i != e; ++i)
		delete *i;

	for (NodeCache::iterator i=mCache.begin(), e=mCache.end(); i != e; ++i)
		delete i->second;

	for (FileMap::iterator i=mFiles.begin(), e=mFiles.end(); i != e; ++i)
		delete i->second;
}

Node*
GraphBuilder::Construct(CachePolicy policy,
						NodeType type,
						const char* action,
						const char* annotation,
						const char **inputs,
						int inputCount,
						const char **outputs,
						int outputCount)
{
	Guid nodeId;
	Node::ComputeNodeId(type, action, annotation, inputs, inputCount, outputs, outputCount, &nodeId);

	if (CachePolicy_Allow == policy)
	{
		NodeCache::const_iterator i = mCache.find(nodeId);

		if (i != mCache.end())
		{
			Node* n = i->second;
			n->SetFlag(NodeFlag_Used);
			return n;
		}
	}

	std::vector<File*> inputFiles, outputFiles;
	inputFiles.reserve(inputCount);
	outputFiles.reserve(outputCount);

	for (int i=0; i<inputCount; ++i)
		inputFiles.push_back(GetFile(inputs[i]));

	for (int i=0; i<outputCount; ++i)
		outputFiles.push_back(GetFile(outputs[i]));

	std::auto_ptr<Node> n(new Node(type, action, annotation, inputCount > 0 ? &inputFiles[0] : 0, inputCount, outputCount ? &outputFiles[0] : 0, outputCount, nodeId));

	n->SetFlag(NodeFlag_Used);

	if (CachePolicy_Allow == policy)
	{
		mCache.insert(std::make_pair(nodeId, n.get()));
	}
	else
	{
		mNonCached.push_back(n.get());
	}

	for (int i=0; i<inputCount; ++i)
		inputFiles[i]->AddConsumer(n.get());
	for (int i=0; i<outputCount; ++i)
		outputFiles[i]->AddProducer(n.get());

	return n.release();
}

File* GraphBuilder::GetFile(const char* path)
{
	FileMap::iterator i = mFiles.find(path);
	if (mFiles.end() == i)
	{
		File* f = new File(path);
		mFiles.insert(std::make_pair(f->GetPath(), f));
		return f;
	}
	else
	{
		return i->second;
	}
}

void GraphBuilder::Load(std::istream& is)
{
	while (1)
	{
		is >> std::ws;
		if (is.eof())
			break;

		Guid id;
		NodeType type;
		std::string action;
		std::string annotation;
		std::vector<std::string> inputFiles, outputFiles;
		std::vector<const char*> inputFilesC, outputFilesC;

		is >> id >> type >> UnescapeString(action) >> UnescapeString(annotation);

		int inputCount;
		is >> inputCount;
		inputFiles.resize(inputCount);
		inputFilesC.resize(inputCount);
		for (int i=0; i<inputCount; ++i)
		{
			is >> UnescapeString(inputFiles[i]);
			inputFilesC[i] = inputFiles[i].c_str();
		}

		int outputCount;
		is >> outputCount;
		outputFiles.resize(outputCount);
		outputFilesC.resize(outputCount);
		for (int i=0; i<outputCount; ++i)
		{
			is >> UnescapeString(outputFiles[i]);
			outputFilesC[i] = outputFiles[i].c_str();
		}

		Node* n = Construct(
			CachePolicy_Allow,
			type,
			action.c_str(),
			annotation.c_str(),
			inputCount ? &inputFilesC[0] : 0, inputCount,
			outputCount ? &outputFilesC[0] : 0, outputCount);

		assert(n->GetId() == id);

		int depCount;
		is >> depCount;
		for (int i=0; i<depCount; ++i)
		{
			Guid depId;
			is >> depId;
			NodeCache::iterator iter = mCache.find(depId);
			if (iter != mCache.end())
				n->AddDependency(iter->second);
			else
				throw std::runtime_error("dependency not found in cache (cycle in input?)"); 
		}
	}
}

void GraphBuilder::Save(std::ostream& o)
{
	for (NodeCache::iterator i=mCache.begin(), e=mCache.end(); i != e; ++i)
	{
		Node* const node = i->second;
		node->Persist(o);
	}
}

static inline GraphBuilder* check_graphbuilder(lua_State* L, int index)
{
	return (GraphBuilder*) luaL_checkudata(L, index, "tundra.graphbuilder");
}

static inline Node* check_node(lua_State* L, int index)
{
	return *((Node**) luaL_checkudata(L, index, "tundra.node"));
}

static int tundra_graphbuilder_new(lua_State* L)
{
	void* ptr = lua_newuserdata(L, sizeof(GraphBuilder));

	GraphBuilder* const self = new (ptr) GraphBuilder;
	(void) self;

	luaL_getmetatable(L, "tundra.graphbuilder");
	assert(!lua_isnil(L, -1));
	lua_setmetatable(L, -2);
	return 1;
}

static int tundra_graphbuilder_gc(lua_State* L)
{
	GraphBuilder* const self = check_graphbuilder(L, 1);
	self->~GraphBuilder();
	return 0;
}

static int tundra_graphbuilder_load(lua_State* L)
{
	GraphBuilder* const self = check_graphbuilder(L, 1);
	const char* fn = luaL_checkstring(L, 2);
	try
	{
		std::ifstream f(fn);
		if (!f.good())
			luaL_error(L, "Couldn't open file %s for reading", fn);
		f.exceptions(std::ios_base::badbit | std::ios_base::failbit);
		self->Load(f);
	}
	catch (std::exception& ex)
	{
		luaL_error(L, "Couldn't load %s: %s", fn, ex.what());
	}
	return 0;
}

static int tundra_graphbuilder_save(lua_State* L)
{
	GraphBuilder* const self = check_graphbuilder(L, 1);
	const char* fn = luaL_checkstring(L, 2);
	std::ofstream f(fn);
	if (!f.good())
		luaL_error(L, "Couldn't open file %s for writing", fn);
	f.exceptions(std::ios_base::badbit | std::ios_base::failbit);
	self->Save(f);
	return 0;
}

static int tundra_graphbuilder_get_node(lua_State* L)
{
	GraphBuilder* const self = check_graphbuilder(L, 1);
	luaL_checktype(L, 2, LUA_TTABLE);

	NodeType nodeType = NodeType_ShellAction;
	CachePolicy cachePolicy = CachePolicy_Forbid;
	const char* action = 0;
	const char* annotation = 0;

	typedef std::vector<const char*> SVec;
	SVec inputs, outputs;

	lua_pushnil(L);
	while (lua_next(L, 2))
	{
		// key now at -2, value at -1
		const char* const key = luaL_checkstring(L, -2);

		if (0 == strcmp(key, "Inputs") || 0 == strcmp(key, "Outputs"))
		{
			SVec& target = 'I' == key[0] ? inputs : outputs;
			luaL_checktype(L, -1, LUA_TTABLE);
			for (int i=1, e=(int) lua_objlen(L, -1); i <= e; ++i)
			{
				lua_rawgeti(L, -1, i);
				target.push_back(lua_tostring(L, -1));
				lua_pop(L, 1);
			}
		}
		else if (0 == strcmp(key, "Action"))
		{
			action = lua_tostring(L, -1);
		}
		else if (0 == strcmp(key, "Annotation"))
		{
			annotation = lua_tostring(L, -1);
		}
		else if (0 == strcmp(key, "Cacheable"))
		{
			if (lua_toboolean(L, -1) != 0)
				cachePolicy = CachePolicy_Allow;
		}
		else if (0 == strcmp(key, "Type"))
		{
			const char* typeName = lua_tostring(L, -1);
			if (0 == strcmp(typeName, "GraphGenerator"))
				nodeType = NodeType_GraphGenerator;
			else if (0 == strcmp(typeName, "ShellAction"))
				nodeType = NodeType_ShellAction;
			else
				luaL_error(L, "Unknown node type: %s", typeName);
		}
		else
		{
			luaL_error(L, "Unknown node construction key: %s", key);
		}

		// pop value, leave key for luaL_next
		lua_pop(L, 1);
	}

	if (!action && !annotation)
		luaL_error(L, "At least one of Action and Annotation must be specified");

	if (!action)
		action = "";
	if (!annotation)
		annotation = "";

	Node** slot = (Node**) lua_newuserdata(L, sizeof(Node*));

	*slot = self->Construct(
		cachePolicy,
		nodeType,
		action,
		annotation,
		inputs.empty() ? 0 : &inputs[0], int(inputs.size()),
		outputs.empty() ? 0 : &outputs[0], int(outputs.size()));

	luaL_getmetatable(L, "tundra.node");
	assert(!lua_isnil(L, -1));
	lua_setmetatable(L, -2);
	return 1;
}

static int tundra_node_add_dependency(lua_State* L)
{
	Node* const self = check_node(L, 1);

	for (int i=2, e=lua_gettop(L); i <= e; ++i)
	{
		Node* const other = check_node(L, i);
		self->AddDependency(other);
	}
	return 0;
}

static int tundra_node_get_action(lua_State* L)
{
	Node* const self = check_node(L, 1);
	lua_pushstring(L, self->GetAction());
	return 1;
}

static int tundra_node_get_annotation(lua_State* L)
{
	Node* const self = check_node(L, 1);
	lua_pushstring(L, self->GetAnnotation());
	return 1;
}

static int tundra_node_get_type(lua_State* L)
{
	Node* const self = check_node(L, 1);
	const char* type;
	switch (self->GetType())
	{
	case NodeType_GraphGenerator:
		type = "GraphGenerator";
		break;
	case NodeType_ShellAction:
		type = "ShellAction";
		break;
	default:
		type = "Unknown";
		break;
	}
	lua_pushstring(L, type);
	return 1;
}

static int tundra_node_get_id(lua_State* L)
{
	Node* const self = check_node(L, 1);
	return PushMd5Digest(L, self->GetId().GetHash());
}

static int tundra_node_iter_input_files_i(lua_State* L)
{
	Node* const self = check_node(L, 1);

	// Get the current index and bump it.
	int index = (int) lua_tointeger(L, lua_upvalueindex(1));

	int const limit = self->GetInputFileCount();
	if (index >= limit)
		return 0;

	lua_pushinteger(L, index + 1);
	lua_replace(L, lua_upvalueindex(1));

	lua_pushstring(L, self->GetInputFile(index)->GetPath());
	return 1;
}

static int tundra_node_iter_output_files_i(lua_State* L)
{
	Node* const self = check_node(L, 1);

	// Get the current index and bump it.
	int index = (int) lua_tointeger(L, lua_upvalueindex(1));

	int const limit = self->GetOutputFileCount();
	if (index >= limit)
		return 0;

	lua_pushinteger(L, index + 1);
	lua_replace(L, lua_upvalueindex(1));

	lua_pushstring(L, self->GetOutputFile(index)->GetPath());
	return 1;
}

static int tundra_node_iter_input_files(lua_State* L)
{
	// Keep the array index as an upvalue; the generator will only return the file names.
	lua_pushinteger(L, 0);
	lua_pushcclosure(L, tundra_node_iter_input_files_i, 1);
	lua_pushvalue(L, 1); // dup the node (self) parameter as the state
	lua_pushnumber(L, 0);
	return 3;
}

static int tundra_node_iter_output_files(lua_State* L)
{
	// Keep the array index as an upvalue; the generator will only return the file names.
	lua_pushinteger(L, 0);
	lua_pushcclosure(L, tundra_node_iter_output_files_i, 1);
	lua_pushvalue(L, 1); // dup the node (self) parameter as the state
	lua_pushnumber(L, 0);
	return 3;
}

static int tundra_node_get_input_files(lua_State* L)
{
	Node* const self = check_node(L, 1);
	lua_newtable(L);
	for (int i=0, e=self->GetInputFileCount(); i != e; ++i)
	{
		lua_pushstring(L, self->GetInputFile(i)->GetPath());
		lua_rawseti(L, -2, i+1);
	}
	return 1;
}

static int tundra_node_get_output_files(lua_State* L)
{
	Node* const self = check_node(L, 1);
	lua_newtable(L);
	for (int i=0, e=self->GetOutputFileCount(); i != e; ++i)
	{
		lua_pushstring(L, self->GetOutputFile(i)->GetPath());
		lua_rawseti(L, -2, i+1);
	}
	return 1;
}

void GraphBuilder::Export(lua_State* L)
{
	static const luaL_Reg node_mt_entries[] = {
		{ "AddDependency", tundra_node_add_dependency },
		{ "GetAction", tundra_node_get_action },
		{ "GetAnnotation", tundra_node_get_annotation },
		{ "GetType", tundra_node_get_type },
		{ "GetId", tundra_node_get_id },
		{ "GetInputFiles", tundra_node_get_input_files },
		{ "GetOutputFiles", tundra_node_get_output_files },
		{ "IterInputFiles", tundra_node_iter_input_files },
		{ "IterOutputFiles", tundra_node_iter_output_files },
		{ NULL, NULL }
	};

	// Set up metatable for node objects
	luaL_newmetatable(L, "tundra.node");
	lua_pushstring(L, "__index");
	lua_pushvalue(L, -2);
	lua_settable(L, -3);
	luaL_register(L, NULL, node_mt_entries);
	lua_pop(L, 1);

	static const luaL_Reg gb_mt_entries[] = {
		{ "Load", tundra_graphbuilder_load },
		{ "Save", tundra_graphbuilder_save },
		{ "GetNode", tundra_graphbuilder_get_node },
		{ "__gc", tundra_graphbuilder_gc },
		{ NULL, NULL }
	};

	// Set up metatable for graph builder objects
	luaL_newmetatable(L, "tundra.graphbuilder");
	lua_pushstring(L, "__index");
	lua_pushvalue(L, -2);
	lua_settable(L, -3);
	luaL_register(L, NULL, gb_mt_entries);
	lua_pop(L, 1);

	// Register creation function
	static const luaL_Reg graph_entries[] = {
		{ "New", tundra_graphbuilder_new },
		{ NULL, NULL }
	};

	luaL_register(L, "tundra.native.graph", graph_entries);
	lua_pop(L, 1);
}

}
