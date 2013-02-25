#ifndef ATOMIC_HPP
#define ATOMIC_HPP

#include "Common.hpp"

#if defined(_MSC_VER)
#include <windows.h>
#endif

namespace t2
{

#if defined(__GNUC__)
  inline uint32_t AtomicIncrement(uint32_t* value)
  {
    return __sync_add_and_fetch(value, 1);
  }
  inline uint64_t AtomicAdd(uint64_t* ptr, uint64_t value)
  {
#if defined(__powerpc__)
    // Not implemented on PPC. I only have a single core PPC anyway.
    *ptr += value;
    return *ptr;
#else
    return __sync_add_and_fetch(ptr, value);
#endif
  }
#elif defined(_MSC_VER)
  inline uint32_t AtomicIncrement(uint32_t* value)
  {
    return InterlockedIncrement((long*)value);
  }
  inline uint64_t AtomicAdd(uint64_t* ptr, uint64_t value)
  {
    return InterlockedExchangeAdd64((__int64 volatile*) ptr, value);
  }
#endif

}

#endif
