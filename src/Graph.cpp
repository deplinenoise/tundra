#include "Graph.hpp"
#include "Hasher.hpp"
#include "StreamUtil.hpp"
#include <istream>
#include <ostream>
#include <cassert>
#include <stdexcept>
#include <algorithm>

namespace tundra
{

std::ostream& operator<<(std::ostream& o, NodeType t)
{
	switch (t)
	{
	case NodeType_GraphGenerator: return o << "GraphGenerator";
	case NodeType_ShellAction: return o << "ShellAction";
	}
	throw std::runtime_error("illegal node type");
}

std::istream& operator>>(std::istream& i, NodeType& result)
{
	char kw[32];
	i.width(sizeof kw);
	if (i >> kw)
	{
		if (0 == strcmp(kw, "ShellAction"))
			result = NodeType_ShellAction;
		else if (0 == strcmp(kw, "GraphGenerator"))
			result = NodeType_GraphGenerator;
		else
			i.setstate(std::ios_base::failbit);
	}
	else
	{
		i.setstate(std::ios_base::failbit);
	}
	return i;
}

Node::Node(NodeType type,
	const char* action,
	const char* annotation,
	File** inputFiles,
	int inputFileCount,
	File** outputFiles,
	int outputFileCount,
	const Guid& id)
: mFlags(0)
, mId(id)
, mType(type)
, mAction(action)
, mAnnotation(annotation)
, mInputFiles(&inputFiles[0], &inputFiles[inputFileCount])
, mOutputFiles(&outputFiles[0], &outputFiles[outputFileCount])
{
}

Node::~Node()
{}

void Node::ComputeNodeId(
	NodeType type,
	const char* action,
	const char* annotation,
	const char** inputFiles,
	int inputFileCount,
	const char** outputFiles,
	int outputFileCount,
	Guid* out)
{
	Hasher h;

	switch (type)
	{
		case NodeType_ShellAction:
			h.AddString("S");
			break;
		case NodeType_GraphGenerator:
			h.AddString("G");
			break;
		default:
			assert(0);
			break;
	}

	h.AddString(action);

	h.AddString(annotation);

	for (int i=0; i < inputFileCount; ++i)
	{
		h.AddString("I");
		h.AddString(inputFiles[i]);
	}
	for (int i=0; i < outputFileCount; ++i)
	{
		h.AddString("O");
		h.AddString(outputFiles[i]);
	}

	h.GetDigest(out);
}

void Node::Persist(std::ostream& stream)
{
	if (TestFlag(NodeFlag_Persisted))
		return;
	SetFlag(NodeFlag_Persisted);

	for (int i=0, count=GetDependencyCount(); i != count; ++i)
		GetDependency(i)->Persist(stream);

	stream << mId << " " << mType << " ";
	stream << EscapeString(mAction) << " " << EscapeString(mAnnotation) << "\n";
	stream << mInputFiles.size() << " ";
	for (FileVec::const_iterator i=mInputFiles.begin(), e=mInputFiles.end(); i != e; ++i)
		stream << EscapeString((*i)->GetPath()) << " ";
	stream << "\n" << mOutputFiles.size() << " ";
	for (FileVec::const_iterator i=mOutputFiles.begin(), e=mOutputFiles.end(); i != e; ++i)
		stream << EscapeString((*i)->GetPath()) << " ";
	stream << "\n" << mDependencies.size() << " ";
	for (NodeVec::const_iterator i=mDependencies.begin(), e=mDependencies.end(); i != e; ++i)
		stream << (*i)->GetId() << " ";
	stream << "\n";
}

File::File(const char* path)
: mPath(path)
{}

File::~File()
{}

void Node::AddDependency(Node* n)
{
	if (std::find(mDependencies.begin(), mDependencies.end(), n) == mDependencies.end())
		mDependencies.push_back(n);
}

void File::AddConsumer(Node* n)
{
	if (std::find(mConsumers.begin(), mConsumers.end(), n) == mConsumers.end())
		mConsumers.push_back(n);
}

void File::AddProducer(Node* n)
{
	if (std::find(mProducers.begin(), mProducers.end(), n) == mProducers.end())
		mProducers.push_back(n);
}


}
