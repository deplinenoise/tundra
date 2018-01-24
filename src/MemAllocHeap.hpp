#ifndef MEMALLOCHEAP_HPP
#define MEMALLOCHEAP_HPP

#include "Common.hpp"
#include "Thread.hpp"
#include "Mutex.hpp"

#include <string.h>

namespace t2
{

struct MemAllocHeap
{
};

void HeapInit(MemAllocHeap* heap);
void HeapDestroy(MemAllocHeap* heap);

void* HeapAllocate(MemAllocHeap* heap, size_t size);
void* HeapAllocateAligned(MemAllocHeap* heap, size_t size, size_t alignment);

void HeapFree(MemAllocHeap* heap, const void *ptr);

void* HeapReallocate(MemAllocHeap* heap, void *ptr, size_t size);

template <typename T>
T* HeapAllocateArray(MemAllocHeap* heap, size_t count)
{
  return (T*) HeapAllocate(heap, sizeof(T) * count);
}

template <typename T>
T* HeapAllocateArrayZeroed(MemAllocHeap* heap, size_t count)
{
  T* result = HeapAllocateArray<T>(heap, sizeof(T) * count);
  memset(result, 0, sizeof(T) * count);
  return result;
}

}

#endif
