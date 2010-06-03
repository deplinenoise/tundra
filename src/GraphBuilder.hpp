#ifndef TUNDRA_GRAPHBUILDER_HPP
#define TUNDRA_GRAPHBUILDER_HPP

#include <vector>
#include <map>
#include <iosfwd>

#include <lua.h>

#include "Portable.hpp"
#include "Guid.hpp"
#include "Graph.hpp"

namespace tundra
{

enum CachePolicy
{
	CachePolicy_Allow,
	CachePolicy_Forbid
};

class GraphBuilder
{
public:
	GraphBuilder();
	~GraphBuilder();

private:
	typedef std::vector<Node*> NodeVec;
	typedef std::map<Guid, Node*> NodeCache;
	typedef std::map<const char*, File*, StringCompare> FileMap;

	NodeCache mCache;
	NodeVec mNonCached;
	FileMap mFiles;

public:
	void Load(std::istream&);
	void Save(std::ostream&);

public:
	Node* Construct(CachePolicy policy,
					NodeType type,
					const char* action,
					const char* annotation,
					const char **inputs,
					int inputCount,
					const char **outputs,
					int outputCount);
private:
	File* GetFile(const char* path);

private:
	GraphBuilder(const GraphBuilder&);
	GraphBuilder& operator=(const GraphBuilder&);

public:
	static void Export(lua_State*);
};

}

#endif
