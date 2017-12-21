#ifndef STATCACHE_HPP
#define STATCACHE_HPP

#include "Common.hpp"
#include "FileInfo.hpp"
#include "ReadWriteLock.hpp"
#include "HashTable.hpp"

#include <string.h>

namespace t2
{

struct MemAllocHeap;
struct MemAllocLinear;

struct StatCache
{
  MemAllocLinear* m_Allocator;
  MemAllocHeap*   m_Heap;
  ReadWriteLock   m_HashLock;
  HashTable<FileInfo, kFlagPathStrings> m_Files;
};

void StatCacheInit(StatCache* stat_cache, MemAllocLinear* allocator, MemAllocHeap* heap);

void StatCacheDestroy(StatCache* stat_cache);

void StatCacheMarkDirty(StatCache* stat_cache, const char* path, uint32_t hash);

FileInfo StatCacheStat(StatCache* stat_cache, const char* path, uint32_t hash);

inline FileInfo StatCacheStat(StatCache* stat_cache, const char* path)
{
  return StatCacheStat(stat_cache, path, Djb2HashPath(path));
}


}


#endif
