#include "BinaryWriter.hpp"

#include <cstdio>

namespace t2
{

struct BinarySegment
{
  int                  m_Index;
  int64_t              m_GlobalOffset;
  MemAllocHeap*        m_Heap;
  Buffer<uint8_t>      m_Bytes;
  Buffer<BinaryFixup>  m_Fixups;
};

size_t BinarySegmentSize(BinarySegment* self)
{
  return self->m_Bytes.m_Size;
}

static bool BinarySegmentWrite(BinarySegment* self, void* file_ptr)
{
  size_t count = self->m_Bytes.m_Size;
  return count == fwrite(self->m_Bytes.m_Storage, 1, count, (FILE*) file_ptr);
}

static void BinarySegmentInit(BinarySegment* self, int index, MemAllocHeap* heap)
{
  self->m_Index        = index;
  self->m_GlobalOffset = -1;
  self->m_Heap         = heap;
  BufferInitWithCapacity(&self->m_Bytes, heap, 128 * 1024);
  BufferInitWithCapacity(&self->m_Fixups, heap, 4096);
}

static void BinarySegmentDestroy(BinarySegment* self)
{
  BufferDestroy(&self->m_Fixups, self->m_Heap);
  BufferDestroy(&self->m_Bytes, self->m_Heap);
}

void BinarySegmentAlign(BinarySegment* self, size_t alignment)
{
  size_t offset  = self->m_Bytes.m_Size;
  size_t aligned = (offset + alignment - 1) & ~(alignment - 1);
  size_t delta   = aligned - offset;

  while (delta--)
  {
    BufferAppendOne(&self->m_Bytes, self->m_Heap, 0xfe);
  }
}

void* BinarySegmentAlloc(BinarySegment* seg, size_t len)
{
  return BufferAlloc(&seg->m_Bytes, seg->m_Heap, len);
}

void BinarySegmentWrite(BinarySegment* seg, const void* data, size_t len)
{
  BufferAppend(&seg->m_Bytes, seg->m_Heap, (const uint8_t*) data, len);
}

void BinarySegmentWritePointer(BinarySegment* seg, BinaryLocator locator)
{
  BinaryFixup* fixup = BufferAlloc(&seg->m_Fixups, seg->m_Heap, 1);
  fixup->m_PointerOffset = seg->m_Bytes.m_Size;
  fixup->m_Target        = locator;

  // Write dummy value to be fixed up later
  BinarySegmentWriteUint32(seg, 0x7eeeeeee);
}

static void BinarySegmentFixupPointers(BinarySegment* self, BinarySegment** segs)
{
  int64_t my_seg_base = self->m_GlobalOffset;

  for (auto const& fixup : self->m_Fixups)
  {
    int64_t  target_seg_base = segs[fixup.m_Target.m_SegIndex]->m_GlobalOffset;

    int64_t  source_pos      = my_seg_base + fixup.m_PointerOffset;
    int64_t  dest_pos        = target_seg_base + fixup.m_Target.m_Offset;

    int64_t  delta           = dest_pos - source_pos;
    int32_t  delta32         = int32_t(delta);

    uint8_t *ptr_loc         = self->m_Bytes.m_Storage + fixup.m_PointerOffset;

    if (delta32 != delta)
      Croak("pointer relocation too big");

    memcpy(ptr_loc, &delta32, sizeof(int32_t));
  }
}

BinaryLocator BinarySegmentPosition(BinarySegment* self)
{
  BinaryLocator result;
  result.m_SegIndex = self->m_Index;
  result.m_Offset   = self->m_Bytes.m_Size;
  return result;
}

void BinaryWriterInit(BinaryWriter* w, MemAllocHeap* heap)
{
  w->m_Heap = heap;
  BufferInit(&w->m_Segments);
}

void BinaryWriterDestroy(BinaryWriter* self)
{
  for (BinarySegment* seg : self->m_Segments)
  {
    BinarySegmentDestroy(seg);
    HeapFree(self->m_Heap, seg);
  }

  BufferDestroy(&self->m_Segments, self->m_Heap);
  self->m_Heap = nullptr;
}

static bool BinaryWriterFinalize(BinaryWriter* w)
{
  const size_t seg_count = w->m_Segments.m_Size;
  BinarySegment** segs = w->m_Segments.m_Storage;

  // Align all segments to 16 bytes
  for (size_t i = 0; i < seg_count; ++i)
  {
    BinarySegmentAlign(segs[i], 16);
  }

  // Compute global segment positions
  size_t offset = 0;
  for (size_t i = 0; i < seg_count; ++i)
  {
    segs[i]->m_GlobalOffset = offset;
    offset += BinarySegmentSize(segs[i]);
  }

  // Fix up pointers wrt global segment positions
  for (size_t i = 0; i < seg_count; ++i)
  {
    BinarySegmentFixupPointers(segs[i], segs);
  }

  return true;
}

bool BinaryWriterFlush(BinaryWriter* self, const char* out_fn)
{
  if (!BinaryWriterFinalize(self))
    return false;

  FILE* f = fopen(out_fn, "wb");
  if (!f)
    return false;
  
  bool success = true;

  const size_t seg_count = self->m_Segments.m_Size;
  BinarySegment** segs = self->m_Segments.m_Storage;

  // Align all segments to 16 bytes
  for (size_t i = 0; success && i < seg_count; ++i)
  {
    success = BinarySegmentWrite(segs[i], f);
  }

  fclose(f);
  return success;
}

BinarySegment* BinaryWriterAddSegment(BinaryWriter* self)
{
  BinarySegment* seg = (BinarySegment*) HeapAllocate(self->m_Heap, sizeof(BinarySegment));
  BinarySegmentInit(seg, (int) self->m_Segments.m_Size, self->m_Heap);
  BufferAppendOne(&self->m_Segments, self->m_Heap, seg);
  return seg;
}

}
