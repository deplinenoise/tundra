#ifndef TUNDRA_FILEINDEXCLIENT_HPP
#define TUNDRA_FILEINDEXCLIENT_HPP

#include "Portable.hpp"

namespace tundra
{

enum FileType
{
	FT_Directory,
	FT_File,
	FT_Symlink,
	FT_Other
};

struct FileInfo
{
	FileType Type;
	Guid Digest;
	s64 Size;
};


class FileIndexClient
{
public:
	virtual void ListFiles(const char* path) = 0;

	virtual void Stat(const char* path, FileInfo* out) = 0;

	virtual void Invalidate(const char* path) = 0;
};


}

#endif
