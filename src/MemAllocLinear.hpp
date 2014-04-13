#ifndef MEMALLOCLINEAR_HPP
#define MEMALLOCLINEAR_HPP

#include "Common.hpp"
#include "Thread.hpp"
#include <cstring>

#if ENABLED(USE_VALGRIND)
#include <valgrind/memcheck.h>
#endif

namespace t2
{

struct MemAllocHeap;

struct MemAllocLinear
{
  enum
  {
    kMaxAlignment     = 64
  };

  char*         m_BasePointer;		// allocated pointer
  char*         m_Pointer;				// aligned pointer
  size_t        m_Size;
  size_t        m_Offset;
  MemAllocHeap* m_BackingHeap;
  const char*   m_DebugName;
  ThreadId      m_OwnerThread;
};

void LinearAllocInit(MemAllocLinear* allocator, MemAllocHeap* heap, size_t max_size, const char* debug_name);

void LinearAllocDestroy(MemAllocLinear* allocator);

void LinearAllocSetOwner(MemAllocLinear* allocator, ThreadId thread_id);

void* LinearAllocate(MemAllocLinear* allocator, size_t size, size_t align);

void LinearAllocReset(MemAllocLinear* allocator);

class MemAllocLinearScope
{
  MemAllocLinear* m_Allocator;
  size_t          m_Offset;

public:
  explicit MemAllocLinearScope(MemAllocLinear* a)
  : m_Allocator(a)
  , m_Offset(a->m_Offset)
  {
  }

  ~MemAllocLinearScope()
  {
    m_Allocator->m_Offset = m_Offset;
#if ENABLED(USE_VALGRIND)
    VALGRIND_MAKE_MEM_NOACCESS(m_Allocator->m_Pointer + m_Offset, m_Allocator->m_Size - m_Offset);
#endif
  }

private:
  MemAllocLinearScope(const MemAllocLinearScope&);
  MemAllocLinearScope& operator=(const MemAllocLinearScope&);
};

template <typename T>
T* LinearAllocate(MemAllocLinear *allocator)
{
  return static_cast<T*>(LinearAllocate(allocator, sizeof(T), ALIGNOF(T)));
}

template <typename T>
T* LinearAllocateArray(MemAllocLinear *allocator, size_t count)
{
  return static_cast<T*>(LinearAllocate(allocator, sizeof(T) * count, ALIGNOF(T)));
}

inline char* StrDupN(MemAllocLinear* allocator, const char* str, size_t len)
{
  size_t sz = len + 1;
  char* buffer = static_cast<char*>(LinearAllocate(allocator, sz, 1));
  memcpy(buffer, str, sz - 1);
  buffer[sz - 1] = '\0';
  return buffer;
}

inline char* StrDup(MemAllocLinear* allocator, const char* str)
{
  return StrDupN(allocator, str, strlen(str));
}


}

#endif
