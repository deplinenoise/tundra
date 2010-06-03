#include "Portable.hpp"

#include <cstdio>
#include <stdexcept>
#include <vector>

#ifdef _MSC_VER
#include <windows.h>
#else
#include <unistd.h>
#include <dirent.h>
#endif

void tundra::GetExecutableDir(char* path, size_t path_max)
{
#if defined(_MSC_VER)
	if (0 == GetModuleFileNameA(NULL, path, (DWORD)path_max))
		throw std::runtime_error("GetModuleFileNameA() failed");

	if (char* backslash = strrchr(path, '\\'))
		*backslash = 0;
#else
	if (-1 == readlink("/proc/self/exe", path, path_max))
		throw std::runtime_error("couldn't read /proc/self/exe");

	if (char* slash = strrchr(path, '/'))
		*slash = 0;
#endif
}

namespace tundra
{

#if defined(_MSC_VER)
struct DirectoryEnumerator::Impl
{
	explicit Impl(const char* path)
		: handle(INVALID_HANDLE_VALUE)
	{
		std::string filter(path);
		if (1)
		{
			for (size_t i=0, e=filter.size(); i != e; ++i)
			{
				if (filter[i] == '/')
					filter[i] = '\\';
			}
			filter.append("\\*");
		}
		handle = FindFirstFileA(filter.c_str(), &find_data);
	}

	~Impl()
	{
		if (IsValid())
			FindClose(handle);
	}

	bool IsValid() const
	{
		return INVALID_HANDLE_VALUE != handle;
	}

	bool MoveToNext()
	{
		for (;;)
		{
			if (firstConsumed)
			{
				if (!FindNextFileA(handle, &find_data))
					return false;
			}
			else
				firstConsumed = true;

			if (0 == strcmp(".", find_data.cFileName) ||
				0 == strcmp("..", find_data.cFileName))
				continue;

			return true;
		}
	}

	const char* GetPath(bool* isDir) const
	{
		*isDir = (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? true : false;
		return find_data.cFileName;
	}

	bool firstConsumed;
	HANDLE handle;
	WIN32_FIND_DATAA find_data;
};
#else
struct DirectoryEnumerator::Impl
{
	explicit Impl(const char* path)
		: mHandle(opendir(path))
	{
	}

	~Impl()
	{
		if (IsValid())
			closedir(mHandle);
	}

	bool IsValid() const
	{
		return !!mHandle;
	}

	bool MoveToNext()
	{
		for (;;)
		{
			const dirent* e = readdir(mHandle);
			if (!e)
				return false;

			const char* const fn = e->d_name;

			if (0 == strcmp(".", fn) || 0 == strcmp("..", fn))
				continue;

			size_t len = strlen(fn);

			mFilename.reserve(len+1);
			mFilename.assign(fn, fn+len);
			mFilename.push_back(0);

			// Linux only, but saves a stat()
			mIsDir = (e->d_type == DT_DIR);

			return true;
		}
	}

	const char* GetPath(bool* isDir) const
	{
		*isDir = mIsDir;
		return &mFilename[0];
	}

	DIR* mHandle;
	bool mIsDir;
	std::vector<char> mFilename;
};
#endif

DirectoryEnumerator::DirectoryEnumerator(const char* path)
	: mImpl(new Impl(path))
{
}

DirectoryEnumerator::~DirectoryEnumerator()
{
	delete mImpl;
}

bool DirectoryEnumerator::IsValid() const
{
	return mImpl->IsValid();
}

bool DirectoryEnumerator::MoveToNext()
{
	return mImpl->MoveToNext();
}

const char* DirectoryEnumerator::GetPath(bool* isDir) const
{
	return mImpl->GetPath(isDir);
}

}
