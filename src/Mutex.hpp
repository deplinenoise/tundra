#ifndef MUTEX_HPP
#define MUTEX_HPP

#include "Common.hpp"

#if !defined(_WIN32)
#include <pthread.h>
#else
#include <windows.h>
#endif

namespace t2
{
  struct Mutex
  {
#if !defined(_WIN32)
    pthread_mutex_t m_Impl;
#else
    CRITICAL_SECTION m_Impl;
#endif
  };

#if !defined(_WIN32)
inline void MutexInit(Mutex* self)
{
  if (0 != pthread_mutex_init(&self->m_Impl, nullptr))
    CroakErrno("pthread_mutex_init() failed");
}

inline void MutexDestroy(Mutex* self)
{
  if (0 != pthread_mutex_destroy(&self->m_Impl))
    CroakErrno("pthread_mutex_destroy() failed");
}

inline void MutexLock(Mutex* self)
{
  if (0 != pthread_mutex_lock(&self->m_Impl))
    CroakErrno("pthread_mutex_lock() failed");
}

inline void MutexUnlock(Mutex* self)
{
  if (0 != pthread_mutex_unlock(&self->m_Impl))
    CroakErrno("pthread_mutex_unlock() failed");
}

#else

inline void MutexInit(Mutex* self)
{
  InitializeCriticalSection(&self->m_Impl);
}

inline void MutexDestroy(Mutex* self)
{
  DeleteCriticalSection(&self->m_Impl);
}

inline void MutexLock(Mutex* self)
{
  EnterCriticalSection(&self->m_Impl);
}

inline void MutexUnlock(Mutex* self)
{
  LeaveCriticalSection(&self->m_Impl);
}

#endif

}

#endif
