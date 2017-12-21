#ifndef TUNDRA_DIGESTCACHE_HPP
#define TUNDRA_DIGESTCACHE_HPP

#include "Common.hpp"
#include "BinaryData.hpp"
#include "Hash.hpp"
#include "HashTable.hpp"
#include "MemoryMappedFile.hpp"
#include "MemAllocLinear.hpp"
#include "MemAllocHeap.hpp"
#include "ReadWriteLock.hpp"

namespace t2
{
  struct MemAllocHeap;
  struct MemAllocLinear;
  struct DigestCacheState;

  struct FrozenDigestRecord
  {
    uint64_t                       m_Timestamp;
    uint64_t                       m_AccessTime;
    uint32_t                       m_FilenameHash;
    HashDigest                     m_ContentDigest;
    FrozenString                   m_Filename;
#if ENABLED(USE_SHA1_HASH)
    uint32_t                       m_Padding;
#elif ENABLED(USE_FAST_HASH)
    uint32_t                       m_Padding[2];
#endif
  };
  static_assert(sizeof(FrozenDigestRecord) == 48, "struct size");

  struct DigestCacheState
  {
    static const uint32_t           MagicNumber   = 0x12781fa6 ^ kTundraHashMagic;

    uint32_t                        m_MagicNumber;
    FrozenArray<FrozenDigestRecord> m_Records;
  };

  struct DigestCacheRecord
  {
    HashDigest m_ContentDigest;
    uint64_t   m_Timestamp;
    uint64_t   m_AccessTime;
  };


  struct DigestCache
  {
    ReadWriteLock           m_Lock;
    const DigestCacheState* m_State;
    const char*             m_StateFilename;
    MemAllocHeap            m_Heap;
    MemAllocLinear          m_Allocator;
    MemoryMappedFile        m_StateFile;
    HashTable<DigestCacheRecord, kFlagPathStrings> m_Table;
    uint64_t                m_AccessTime;
  };

  void DigestCacheInit(DigestCache* self, size_t heap_size, const char* filename);

  void DigestCacheDestroy(DigestCache* self);

  bool DigestCacheSave(DigestCache* self, MemAllocHeap* serialization_heap, const char* tmp_filename);

  bool DigestCacheGet(DigestCache* self, const char* filename, uint32_t hash, uint64_t timestamp, HashDigest* digest_out);

  void DigestCacheSet(DigestCache* self, const char* filename, uint32_t hash, uint64_t timestamp, const HashDigest& digest);
}

#endif
