#ifndef TUNDRA_NODE_HPP
#define TUNDRA_NODE_HPP

#include "Guid.hpp"

#include <iosfwd>
#include <vector>

namespace tundra
{

class File;
class Action;
class Scanner;

enum NodeFlag
{
	NodeFlag_Cached			= 1 << 0,
	NodeFlag_Used			= 1 << 1,
	NodeFlag_Persisted		= 1 << 30,
};

class Node
{
private:
	friend class GraphBuilder;

private:
	typedef std::vector<File*> FileVec;
	typedef std::vector<Node*> NodeVec;

private:
	int mFlags;
	const Guid mId;
	Action* mAction;
	Scanner* mScanner;
	const std::string mAnnotation;
	FileVec mInputFiles;
	FileVec mOutputFiles;
	NodeVec mDependencies;

private:
	Node(Action* action,
		int inputFileCount,
		File** inputFiles,
		int outputFileCount,
		File** outputFiles,
		const Guid& id);

public:
	~Node();

public:
	const Guid& GetId() const;
	Action* GetAction() const;
	const char* GetAnnotation() const;

public:
	int GetFlags() const;
	bool TestFlag(NodeFlag f) const;
	void SetFlags(int v);
	void SetFlag(NodeFlag f);
	void ClearFlag(NodeFlag f);

public:
	void AddDependency(Node* node);
	int GetDependencyCount() const;
	Node* GetDependency(int idx) const;

public:
	Scanner* GetScanner() const;
	void SetScanner(Scanner* scanner);

public:
	int GetInputFileCount() const;
	File* GetInputFile(int index) const;

public:
	int GetOutputFileCount() const;
	File* GetOutputFile(int index) const;

public:
	Node(const Node&);
	Node& operator=(const Node&);

private:
	static void ComputeNodeId(
		Guid* out,
		Action* action,
		int inputFileCount,
		File** inputFiles,
		int outputFileCount,
		File** outputFiles);

public:
	void Persist(std::ostream& stream);
};

}

#endif
