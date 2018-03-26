#include "FileInfo.hpp"
#include "Stats.hpp"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#if defined(TUNDRA_WIN32_MINGW)
// mingw's sys/stat.h is broken and doesn't wrap structs in the extern "C" block
extern "C" {
#endif

#include <sys/stat.h>

#if defined(TUNDRA_WIN32_MINGW)
}
#endif

#include <errno.h>

#if defined(TUNDRA_UNIX)
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#elif defined(TUNDRA_WIN32)
#include <windows.h>
#endif

namespace t2
{

FileInfo GetFileInfo(const char* path)
{
  TimingScope timing_scope(&g_Stats.m_StatCount, &g_Stats.m_StatTimeCycles);

  FileInfo result;
#if defined(TUNDRA_UNIX)
  struct stat stbuf;
#elif defined(TUNDRA_WIN32)
  struct __stat64 stbuf;
#endif

#if defined(TUNDRA_UNIX)
  if (0 == stat(path, &stbuf))
#elif defined(TUNDRA_WIN32_MINGW)
  if (0 == _stat64(path, &stbuf))
#elif defined(TUNDRA_WIN32)
  if (0 == __stat64(path, &stbuf))
#endif
  {
    uint32_t flags = FileInfo::kFlagExists;

    if ((stbuf.st_mode & S_IFMT) == S_IFDIR)
      flags |= FileInfo::kFlagDirectory;
    else if ((stbuf.st_mode & S_IFMT) == S_IFREG)
      flags |= FileInfo::kFlagFile;

    result.m_Flags     = flags;
    result.m_Timestamp = stbuf.st_mtime;
    result.m_Size      = stbuf.st_size;
  }
  else
  {
    result.m_Flags     = errno == ENOENT ? 0 : FileInfo::kFlagError;
    result.m_Timestamp = 0;
    result.m_Size      = 0;
  }

  return result;
}

bool ShouldFilter(const char* name)
{
  return ShouldFilter(name, strlen(name));
}

bool ShouldFilter(const char* name, size_t len)
{
  // Filter out some common noise entries that only serve to cause DAG regeneration.

  if (1 == len && name[0] == '.')
    return true;

  if (2 == len && name[0] == '.' && name[1] == '.')
    return true;

  // Vim .foo.swp files
  if (len >= 4 && name[0] == '.' && 0 == memcmp(name + len - 4, ".swp", 4))
    return true;

  // Weed out '.tundra2.*' files too, as the .json file gets removed in between
  // regenerating, messing up glob signatures.
  static const char t2_prefix[] = ".tundra2.";
  if (len >= (sizeof t2_prefix) - 1 && 0 == memcmp(name, t2_prefix, (sizeof t2_prefix) - 1))
    return true;

  // Emacs foo~ files
  if (len > 1 && name[len-1] == '~')
    return true;

  return false;
}

void ListDirectory(
    const char* path,
    void* user_data,
    void (*callback)(void* user_data, const FileInfo& info, const char* path))
{
#if defined(TUNDRA_UNIX)
	char full_fn[512];
	struct dirent entry;
	struct dirent* result = NULL;
	const size_t path_len = strlen(path);

	if (path_len + 1 > sizeof(full_fn)) {
    Log(kWarning, "path too long: %s", path);
		return;
  }

	strcpy(full_fn, path);

	DIR* dir = opendir(path);

	if (!dir)
  {
    Log(kWarning, "opendir() failed: %s", path);
		return;
  }

	while (0 == readdir_r(dir, &entry, &result) && result)
	{
    size_t len = strlen(entry.d_name);

    if (ShouldFilter(entry.d_name, len))
      continue;

		if (len + path_len + 2 >= sizeof(full_fn))
    {
			Log(kWarning, "%s: name too long\n", entry.d_name);
      continue;
    }

		full_fn[path_len] = '/';
		strcpy(full_fn + path_len + 1, entry.d_name);

    FileInfo info = GetFileInfo(full_fn);
    (*callback)(user_data, info, entry.d_name);
	}

	closedir(dir);

#else
	WIN32_FIND_DATAA find_data;
	char             scan_path[MAX_PATH];

	_snprintf(scan_path, sizeof(scan_path), "%s/*", path);

	for (int i = 0; i < MAX_PATH; ++i)
	{
		char ch = scan_path[i];
		if ('/' == ch)
			scan_path[i] = '\\';
		else if ('\0' == ch)
			break;
	}

	HANDLE h = FindFirstFileA(scan_path, &find_data);

	if (INVALID_HANDLE_VALUE == h)
  {
    Log(kWarning, "FindFirstFile() failed: %s", path);
		return;
  }

	do
	{
    if (ShouldFilter(find_data.cFileName, strlen(find_data.cFileName)))
      continue;

    static const uint64_t kEpochDiff = 0x019DB1DED53E8000LL; // 116444736000000000 nsecs
    static const uint64_t kRateDiff = 10000000; // 100 nsecs

    uint64_t ft = uint64_t(find_data.ftLastWriteTime.dwHighDateTime) << 32 | find_data.ftLastWriteTime.dwLowDateTime;

    FileInfo info;
    info.m_Flags     = FileInfo::kFlagExists;
    info.m_Size      = uint64_t(find_data.nFileSizeHigh) << 32 | find_data.nFileSizeLow;
    info.m_Timestamp = (ft - kEpochDiff) / kRateDiff;

    if (FILE_ATTRIBUTE_DIRECTORY & find_data.dwFileAttributes)
      info.m_Flags |= FileInfo::kFlagDirectory;
    else
      info.m_Flags |= FileInfo::kFlagFile;

    (*callback)(user_data, info, find_data.cFileName);

	} while (FindNextFileA(h, &find_data));

	if (!FindClose(h))
		CroakErrno("couldn't close FindFile handle");
#endif
}

}
