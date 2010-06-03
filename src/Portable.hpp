#ifndef TUNDRA_PORTABLE_HPP
#define TUNDRA_PORTABLE_HPP

#include <cstddef>
#include <cstring>

namespace tundra
{

typedef unsigned char u8;
typedef signed char s8;

typedef unsigned short u16;
typedef short s16;

typedef unsigned int u32;
typedef int s32;

#ifdef _MSC_VER
typedef __int64 s64;
typedef unsigned __int64 u64;
#else
typedef long long s64;
typedef unsigned long long u64;
#endif

void GetExecutableDir(char* buffer, size_t buffer_size);

class DirectoryEnumerator
{
private:
	struct Impl;
	Impl* mImpl;

public:
	DirectoryEnumerator(const char* path);
	~DirectoryEnumerator();

public:
	bool IsValid() const;
	bool MoveToNext();
	const char* GetPath(bool* isDir) const;

private:
	DirectoryEnumerator(const DirectoryEnumerator&);
	DirectoryEnumerator& operator=(const DirectoryEnumerator&);
};

struct StringCompare
{
	bool operator()(const char* a, const char* b) const
	{
		return strcmp(a, b) < 0;
	}
};

}

#endif
