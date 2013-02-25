#ifndef MEMORYMAPPEDFILE_HPP
#define MEMORYMAPPEDFILE_HPP

#include "Common.hpp"

namespace t2
{

struct MemoryMappedFile
{
  void*     m_Address;
  size_t    m_Size;
  uintptr_t m_SysData[2];
};

void MmapFileInit(MemoryMappedFile *file);

void MmapFileDestroy(MemoryMappedFile *file);

void MmapFileMap(MemoryMappedFile *file, const char* fn);

void MmapFileUnmap(MemoryMappedFile *file);

inline bool MmapFileValid(MemoryMappedFile *file)
{
  return file->m_Address != nullptr;
}

}

#endif
