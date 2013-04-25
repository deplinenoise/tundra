#ifndef HASHTABLE_HPP
#define HASHTABLE_HPP

#include "Common.hpp"

namespace t2
{
  struct MemAllocHeap;

  struct HashRecord
  {
    uint32_t    m_Hash;
    const char* m_String;
    HashRecord* m_Next;
  };

  struct HashTable
  {
    enum
    {
      kFlagCaseInsensitive = 1 << 0,

#if ENABLED(TUNDRA_CASE_INSENSITIVE_FILESYSTEM)
      kFlagPathStrings = kFlagCaseInsensitive,
#else
      kFlagPathStrings = 0,
#endif

      kFlagFrozen         = 1 << 1,
    };

    HashRecord   **m_Table;
    uint32_t       m_TableSize;
    uint32_t       m_TableSizeShift;
    uint32_t       m_RecordCount;
    MemAllocHeap  *m_Heap;
    uint32_t       m_Flags;
  };

  void HashTableInit(HashTable* table, MemAllocHeap* heap, uint32_t flags);

  void HashTableDestroy(HashTable* table);

  HashRecord* HashTableLookup(HashTable* table, uint32_t hash, const char* string);

  void HashTableInsert(HashTable* table, HashRecord* record);

  template <typename Callback>
  void HashTableWalk(HashTable* self, Callback callback)
  {
    HashRecord** table = self->m_Table;
    uint32_t index = 0;
    for (uint32_t i = 0, count = self->m_TableSize; i < count; ++i)
    {
      HashRecord* chain = table[i];
      while (chain)
      {
        callback(index, chain);
        ++index;
        chain = chain->m_Next;
      }
    }

    CHECK(index == self->m_RecordCount);
  }
}

#endif
