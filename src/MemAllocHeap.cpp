#include "MemAllocHeap.hpp"
#include <stdlib.h>

namespace t2
{

void HeapInit(MemAllocHeap* heap)
{
  (void) heap;
}

void HeapDestroy(MemAllocHeap* heap)
{
  (void) heap;
}

void* HeapAllocate(MemAllocHeap* heap, size_t size)
{
  (void) heap;
  return malloc(size);
}

void HeapFree(MemAllocHeap* heap, const void *ptr)
{
  (void) heap;
  free((void*) ptr);
}

void* HeapReallocate(MemAllocHeap *heap, void *ptr, size_t size)
{
  (void) heap;
  void* new_ptr = realloc(ptr, size);

  if (!new_ptr && size > 0)
  {
    Croak("out of memory reallocating %d bytes at %p", (int) size, ptr);
  }

  return new_ptr;
}

}
