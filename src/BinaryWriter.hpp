#ifndef BINARYWRITER_HPP
#define BINARYWRITER_HPP

#include "Buffer.hpp"

namespace t2
{

struct MemAllocHeap;
struct BinarySegment;

struct BinaryLocator
{
  int32_t m_SegIndex;
  size_t  m_Offset;
};

struct BinaryFixup
{
  size_t        m_PointerOffset;   // Offset to pointer in segment with fixup.
  BinaryLocator m_Target;          // What the pointer points to.
};

struct BinaryWriter
{
  MemAllocHeap*          m_Heap;
  Buffer<BinarySegment*> m_Segments;
};

size_t BinarySegmentSize(BinarySegment* seg);

void BinarySegmentAlign(BinarySegment* seg, size_t alignment);

void* BinarySegmentAlloc(BinarySegment* seg, size_t len);
void BinarySegmentWrite(BinarySegment* seg, const void* data, size_t len);
void BinarySegmentWritePointer(BinarySegment* seg, BinaryLocator locator);

inline void BinarySegmentWriteUint8(BinarySegment* seg, uint8_t v)
{
  BinarySegmentWrite(seg, &v, sizeof v);
}

inline void BinarySegmentWriteInt16(BinarySegment* seg, int16_t v)
{
  BinarySegmentWrite(seg, &v, sizeof v);
}

inline void BinarySegmentWriteInt32(BinarySegment* seg, int32_t v)
{
  BinarySegmentWrite(seg, &v, sizeof v);
}

inline void BinarySegmentWriteInt64(BinarySegment* seg, int64_t v)
{
  BinarySegmentWrite(seg, &v, sizeof v);
}

inline void BinarySegmentWriteUint32(BinarySegment* seg, uint32_t v)
{
  BinarySegmentWrite(seg, &v, sizeof v);
}

inline void BinarySegmentWriteUint64(BinarySegment* seg, uint64_t v)
{
  BinarySegmentWrite(seg, &v, sizeof v);
}

inline void BinarySegmentWriteStringData(BinarySegment* seg, const char* s)
{
  BinarySegmentWrite(seg, s, strlen(s) + 1); // include the nul terminator
}

inline void BinarySegmentWriteNullPointer(BinarySegment* seg)
{
  BinarySegmentWriteUint32(seg, 0);
}

BinaryLocator BinarySegmentPosition(BinarySegment *seg);

void BinaryWriterInit(BinaryWriter* w, MemAllocHeap* heap);
void BinaryWriterDestroy(BinaryWriter* w);

BinarySegment* BinaryWriterAddSegment(BinaryWriter* w);

bool BinaryWriterFlush(BinaryWriter* w, const char* out_fn);

}

#endif
