#include "DigestCache.hpp"
#include "BinaryWriter.hpp"
#include "Stats.hpp"

#include <algorithm>
#include <time.h>
#include <stdio.h>

namespace t2
{

void DigestCacheInit(DigestCache* self, size_t heap_size, const char* filename)
{
  ReadWriteLockInit(&self->m_Lock);

  self->m_State = nullptr;
  self->m_StateFilename = filename;

  HeapInit(&self->m_Heap);
  LinearAllocInit(&self->m_Allocator, &self->m_Heap, heap_size / 2, "digest allocator");
  MmapFileInit(&self->m_StateFile);
  HashTableInit(&self->m_Table, &self->m_Heap);

  self->m_AccessTime = time(nullptr);

  MmapFileMap(&self->m_StateFile, filename);
  if (MmapFileValid(&self->m_StateFile))
  {
    const DigestCacheState* state = (const DigestCacheState*) self->m_StateFile.m_Address;
    if (DigestCacheState::MagicNumber == state->m_MagicNumber)
    {
      const uint64_t time_now = time(nullptr);

      // Throw out records that haven't been accessed in a week.
      const uint64_t cutoff_time = time_now - 7 * 24 * 60 * 60;

      self->m_State = state;

      //HashTablePrepareBulkInsert(&self->m_Table, state->m_Records.GetCount());

      for (const FrozenDigestRecord& record : state->m_Records)
      {
        if (record.m_AccessTime < cutoff_time)
          continue;

        DigestCacheRecord r;
        r.m_ContentDigest = record.m_ContentDigest;
        r.m_Timestamp     = record.m_Timestamp;
        r.m_AccessTime    = record.m_AccessTime;
        HashTableInsert(&self->m_Table, record.m_FilenameHash, record.m_Filename.Get(), r);
      }
      Log(kDebug, "digest cache initialized -- %d entries", state->m_Records.GetCount());
    }
    else
    {
      MmapFileUnmap(&self->m_StateFile);
    }
  }
}

void DigestCacheDestroy(DigestCache* self)
{
  HashTableDestroy(&self->m_Table);
  MmapFileDestroy(&self->m_StateFile);
  LinearAllocDestroy(&self->m_Allocator);
  HeapDestroy(&self->m_Heap);
  ReadWriteLockDestroy(&self->m_Lock);
}

bool DigestCacheSave(DigestCache* self, MemAllocHeap* serialization_heap, const char* tmp_filename)
{
  TimingScope timing_scope(nullptr, &g_Stats.m_DigestCacheSaveTimeCycles);

  BinaryWriter writer;
  BinaryWriterInit(&writer, serialization_heap);

  BinarySegment *main_seg   = BinaryWriterAddSegment(&writer);
  BinarySegment *array_seg  = BinaryWriterAddSegment(&writer);
  BinarySegment *string_seg = BinaryWriterAddSegment(&writer);
  BinaryLocator  array_ptr  = BinarySegmentPosition(array_seg);

  auto save_record = [=](size_t index, uint32_t hash, const char* path, const DigestCacheRecord& r)
  {
    BinarySegmentWriteUint64(array_seg, r.m_Timestamp);
    BinarySegmentWriteUint64(array_seg, r.m_AccessTime);
    BinarySegmentWriteUint32(array_seg, hash);
    BinarySegmentWrite(array_seg, &r.m_ContentDigest, sizeof(r.m_ContentDigest));
    BinarySegmentWritePointer(array_seg, BinarySegmentPosition(string_seg));
    BinarySegmentWriteStringData(string_seg, path);
    BinarySegmentWriteUint32(array_seg, 0); // m_Padding
#if ENABLED(USE_FAST_HASH)
    BinarySegmentWriteUint32(array_seg, 0); // m_Padding
#endif
  };

  HashTableWalk(&self->m_Table, save_record);

  BinarySegmentWriteUint32(main_seg, DigestCacheState::MagicNumber);
  BinarySegmentWriteInt32(main_seg, (int) self->m_Table.m_RecordCount);
  BinarySegmentWritePointer(main_seg, array_ptr);

  // Unmap old state to avoid sharing conflicts on Windows.
  MmapFileUnmap(&self->m_StateFile);
  self->m_State = nullptr;

  bool success = BinaryWriterFlush(&writer, tmp_filename);

  if (success)
  {
    success = RenameFile(tmp_filename, self->m_StateFilename);
  }
  else
  {
    remove(tmp_filename);
  }

  BinaryWriterDestroy(&writer);

  return success;
}

bool DigestCacheGet(DigestCache* self, const char* filename, uint32_t hash, uint64_t timestamp, HashDigest* digest_out)
{
  bool result = false;

  ReadWriteLockRead(&self->m_Lock);

  if (DigestCacheRecord* r = (DigestCacheRecord*) HashTableLookup(&self->m_Table, hash, filename))
  {
    if (r->m_Timestamp == timestamp)
    {
      // Technically violates r/w lock - doesn't matter
      r->m_AccessTime = self->m_AccessTime;
      *digest_out     = r->m_ContentDigest;
      result          = true;
    }
  }

  ReadWriteUnlockRead(&self->m_Lock);

  return result;
}

void DigestCacheSet(DigestCache* self, const char* filename, uint32_t hash, uint64_t timestamp, const HashDigest& digest)
{
  ReadWriteLockWrite(&self->m_Lock);

  DigestCacheRecord* r;

  if (nullptr != (r = (DigestCacheRecord*) HashTableLookup(&self->m_Table, hash, filename)))
  {
    r->m_Timestamp     = timestamp;
    r->m_ContentDigest = digest;
    r->m_AccessTime    = self->m_AccessTime;
  }
  else
  {
    DigestCacheRecord r;
    r.m_ContentDigest = digest;
    r.m_Timestamp     = timestamp;
    r.m_AccessTime    = self->m_AccessTime;
    HashTableInsert(&self->m_Table, hash, StrDup(&self->m_Allocator, filename), r);
  }

  ReadWriteUnlockWrite(&self->m_Lock);
}

}
