#include "MemAllocLinear.hpp"
#include "MemAllocHeap.hpp"
#include "Common.hpp"

#define CHECK_THREAD_OWNERSHIP(alloc) \
  CHECK(0 == alloc->m_OwnerThread || ThreadCurrent() == alloc->m_OwnerThread)

namespace t2
{

void LinearAllocInit(MemAllocLinear* self, MemAllocHeap* heap, size_t max_size, const char* debug_name)
{
  self->m_BasePointer = static_cast<char*>(HeapAllocate(heap, max_size + MemAllocLinear::kMaxAlignment - 1));
  self->m_Pointer     = nullptr;
  self->m_Size        = max_size;
  self->m_Offset      = 0;
  self->m_BackingHeap = heap;
  self->m_DebugName   = debug_name;

	uintptr_t aligned_base = uintptr_t(self->m_BasePointer + MemAllocLinear::kMaxAlignment - 1) & ~(MemAllocLinear::kMaxAlignment - 1);
	uintptr_t delta        = aligned_base - uintptr_t(self->m_BasePointer);

	self->m_Pointer = self->m_BasePointer + delta;
	self->m_Size   -= (delta);

  self->m_OwnerThread = 0;
}

void LinearAllocDestroy(MemAllocLinear* self)
{
  HeapFree(self->m_BackingHeap, self->m_BasePointer);
  self->m_BasePointer = nullptr;
}

void LinearAllocSetOwner(MemAllocLinear* allocator, ThreadId thread_id)
{
  allocator->m_OwnerThread = thread_id;
}

void* LinearAllocate(MemAllocLinear* self, size_t size, size_t align)
{
  CHECK_THREAD_OWNERSHIP(self);

  // Alignment must be power of two
  CHECK(0 == (align & (align - 1)));

  // Alignment must be non-zero
  CHECK(align > 0);

  // Compute aligned offset.
  size_t offset = (self->m_Offset + align - 1) & ~(align - 1);

	CHECK(0 == (offset & (align -1)));

  // See if we have space.
  if (offset + size <= self->m_Size)
  {
    char* ptr = self->m_Pointer + offset;
    self->m_Offset = offset + size;
    return ptr;
  }
  else
  {
    Croak("Out of memory in linear allocator: %s", self->m_DebugName);
  }
}

void LinearAllocReset(MemAllocLinear* allocator)
{
  CHECK_THREAD_OWNERSHIP(allocator);
  allocator->m_Offset = 0;
}

}
