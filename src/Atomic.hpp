#ifndef ATOMIC_HPP
#define ATOMIC_HPP

#include "Common.hpp"

#if defined(TUNDRA_WIN32)
#include <windows.h>
#endif

namespace t2
{
#if defined(TUNDRA_WIN32)
  inline uint32_t AtomicIncrement(uint32_t* value)
  {
    return InterlockedIncrement((long*)value);
  }

  inline uint64_t AtomicAdd(uint64_t* ptr, uint64_t value)
  {
#if defined(TUNDRA_WIN32_MINGW)
    // Crappy mingw doesn't have InterlockedExchangeAdd64
    // This is not really atomic (we update the 32-bit words separately), but
    // at least it will not lose values. The only use case we have for this
    // bullshit is the tracking of stats anyway, so it's not mission critical.
    union Atomic64
    {
      struct
      {
        volatile LONG m_Low;
        volatile LONG m_High;
      };
      uint64_t m_Value;
    };
    static_assert(sizeof(Atomic64) == 8, "kill me now");
    Atomic64* w = (Atomic64*) ptr;
    LONG old_lo = InterlockedExchangeAdd(&w->m_Low, (DWORD) value);
    LONG old_hi = InterlockedExchangeAdd(&w->m_High, (DWORD) (value >> 32));
    return (uint64_t(old_hi) << 32) | uint64_t(old_lo);
#else
    return InterlockedExchangeAdd64((__int64 volatile*) ptr, value);
#endif // TUNDRA_WIN32_MINGW
  }

#elif defined(__GNUC__)
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
#endif // __GNUC__

}

#endif
