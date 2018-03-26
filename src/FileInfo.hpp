#ifndef FILEINFO_HPP
#define FILEINFO_HPP

#include "Common.hpp"

namespace t2
{

struct FileInfo
{
  enum
  {
    kFlagExists       = 1 << 0,
    kFlagError        = 1 << 1,
    kFlagFile         = 1 << 2,
    kFlagDirectory    = 1 << 3,
    kFlagDirty        = 1 << 30  // used by stat cache
  };

  uint32_t      m_Flags;
  uint64_t      m_Size;
  uint64_t      m_Timestamp;

  bool Exists()      const { return 0 != (kFlagExists & m_Flags); }
  bool IsFile()      const { return 0 != (kFlagFile & m_Flags); }
  bool IsDirectory() const { return 0 != (kFlagDirectory & m_Flags); }
};

FileInfo GetFileInfo(const char* path);

bool ShouldFilter(const char* name);
bool ShouldFilter(const char* name, size_t len);

void ListDirectory(
    const char* dir,
    void* user_data,
    void (*callback)(void* user_data, const FileInfo& info, const char* path));

}

#endif
