#include "StatCache.hpp"
#include "MemAllocHeap.hpp"
#include "MemAllocLinear.hpp"
#include "Stats.hpp"

#include <algorithm>

namespace t2
{

void StatCacheInit(StatCache* self, MemAllocLinear* allocator, MemAllocHeap* heap, uint32_t flags)
{
  self->m_Allocator      = allocator;
  self->m_Heap           = heap;
  self->m_Flags          = flags;
  self->m_EntryCount     = 0;
  self->m_TableSize      = 0;
  self->m_TableSizeShift = 0;
  self->m_Table          = nullptr;

  ReadWriteLockInit(&self->m_HashLock);

  for (int i = 0; i < StatCache::kLockCount; ++i)
    ReadWriteLockInit(&self->m_EntryLocks[i]);
}

void StatCacheDestroy(StatCache* self)
{
  HeapFree(self->m_Heap, self->m_Table);

  for (int i = 0; i < StatCache::kLockCount; ++i)
    ReadWriteLockDestroy(&self->m_EntryLocks[i]);

  ReadWriteLockDestroy(&self->m_HashLock);
}

static bool StatCachePathEqual(const StatCache* self, const char* p1, const char* p2)
{
  if (self->m_Flags & StatCache::kFlagCaseSensitive)
  {
    return 0 == strcmp(p1, p2);
  }
  else
  {
#if defined(_MSC_VER)
    return 0 == _stricmp(p1, p2);
#else
    return 0 == strcasecmp(p1, p2);
#endif
  }
}

static void StatCacheRehash(StatCache* self)
{
  const uint32_t old_size  = self->m_TableSize;
  const uint32_t new_shift = std::max(self->m_TableSizeShift + 2, 7u); // start at 1<<7 (128), increase by 4x each time
  const uint32_t new_size  = 1 << new_shift;

  MemAllocHeap*  heap      = self->m_Heap;
  
  StatCache::Entry** old_table = self->m_Table;
  StatCache::Entry** new_table = HeapAllocateArrayZeroed<StatCache::Entry*>(heap, new_size);

  const uint32_t new_mask = new_size - 1;

  for (uint32_t i = 0; i < old_size; ++i)
  {
    StatCache::Entry* chain = old_table[i];
    while (chain)
    {
      StatCache::Entry* next = chain->m_Next;

      uint32_t index = chain->m_Hash & new_mask;

      chain->m_Next     = new_table[index];
      new_table[index]  = chain;

      chain             = next;
    }
  }

  HeapFree(heap, old_table);

  // Commit
  self->m_Table          = new_table;
  self->m_TableSize      = new_size;
  self->m_TableSizeShift = new_shift;
}

static StatCache::Entry* StatCacheLookupRaw(StatCache* self, uint32_t hash, const char* path)
{
  StatCache::Entry* result = nullptr;

  uint32_t table_size = self->m_TableSize;

  if (table_size > 0)
  {
    uint32_t index = hash & (table_size - 1);
    StatCache::Entry* chain = self->m_Table[index];
    while (chain)
    {
      if (chain->m_Hash == hash && StatCachePathEqual(self, path, chain->m_Path))
      {
        result = chain;
        break;
      }
      else
        chain = chain->m_Next;
    }
  }

  return result;
}

struct StatLookupResult
{
  StatCache::Entry* m_Entry;
  ReadWriteLock*    m_EntryLock;
};

static StatLookupResult StatCacheLookup(StatCache* self, uint32_t hash, const char* path, bool for_write)
{
  StatLookupResult result;

  ReadWriteLockRead(&self->m_HashLock);

  StatCache::Entry* entry = StatCacheLookupRaw(self, hash, path);

  if (entry)
  {
    result.m_Entry     = entry;
    result.m_EntryLock = &self->m_EntryLocks[hash & (StatCache::kLockCount - 1)];

    if (for_write)
    {
      ReadWriteLockWrite(result.m_EntryLock);
    }
    else
    {
      ReadWriteLockRead(result.m_EntryLock);
    }
  }
  else
  {
    result.m_Entry     = nullptr;
    result.m_EntryLock = nullptr;
  }

  ReadWriteUnlockRead(&self->m_HashLock);

  return result;
}

static void StatCacheInsert(StatCache* self, uint32_t hash, const char* path, const FileInfo& info)
{
  ReadWriteLockWrite(&self->m_HashLock);

  // Handle race of someone inserting the same record while we stat'd the file
  StatCache::Entry* entry = StatCacheLookupRaw(self, hash, path);

  if (entry == nullptr)
  {
    uint32_t table_size = self->m_TableSize;

    uint64_t load = 0x100 * uint64_t(self->m_EntryCount + 1) >> uint64_t(self->m_TableSizeShift);

    AtomicIncrement(&g_Stats.m_StatCacheMisses);

    if (load > 0x0c0)
    {
      StatCacheRehash(self);
    }

    table_size     = self->m_TableSize;
    CHECK(table_size > 0);
    uint32_t index = hash & (table_size - 1);

    MemAllocLinear* alloc = self->m_Allocator;
    StatCache::Entry** table = self->m_Table;

    entry         = LinearAllocate<StatCache::Entry>(alloc);

    entry->m_Hash = hash;
    entry->m_Path = StrDup(alloc, path);
    entry->m_Next = table[index];
    entry->m_Info = info;

    table[index]  = entry;

    self->m_EntryCount++;
    ReadWriteUnlockWrite(&self->m_HashLock);
  }
  else
  {
    uint32_t lock_index = hash & (StatCache::kLockCount - 1);

    ReadWriteLockWrite(&self->m_EntryLocks[lock_index]);

    ReadWriteUnlockWrite(&self->m_HashLock);

    entry->m_Info = info;

    ReadWriteUnlockWrite(&self->m_EntryLocks[lock_index]);
  }
}

void StatCacheMarkDirty(StatCache* self, const char* path, uint32_t hash)
{
  StatLookupResult result = StatCacheLookup(self, hash, path, true);

  if (StatCache::Entry *e = result.m_Entry)
  {
    e->m_Info.m_Flags = FileInfo::kFlagDirty;

    ReadWriteUnlockWrite(result.m_EntryLock);
  }
}

FileInfo StatCacheStat(StatCache* self, const char* path, uint32_t hash)
{
  StatLookupResult result = StatCacheLookup(self, hash, path, false);
  StatCache::Entry* entry = result.m_Entry;

  if (nullptr == result.m_Entry)
  {
    AtomicIncrement(&g_Stats.m_StatCacheMisses);
    FileInfo file_info = GetFileInfo(path);

    // There's a natural race condition here. Some other thread might come in,
    // stat the file and insert it before us. We just let that happen. The DAG
    // guarantees that we won't be writing to files that are being stat'd here,
    // so the result of these races is benign.
    StatCacheInsert(self, hash, path, file_info);
    return file_info;
  }

  if (FileInfo::kFlagDirty & entry->m_Info.m_Flags)
  {
    ReadWriteUnlockRead(result.m_EntryLock);

    AtomicIncrement(&g_Stats.m_StatCacheDirty);

    FileInfo file_info = GetFileInfo(entry->m_Path);

    // See comment above - same race.

    StatCacheInsert(self, hash, path, file_info);

    return file_info;
  }
  else
  {
    AtomicIncrement(&g_Stats.m_StatCacheHits);
    FileInfo file_info = entry->m_Info;
    ReadWriteUnlockRead(result.m_EntryLock);
    return file_info;
  }
}

}
