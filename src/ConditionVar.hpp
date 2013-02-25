#ifndef CONDITION_VAR_HPP
#define CONDITION_VAR_HPP

#include "Common.hpp"
#include "Mutex.hpp"

#if defined(TUNDRA_UNIX)
#include <pthread.h>
#elif defined(TUNDRA_WIN32)
#include <windows.h>
#endif

namespace t2
{
  struct Mutex;

#if defined(TUNDRA_UNIX)
  struct ConditionVariable
  {
    pthread_cond_t m_Impl;
  };

  inline void CondInit(ConditionVariable* var)
  {
    if (0 != pthread_cond_init(&var->m_Impl, nullptr))
      CroakErrno("pthread_cond_init() failed");
  }

  inline void CondDestroy(ConditionVariable* var)
  {
    if (0 != pthread_cond_destroy(&var->m_Impl))
      CroakErrno("pthread_cond_destroy() failed");
  }

  inline void CondWait(ConditionVariable* var, Mutex* mutex)
  {
    if (0 != pthread_cond_wait(&var->m_Impl, &mutex->m_Impl))
      CroakErrno("pthread_cond_wait() failed");
  }

  inline void CondSignal(ConditionVariable* var)
  {
    if (0 != pthread_cond_signal(&var->m_Impl))
      CroakErrno("pthread_cond_signal() failed");
  }

  inline void CondBroadcast(ConditionVariable* var)
  {
    if (0 != pthread_cond_broadcast(&var->m_Impl))
      CroakErrno("pthread_cond_broadcast() failed");
  }
#elif defined(TUNDRA_WIN32)
  struct ConditionVariable
  {
    CONDITION_VARIABLE m_Impl;
  };

  inline void CondInit(ConditionVariable* var)
  {
    InitializeConditionVariable(&var->m_Impl);
  }

  inline void CondDestroy(ConditionVariable* var)
  {
    // nop
  }

  inline void CondWait(ConditionVariable* var, Mutex* mutex)
  {
    if (!SleepConditionVariableCS(&var->m_Impl, &mutex->m_Impl, INFINITE))
      CroakErrno("SleepConditionVariableCS() failed");
  }

  inline void CondSignal(ConditionVariable* var)
  {
    WakeConditionVariable(&var->m_Impl);
  }

  inline void CondBroadcast(ConditionVariable* var)
  {
    WakeAllConditionVariable(&var->m_Impl);
  }
#endif
}

#endif
