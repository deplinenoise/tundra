#ifndef STATCACHE_HPP
#define STATCACHE_HPP

#include "Common.hpp"
#include "FileInfo.hpp"
#include "ReadWriteLock.hpp"

#include <string.h>

namespace t2
{

struct MemAllocHeap;
struct MemAllocLinear;

struct StatCache
{
  enum
  {
    kFlagCaseSensitive = 1 << 0,

    kLockCount         = 64
  };

  struct Entry
  {
    uint32_t      m_Hash;
    const char*   m_Path;
    FileInfo      m_Info;
    Entry*        m_Next;
  };

  MemAllocLinear* m_Allocator;
  MemAllocHeap*   m_Heap;
  ReadWriteLock   m_HashLock;
  ReadWriteLock   m_EntryLocks[kLockCount];

  uint32_t        m_Flags;
  uint32_t        m_EntryCount;
  uint32_t        m_TableSize;
  uint32_t        m_TableSizeShift;
  Entry**         m_Table;
};

void StatCacheInit(StatCache* stat_cache, MemAllocLinear* allocator, MemAllocHeap* heap, uint32_t flags);

void StatCacheDestroy(StatCache* stat_cache);

void StatCacheMarkDirty(StatCache* stat_cache, const char* path);

FileInfo StatCacheStat(StatCache* stat_cache, const char* path);

}


#endif
