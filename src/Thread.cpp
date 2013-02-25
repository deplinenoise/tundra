#include "Thread.hpp"

#if defined(TUNDRA_UNIX)
#include <pthread.h>
#endif

#if defined(TUNDRA_WIN32)
#include <windows.h>
#include <process.h>
#endif

namespace t2
{

ThreadId ThreadCurrent()
{
#if defined(TUNDRA_UNIX)
  static_assert(sizeof(pthread_t) <= sizeof(ThreadId), "pthread_t too big");
  return (ThreadId) pthread_self();
#elif defined(TUNDRA_WIN32)
  return ::GetCurrentThreadId();
#endif
}

ThreadId ThreadStart(ThreadRoutine routine, void *param)
{
#if defined(TUNDRA_UNIX)
  pthread_t thread;
  if (0 != pthread_create(&thread, nullptr, routine, param))
    CroakErrno("pthread_create() failed");
  return (ThreadId) thread;
#else
  uintptr_t handle = _beginthreadex(NULL, 0, routine, param, 0, NULL);
  if (!handle)
    CroakErrno("_beginthreadex() failed");
  return handle;
#endif
}

void ThreadJoin(ThreadId thread_id)
{
#if defined(TUNDRA_UNIX)
  void* result;
  if (0 != pthread_join((pthread_t)thread_id, &result))
    CroakErrno("pthread_join() failed");
  return;
#else
  while (WAIT_OBJECT_0 != WaitForSingleObject((HANDLE) thread_id, INFINITE))
  {
    // nop
  }
  CloseHandle((HANDLE) thread_id);
#endif
}

}
