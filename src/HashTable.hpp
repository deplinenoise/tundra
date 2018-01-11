#ifndef HASHTABLE_HPP
#define HASHTABLE_HPP

#include "Common.hpp"
#include "MemAllocHeap.hpp"

#include <algorithm>

namespace t2
{
  struct MemAllocHeap;

  enum HashTableFlags
  {
    kFlagCaseSensitive   = 0,
    kFlagCaseInsensitive = 1 << 0,

#if ENABLED(TUNDRA_CASE_INSENSITIVE_FILESYSTEM)
    kFlagPathStrings = kFlagCaseInsensitive,
#else
    kFlagPathStrings = kFlagCaseSensitive,
#endif
  };

  template <uint32_t kFlags>
  struct HashTableBase
  {
    uint32_t*      m_Hashes;
    const char**   m_Strings;
    uint32_t       m_TableSize;
    uint32_t       m_TableSizeShift;
    uint32_t       m_RecordCount;
    MemAllocHeap  *m_Heap;
  };

  template <typename T, uint32_t kFlags>
  struct HashTable : public HashTableBase<kFlags>
  {
    T*             m_Payloads;
  };

  template <uint32_t kFlags>
  struct HashSet : public HashTableBase<kFlags>
  {
  };

  template <uint32_t kFlags>
  void HashTableBaseInit(HashTableBase<kFlags>* self, MemAllocHeap* heap)
  {
    self->m_Hashes         = nullptr;
    self->m_Strings        = nullptr;
    self->m_TableSize      = 0;
    self->m_TableSizeShift = 0;
    self->m_RecordCount    = 0;
    self->m_Heap           = heap;
  }

  template <typename T, uint32_t kFlags>
  void HashTableInit(HashTable<T, kFlags>* self, MemAllocHeap* heap)
  {
    HashTableBaseInit(self, heap);
    self->m_Payloads       = nullptr;
  }

  template <uint32_t kFlags>
  void HashSetInit(HashSet<kFlags>* self, MemAllocHeap* heap)
  {
    HashTableBaseInit(self, heap);
  }

  template <uint32_t kFlags>
  void HashTableBaseDestroy(HashTableBase<kFlags>* self)
  {
    HeapFree(self->m_Heap, self->m_Hashes);
    HeapFree(self->m_Heap, self->m_Strings);
  }

  template <typename T, uint32_t kFlags>
  void HashTableDestroy(HashTable<T, kFlags>* self)
  {
    HashTableBaseDestroy(self);
    HeapFree(self->m_Heap, self->m_Payloads);
    HashTableInit(self, self->m_Heap);
  }

  template <uint32_t kFlags>
  void HashSetDestroy(HashSet<kFlags>* self)
  {
    HashTableBaseDestroy(self);
    HashSetInit(self, self->m_Heap);
  }

  inline int FastCompareNoCase(const char* lhs, const char* rhs)
  {
    for (;;)
    {
      int lc = *lhs++;
      int rc = *rhs++;

      int diff = FoldCase(lc) - FoldCase(rc);
      if (diff != 0)
        return diff;

      if (lc == 0 || rc == 0)
        return 0;
    }
  }

  template <uint32_t kFlags>
  int HashTableBaseLookup(HashTableBase<kFlags>* self, uint32_t hash, const char* string)
  {
    uint32_t size = self->m_TableSize;

    if (0 == size)
    {
      return -1;
    }

    int (*compare_fn)(const char* l, const char* r);

    if (kFlags & kFlagCaseInsensitive)
    {
      compare_fn = FastCompareNoCase;
    }
    else
    {
      compare_fn = strcmp;
    }

    const uint32_t* hashes = self->m_Hashes;
    const char* const* strings = self->m_Strings;

    uint32_t index = hash & (size - 1);

    for (;;)
    {
      uint32_t candidate_hash = hashes[index];

      if (!candidate_hash)
        return -1;

      if (hash == candidate_hash)
      {
        const char* candidate_string = strings[index];
        if (candidate_string == string || compare_fn(candidate_string, string) == 0)
        {
          return index;
        }
      }

      index = (index + 1) & (size - 1);
    }
  }

  template <typename T, uint32_t kFlags>
  T* HashTableLookup(HashTable<T, kFlags>* self, uint32_t hash, const char* string)
  {
    int index = HashTableBaseLookup(self, hash, string);

    if (-1 == index)
      return nullptr;

    return self->m_Payloads + index;
  }

  template <uint32_t kFlags>
  bool HashSetLookup(HashSet<kFlags>* self, uint32_t hash, const char* string)
  {
    int index = HashTableBaseLookup(self, hash, string);
    return -1 != index;
  }

