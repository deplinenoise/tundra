#include "MemoryMappedFile.hpp"
#include "Stats.hpp"

#if defined(TUNDRA_UNIX)
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#endif

#if defined(TUNDRA_WIN32)
#include <windows.h>
#endif

namespace t2
{

static void Clear(MemoryMappedFile* file)
{
  file->m_Address    = nullptr;
  file->m_Size       = 0;
  file->m_SysData[0] = 0;
  file->m_SysData[1] = 0;
}

void MmapFileInit(MemoryMappedFile *self)
{
  Clear(self);
}

void MmapFileDestroy(MemoryMappedFile* self)
{
  MmapFileUnmap(self);
}

#if defined(TUNDRA_UNIX)
// Attempt to mmap a file for read-only access.
void MmapFileMap(MemoryMappedFile* self, const char *fn)
{
  TimingScope timing_scope(&g_Stats.m_MmapCalls, &g_Stats.m_MmapTimeCycles);

  MmapFileUnmap(self);

  int fd = open(fn, O_RDONLY);

  if (-1 == fd)
    goto error;

  struct stat stbuf;
  if (0 != fstat(fd, &stbuf))
    goto error;

  self->m_Address    = mmap(NULL, stbuf.st_size, PROT_READ, MAP_FILE|MAP_PRIVATE, fd, 0);
  self->m_Size       = stbuf.st_size;
  self->m_SysData[0] = fd;

  if (self->m_Address)
    return;

error:
  if (-1 != fd)
    close(fd);

  Clear(self);
}

// Unmap an mmaped file from RAM.
void MmapFileUnmap(MemoryMappedFile* self)
{
  if (self->m_Address)
  {
    TimingScope timing_scope(&g_Stats.m_MunmapCalls, &g_Stats.m_MunmapTimeCycles);

    if (0 != munmap(self->m_Address, self->m_Size))
      Croak("munmap(%p, %d) failed: %d", self->m_Address, (int) self->m_Size, errno);

    close((int) self->m_SysData[0]);
  }

  Clear(self);
}
#endif

#if defined(TUNDRA_WIN32)
static uint64_t GetFileSize64(HANDLE h)
{
  DWORD size_hi;
  DWORD size_lo = GetFileSize(h, &size_hi);
  return uint64_t(size_hi) << 32 | size_lo;
}

// Attempt to mmap a file for read-only access.
void MmapFileMap(MemoryMappedFile* self, const char *fn)
{
  TimingScope timing_scope(&g_Stats.m_MmapCalls, &g_Stats.m_MmapTimeCycles);

  const DWORD desired_access       = GENERIC_READ;
  const DWORD share_mode           = FILE_SHARE_READ;
  const DWORD creation_disposition = OPEN_EXISTING;
  const DWORD flags                = FILE_ATTRIBUTE_NORMAL;

  HANDLE file = CreateFileA(fn, desired_access, share_mode, NULL, creation_disposition, flags, NULL);

  if (INVALID_HANDLE_VALUE == file)
  {
    return;
  }

  const uint64_t file_size = GetFileSize64(file);

  HANDLE mapping = CreateFileMapping(file, NULL, PAGE_READONLY, DWORD(file_size >> 32), DWORD(file_size), NULL);
  if (nullptr == mapping)
  {
    Log(kError, "CreateFileMapping() failed: %u", GetLastError());
    CloseHandle(file);
    return;
  }

  void* address = MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, DWORD(file_size));

  if (nullptr == address)
  {
    Log(kError, "MapViewOfFile() failed: %u", GetLastError());
    CloseHandle(mapping);
    CloseHandle(file);
    return;
  }

  self->m_Address    = address;
  self->m_Size       = (size_t) file_size;
  self->m_SysData[0] = (uintptr_t) file;
  self->m_SysData[1] = (uintptr_t) mapping;
}

// Unmap an mmaped file from RAM.
void MmapFileUnmap(MemoryMappedFile* self)
{
  TimingScope timing_scope(&g_Stats.m_MmapCalls, &g_Stats.m_MmapTimeCycles);

  if (self->m_Address)
  {
    if (!UnmapViewOfFile(self->m_Address))
    {
      CroakErrno("UnMapViewOfFile() failed");
    }

    HANDLE file    = (HANDLE) self->m_SysData[0];
    HANDLE mapping = (HANDLE) self->m_SysData[1];

    CloseHandle(mapping);
    CloseHandle(file);
  }

  Clear(self);
}
#endif

}
