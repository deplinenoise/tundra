#include "ConditionVar.hpp"
#include "ReadWriteLock.hpp"

#if defined(TUNDRA_WIN32)
#if DISABLED(TUNDRA_WIN32_VISTA_APIS)

#include <windows.h>

void t2::CondInit(t2::ConditionVariable* var)
{
  var->waiters_count_ = 0;
  var->was_broadcast_ = 0;

  var->sema_ = CreateSemaphore(NULL, 0, 0x7fffffff, NULL);
  if (!var->sema_)
    CroakErrno("CreateSemaphore failed");

  InitializeCriticalSection(&var->waiters_count_lock_);

  var->waiters_done_ = CreateEvent(NULL, FALSE, FALSE, NULL);
  if (!var->waiters_done_)
    CroakErrno("CreateEvent failed");
}

void t2::CondDestroy(t2::ConditionVariable* var)
{
  DeleteCriticalSection(&var->waiters_count_lock_);
  CloseHandle(var->sema_);
  CloseHandle(var->waiters_done_);
}

void t2::CondWait(t2::ConditionVariable* var, t2::Mutex* mutex)
{
  HANDLE external_mutex = mutex->m_Impl;

  // Avoid race conditions.
  EnterCriticalSection(&var->waiters_count_lock_);
  var->waiters_count_++;
  LeaveCriticalSection(&var->waiters_count_lock_);

  // This call atomically releases the mutex and waits on the
  // semaphore until <pthread_cond_signal> or <pthread_cond_broadcast>
  // are called by another thread.
  if (WAIT_OBJECT_0 != SignalObjectAndWait(external_mutex, var->sema_, INFINITE, FALSE))
    CroakErrno("SignalObjectAndWait failed");

  // Reacquire lock to avoid race conditions.
  EnterCriticalSection(&var->waiters_count_lock_);

  // We're no longer waiting...
  var->waiters_count_--;

  // Check to see if we're the last waiter after <pthread_cond_broadcast>.
  int last_waiter = var->was_broadcast_ && var->waiters_count_ == 0;

  LeaveCriticalSection(&var->waiters_count_lock_);

  // If we're the last waiter thread during this particular broadcast
  // then let all the other threads proceed.
  if (last_waiter)
  {
    // This call atomically signals the <waiters_done_> event and waits until
    // it can acquire the <external_mutex>.  This is required to ensure fairness. 
    if (WAIT_OBJECT_0 != SignalObjectAndWait(var->waiters_done_, external_mutex, INFINITE, FALSE))
      CroakErrno("SignalObjectAndWait failed");
  }
  else
  {
    // Always regain the external mutex since that's the guarantee we
    // give to our callers. 
    if (WAIT_OBJECT_0 != WaitForSingleObject(external_mutex, INFINITE))
      CroakErrno("WaitForSingleObject failed");
  }
}

void t2::CondSignal(t2::ConditionVariable* var)
{
  EnterCriticalSection(&var->waiters_count_lock_);
  int have_waiters = var->waiters_count_ > 0;
  LeaveCriticalSection(&var->waiters_count_lock_);

  // If there aren't any waiters, then this is a no-op.  
  if (have_waiters)
  {
    if (!ReleaseSemaphore(var->sema_, 1, 0))
      CroakErrno("ReleaseSemaphore failed");
  }
}

void t2::CondBroadcast(t2::ConditionVariable* var)
{
  int have_waiters = 0;

  // This is needed to ensure that <waiters_count_> and <was_broadcast_> are
  // consistent relative to each other.
  EnterCriticalSection(&var->waiters_count_lock_);

  if (var->waiters_count_ > 0)
  {
    // We are broadcasting, even if there is just one waiter...
    // Record that we are broadcasting, which helps optimize
    // <pthread_cond_wait> for the non-broadcast case.
    var->was_broadcast_ = 1;
    have_waiters = 1;
  }

  if (have_waiters)
  {
    // Wake up all the waiters atomically.
    if (!ReleaseSemaphore(var->sema_, var->waiters_count_, 0))
      CroakErrno("ReleaseSemaphore failed");

    LeaveCriticalSection(&var->waiters_count_lock_);

    // Wait for all the awakened threads to acquire the counting
    // semaphore. 
    if (WAIT_OBJECT_0 != WaitForSingleObject(var->waiters_done_, INFINITE))
      CroakErrno("WaitForSingleObject failed");

    // This assignment is okay, even without the <waiters_count_lock_> held 
    // because no other waiter threads can wake up to access it.
    var->was_broadcast_ = 0;
  }
  else
  {
    LeaveCriticalSection(&var->waiters_count_lock_);
  }
}

#endif
#endif