  template <typename T, uint32_t kFlags>
  void HashTableGrow(HashTable<T, kFlags>* self)
  {
    MemAllocHeap*  heap      = self->m_Heap;

    const uint32_t old_size  = self->m_TableSize;
    // start at 1<<7 (128), increase by 4x each time
    const uint32_t new_shift = std::max(self->m_TableSizeShift + 2, 7u);
    const uint32_t new_size  = 1 << new_shift;

    uint32_t* old_hashes = self->m_Hashes;
    const char** old_strings = self->m_Strings;
    const T* old_payloads = self->m_Payloads;

    uint32_t* new_hashes = HeapAllocateArrayZeroed<uint32_t>(heap, new_size);
    const char** new_strings = HeapAllocateArrayZeroed<const char*>(heap, new_size);
    T* new_payloads = HeapAllocateArrayZeroed<T>(heap, new_size);

    const uint32_t new_mask = new_size - 1;

    for (uint32_t i = 0; i < old_size; ++i)
    {
      if (uint32_t h = old_hashes[i])
      {
        uint32_t index = h & new_mask;

        while (new_hashes[index] != 0)
        {
          index = (index + 1) & new_mask;
        }

        new_hashes[index] = h;
        new_strings[index] = old_strings[i];
        new_payloads[index] = old_payloads[i];
      }
    }

    HeapFree(heap, old_payloads);
    HeapFree(heap, old_strings);
    HeapFree(heap, old_hashes);

    // Commit
    self->m_Hashes         = new_hashes;
    self->m_Strings        = new_strings;
    self->m_Payloads       = new_payloads;
    self->m_TableSize      = new_size;
    self->m_TableSizeShift = new_shift;
  }

  template <uint32_t kFlags>
  void HashTableGrow(HashSet<kFlags>* self)
  {
    MemAllocHeap*  heap      = self->m_Heap;

    const uint32_t old_size  = self->m_TableSize;
    // start at 1<<7 (128), increase by 4x each time
    const uint32_t new_shift = std::max(self->m_TableSizeShift + 2, 7u);
    const uint32_t new_size  = 1 << new_shift;

    uint32_t* old_hashes = self->m_Hashes;
    const char** old_strings = self->m_Strings;

    uint32_t* new_hashes = HeapAllocateArrayZeroed<uint32_t>(heap, new_size);
    const char** new_strings = HeapAllocateArrayZeroed<const char*>(heap, new_size);

    const uint32_t new_mask = new_size - 1;

    for (uint32_t i = 0; i < old_size; ++i)
    {
      if (uint32_t h = old_hashes[i])
      {
        uint32_t index = h & new_mask;

        while (new_hashes[index] != 0)
        {
          index = (index + 1) & new_mask;
        }

        new_hashes[index] = h;
        new_strings[index] = old_strings[i];
      }
    }

    HeapFree(heap, old_strings);
    HeapFree(heap, old_hashes);

    // Commit
    self->m_Hashes         = new_hashes;
    self->m_Strings        = new_strings;
    self->m_TableSize      = new_size;
    self->m_TableSizeShift = new_shift;
  }

  template <typename TableType>
  int HashTableBaseInsert(TableType* self, uint32_t hash, const char* string)
  {
    uint32_t record_count = self->m_RecordCount;
    uint32_t table_size = self->m_TableSize;
    uint64_t load = 0x100 * uint64_t(record_count + 1) >> uint64_t(self->m_TableSizeShift);

    if (load > 0x050)
    {
      HashTableGrow(self);
      table_size = self->m_TableSize;
    }

    uint32_t index = hash & (table_size - 1);

    uint32_t* hashes = self->m_Hashes;

    while (hashes[index] != 0)
    {
      index = (index + 1) & (table_size - 1);
    }

    hashes[index] = hash;
    self->m_Strings[index] = string;
    self->m_RecordCount = record_count + 1;

    return index;
  }

  template <typename T, uint32_t kFlags>
  void HashTableInsert(HashTable<T, kFlags>* self, uint32_t hash, const char* string, const T& payload)
  {
    int index = HashTableBaseInsert(self, hash, string);
    self->m_Payloads[index] = payload;
  }

  template <uint32_t kFlags>
  void HashSetInsert(HashSet<kFlags>* self, uint32_t hash, const char* string)
  {
    HashTableBaseInsert(self, hash, string);
  }

  template <typename T, uint32_t kFlags, typename Callback>
  void HashTableWalk(HashTable<T, kFlags>* self, Callback callback)
  {
    uint32_t* hashes = self->m_Hashes;
    const char** strings = self->m_Strings;
    const T* payloads = self->m_Payloads;

    uint32_t index = 0;
    for (uint32_t i = 0, count = self->m_TableSize; i < count; ++i)
    {
      if (uint32_t hash = hashes[i])
      {
        callback(index, hash, strings[i], payloads[i]);
        ++index;
      }
    }

    CHECK(index == self->m_RecordCount);
  }

  template <uint32_t kFlags, typename Callback>
  void HashSetWalk(HashSet<kFlags>* self, Callback callback)
  {
    uint32_t* hashes = self->m_Hashes;
    const char** strings = self->m_Strings;

    uint32_t index = 0;
    for (uint32_t i = 0, count = self->m_TableSize; i < count; ++i)
    {
      if (uint32_t hash = hashes[i])
      {
        callback(index, hash, strings[i]);
        ++index;
      }
    }

    CHECK(index == self->m_RecordCount);
  }
}

#endif
