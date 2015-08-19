#ifndef BUFFER_HPP
#define BUFFER_HPP

#include "Common.hpp"
#include "MemAllocHeap.hpp"

#include <cstring>
#include <type_traits>

namespace t2
{
  template <typename T>
  struct Buffer
  {
    T      *m_Storage;
    size_t  m_Size;
    size_t  m_Capacity;

    // support for(var: buffer) syntax
    T* begin() { return m_Storage; }
    T* end() { return m_Storage + m_Size; }
    const T* begin() const { return m_Storage; }
    const T* end() const { return m_Storage + m_Size; }

    // support indexing
    T& operator[](size_t index) { return m_Storage[index]; }
    const T& operator[](size_t index) const { return m_Storage[index]; }
  };

  template <typename T>
  void BufferInit(Buffer<T>* buffer)
  {
    static_assert(std::is_pod<T>::value, "T must be a POD type");

    buffer->m_Storage  = nullptr;
    buffer->m_Size     = 0;
    buffer->m_Capacity = 0;
  }

  template <typename T>
  void BufferClear(Buffer<T>* buffer)
  {
    buffer->m_Size = 0;
  }

  template <typename T>
  void BufferInitWithCapacity(Buffer<T>* buffer, MemAllocHeap* heap, size_t capacity)
  {
    static_assert(std::is_pod<T>::value, "T must be a POD type");

    buffer->m_Storage  = (T*) HeapAllocate(heap, sizeof(T) * capacity);
    buffer->m_Size     = 0;
    buffer->m_Capacity = capacity;
  }

  template <typename T>
  void BufferDestroy(Buffer<T>* buffer, MemAllocHeap* heap)
  {
    if (T* ptr = buffer->m_Storage)
    {
      HeapFree(heap, ptr);

      buffer->m_Storage  = nullptr;
      buffer->m_Size     = 0;
      buffer->m_Capacity = 0;
    }
  }

  template <typename T>
  T* BufferAlloc(Buffer<T>* buffer, MemAllocHeap* heap, size_t count)
  {
    T            *storage  = buffer->m_Storage;
    const size_t  old_size = buffer->m_Size;
    const size_t  capacity = buffer->m_Capacity;

    if (old_size + count > capacity)
    {
      size_t new_capacity = capacity ? capacity *2 : 8;

      if (new_capacity < old_size + count)
        new_capacity = old_size + count;

      storage           = (T*) HeapReallocate(heap, storage, sizeof(T) *new_capacity);
      buffer->m_Storage  = storage;
      buffer->m_Capacity = new_capacity;
    }

    buffer->m_Size = old_size + count;
    return storage + old_size;
  }

  template <typename T>
  T* BufferAllocZero(Buffer<T>* buffer, MemAllocHeap* heap, size_t count)
  {
    T *result = BufferAlloc(buffer, heap, count);
    memset(result, 0, sizeof(T) * count);
    return result;
  }

  template <typename T>
  T* BufferAllocFill(Buffer<T>* buffer, MemAllocHeap* heap, size_t count, T fill_value)
  {
    T *result = BufferAlloc(buffer, heap, count);
    for (size_t i = 0; i < count; ++i)
      result[i] = fill_value;
    return result;
  }

  template <typename T>
  void BufferAppend(Buffer<T>* buffer, MemAllocHeap* heap, const T* elems, size_t count)
  {
    T *dest = BufferAlloc(buffer, heap, count);
    memcpy(dest, elems, count * sizeof(T));
  }

  template <typename T, typename U>
  void BufferAppendOne(Buffer<T>* buffer, MemAllocHeap* heap, U elem)
  {
    T *dest = BufferAlloc(buffer, heap, 1);
    *dest = (T) elem;
  }

  template <typename T>
  T BufferPopOne(Buffer<T>* buffer)
  {
    size_t size = buffer->m_Size;
    CHECK(size > 0);
    buffer->m_Size = size-1;
    return buffer->m_Storage[size - 1];
  }
}

#endif
