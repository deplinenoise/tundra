#include "Node.hpp"
#include "Hasher.hpp"

#include <istream>
#include <ostream>
#include <cassert>
#include <stdexcept>
#include <algorithm>

namespace tundra
{

Node::Node(
	Action* action,
	int inputFileCount,
	File** inputFiles,
	int outputFileCount,
	File** outputFiles,
	const Guid& id)
: mFlags(0)
, mId(id)
, mAction(action)
, mInputFiles(&inputFiles[0], &inputFiles[inputFileCount])
, mOutputFiles(&outputFiles[0], &outputFiles[outputFileCount])
{
}

Node::~Node()
{}

void Node::ComputeNodeId(
	Guid* out,
	Action* action,
	int inputFileCount,
	File** inputFiles,
	int outputFileCount,
	File** outputFiles)
{
	Hasher h;

	action->ComputeHash(&h);

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

int Node::GetFlags() const
{
	return mFlags;
}

bool Node::TestFlag(NodeFlag f) const
{
	return 0 != (mFlags & f);
}

void Node::SetFlags(int v)
{
	mFlags = v;
}

void Node::SetFlag(NodeFlag f)
{
	mFlags |= f;
}

void Node::ClearFlag(NodeFlag f)
{
	mFlags &= ~f;
}

int Node::GetDependencyCount() const
{
	return int(mDependencies.size());
}

Node* Node::GetDependency(int idx) const
{
	return mDependencies[idx];
}

int Node::GetInputFileCount() const
{
	return int(mInputFiles.size());
}

File* Node::GetInputFile(int index) const
{
	return mInputFiles[index];
}

int Node::GetOutputFileCount() const
{
	return int(mOutputFiles.size());
}

File* Node::GetOutputFile(int index) const
{
	return mOutputFiles[index];
}

const Guid& Node::GetId() const
{
	return mId;
}

const char* Node::GetAnnotation() const
{
	return mAnnotation.c_str();
}

Action* Node::GetAction() const
{
	return mAction;
}

Scanner* Node::GetScanner() const
{
	return mScanner;
}

void Node::SetScanner(Scanner* scanner)
{
	mScanner = scanner;
}


}
