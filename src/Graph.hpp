#ifndef TUNDRA_GRAPH_HPP
#define TUNDRA_GRAPH_HPP

#include <string>
#include <vector>
#include <map>
#include <iosfwd>
#include <set>

#include "Guid.hpp"

namespace tundra
{

enum NodeType
{
	NodeType_ShellAction,
	NodeType_GraphGenerator
};

std::ostream& operator<<(std::ostream&, NodeType);
std::istream& operator>>(std::istream&, NodeType&);

class Node;

class File
{
private:
	const std::string mPath;
	std::vector<Node*> mConsumers;
	std::vector<Node*> mProducers;

public:
	explicit File(const char* path);
	~File();

private:
	File(const File&);
	File& operator=(const File&);
	
public:
	void AddConsumer(Node*);
	void AddProducer(Node*);

public:
	const char* GetPath() const { return mPath.c_str(); }
};

enum NodeFlag
{
	NodeFlag_Cached			= 1 << 0,
	NodeFlag_Used			= 1 << 1,
	NodeFlag_Persisted		= 1 << 30,
};

class Node
{
private:
	typedef std::vector<File*> FileVec;
	typedef std::vector<Node*> NodeVec;

private:
	int mFlags;
	const Guid mId;
	const NodeType mType;
	const std::string mAction;
	const std::string mAnnotation;
	const FileVec mInputFiles;
	const FileVec mOutputFiles;
	NodeVec mDependencies;

public:
	Node(NodeType t,
		const char* action,
		const char* annotation,
		File** inputFiles,
		int inputFileCount,
		File** outputFiles,
		int outputFileCount,
		const Guid& id);

	~Node();

public:
	int GetFlags() const { return mFlags; }
	bool TestFlag(NodeFlag f) { return 0 != (mFlags & f); }
	void SetFlags(int v) { mFlags = v; }
	void SetFlag(NodeFlag f) { mFlags |= f; }
	void ClearFlag(NodeFlag f) { mFlags &= ~f; }

public:
	void AddDependency(Node* node);
	int GetDependencyCount() const { return int(mDependencies.size()); }
	Node* GetDependency(int idx) const { return mDependencies[idx]; }

public:
	int GetInputFileCount() const { return int(mInputFiles.size()); }
	File* GetInputFile(int index) const { return mInputFiles[index]; }

public:
	int GetOutputFileCount() const { return int(mOutputFiles.size()); }
	File* GetOutputFile(int index) const { return mOutputFiles[index]; }

public:
	NodeType GetType() const { return mType; }
	const Guid& GetId() const { return mId; }
	const char* GetAction() const { return mAction.c_str(); }
	const char* GetAnnotation() const { return mAnnotation.c_str(); }

public:
	Node(const Node&);
	Node& operator=(const Node&);

public:
	static void ComputeNodeId(
		NodeType t,
		const char* action,
		const char* annotation,
		const char** inputFiles,
		int inputFileCount,
		const char** outputFiles,
		int outputFileCount,
		Guid* out);

public:
	void Persist(std::ostream& stream);
};

}

#endif
