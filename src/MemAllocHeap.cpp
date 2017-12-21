#include "MemAllocHeap.hpp"
#include "dlmalloc.h"
#include <stdlib.h>

namespace t2
{

void HeapInit(MemAllocHeap* heap, size_t capacity, uint32_t flags)
{
#if ENABLED(USE_DLMALLOC)
  heap->m_MemSpace = create_mspace(capacity, 0);
  if (!heap->m_MemSpace)
    Croak("couldn't create memspace for new heap");
#else
  heap->m_MemSpace = nullptr;
#endif

  heap->m_Flags = flags;

  if (flags & HeapFlags::kThreadSafe)
  {
    MutexInit(&heap->m_Lock);
  }
}

void HeapDestroy(MemAllocHeap* heap)
{
  if (heap->m_Flags & HeapFlags::kThreadSafe)
  {
    MutexDestroy(&heap->m_Lock);
  }

#if ENABLED(USE_DLMALLOC)
  destroy_mspace(heap->m_MemSpace);
  heap->m_MemSpace = nullptr;
#endif
}

void* HeapAllocate(MemAllocHeap* heap, size_t size)
{
  bool thread_safe = 0 != (heap->m_Flags & HeapFlags::kThreadSafe);

  if (thread_safe)
  {
    MutexLock(&heap->m_Lock);
  }

  void* ptr = nullptr;
#if ENABLED(USE_DLMALLOC)
  ptr = mspace_malloc(heap->m_MemSpace, size);
#else
  ptr = malloc(size);
#endif
  if (!ptr)
  {
    Croak("out of memory allocating %d bytes", (int) size);
  }

  if (thread_safe)
  {
    MutexUnlock(&heap->m_Lock);
  }

  return ptr;
}

void HeapFree(MemAllocHeap* heap, const void *ptr)
{
  bool thread_safe = 0 != (heap->m_Flags & HeapFlags::kThreadSafe);

  if (thread_safe)
  {
    MutexLock(&heap->m_Lock);
  }

#if ENABLED(USE_DLMALLOC)
  mspace_free(heap->m_MemSpace, (void*) ptr);
#else
  free((void*) ptr);
#endif

  if (thread_safe)
  {
    MutexUnlock(&heap->m_Lock);
  }
}

void* HeapReallocate(MemAllocHeap *heap, void *ptr, size_t size)
{
  bool thread_safe = 0 != (heap->m_Flags & HeapFlags::kThreadSafe);

  if (thread_safe)
  {
    MutexLock(&heap->m_Lock);
  }

  void *new_ptr;
#if ENABLED(USE_DLMALLOC)
  new_ptr = mspace_realloc(heap->m_MemSpace, ptr, size);
#else
  new_ptr = realloc(ptr, size);
#endif
  if (!new_ptr && size > 0)
  {
    Croak("out of memory reallocating %d bytes at %p", (int) size, ptr);
  }

  if (thread_safe)
  {
    MutexUnlock(&heap->m_Lock);
  }

  return new_ptr;
}

}
