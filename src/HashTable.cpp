#include "HashTable.hpp"
#include "MemAllocHeap.hpp"

#include <algorithm>

namespace t2
{

void HashTableInit(HashTable* self, MemAllocHeap* heap, uint32_t flags)
{
  self->m_Table          = nullptr;
  self->m_TableSize      = 0;
  self->m_TableSizeShift = 0;
  self->m_RecordCount    = 0;
  self->m_Heap           = heap;
  self->m_Flags          = flags;
}

void HashTableDestroy(HashTable* self)
{
  HeapFree(self->m_Heap, self->m_Table);
  self->m_Table = nullptr;
}

HashRecord* HashTableLookup(HashTable* self, uint32_t hash, const char* string)
{
  uint32_t size = self->m_TableSize;

  if (0 == size)
  {
    return nullptr;
  }

  HashRecord* chain = self->m_Table[hash & (size - 1)];

  int (*compare_fn)(const char* l, const char* r);

  if (self->m_Flags & HashTable::kFlagCaseInsensitive)
  {
#if defined(_MSC_VER)
    compare_fn = _stricmp;
#else
    compare_fn = strcasecmp;
#endif
  }
  else
  {
    compare_fn = strcmp;
  }

  while (chain)
  {
    if (chain->m_Hash == hash && 0 == (*compare_fn)(string, chain->m_String))
    {
      return chain;
    }
    chain = chain->m_Next;
  }

  return nullptr;
}

static void HashTableGrow(HashTable* self)
{
  MemAllocHeap*  heap      = self->m_Heap;

  const uint32_t old_size  = self->m_TableSize;
  // start at 1<<7 (128), increase by 4x each time
  const uint32_t new_shift = std::max(self->m_TableSizeShift + 2, 7u);
  const uint32_t new_size  = 1 << new_shift;
  
  HashRecord** old_table = self->m_Table;
  HashRecord** new_table = HeapAllocateArrayZeroed<HashRecord*>(heap, new_size);

  const uint32_t new_mask = new_size - 1;

  for (uint32_t i = 0; i < old_size; ++i)
  {
    HashRecord* chain = old_table[i];
    while (chain)
    {
      HashRecord* next = chain->m_Next;

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

void HashTableInsert(HashTable* self, HashRecord* record)
{
  uint32_t record_count = self->m_RecordCount;
  uint32_t table_size = self->m_TableSize;
  uint64_t load = 0x100 * uint64_t(record_count + 1) >> uint64_t(self->m_TableSizeShift);

  if (load > 0x0c0)
  {
    HashTableGrow(self);
    table_size = self->m_TableSize;
  }

  uint32_t index = record->m_Hash & (table_size - 1);

  HashRecord** table = self->m_Table;

  record->m_Next      = table[index];
  table[index]        = record;
  self->m_RecordCount = record_count + 1;
}

}
