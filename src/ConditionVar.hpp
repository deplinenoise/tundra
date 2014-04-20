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
#if ENABLED(TUNDRA_WIN32_VISTA_APIS)
    CONDITION_VARIABLE m_Impl;
#else
  // Number of waiting threads.
  int waiters_count_;

  // Serialize access to <waiters_count_>.
  CRITICAL_SECTION waiters_count_lock_;

  // Semaphore used to queue up threads waiting for the condition to
  // become signaled. 
  HANDLE sema_;

  // An auto-reset event used by the broadcast/signal thread to wait
  // for all the waiting thread(s) to wake up and be released from the
  // semaphore. 
  HANDLE waiters_done_;

  // Keeps track of whether we were broadcasting or signaling.  This
  // allows us to optimize the code if we're just signaling.
  size_t was_broadcast_;
#endif
  };

  void CondInit(ConditionVariable* var);
  void CondDestroy(ConditionVariable* var);
  void CondWait(ConditionVariable* var, Mutex* mutex);
  void CondSignal(ConditionVariable* var);
  void CondBroadcast(ConditionVariable* var);

#if ENABLED(TUNDRA_WIN32_VISTA_APIS)
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
#endif // ENABLED(TUNDRA_WIN32_VISTA_APIS)

#endif // TUNDRA_WIN32
}

#endif
