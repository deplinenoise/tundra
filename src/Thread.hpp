#ifndef THREAD_HPP
#define THREAD_HPP

#include "Common.hpp"

namespace t2
{

  typedef uintptr_t ThreadId;

#if defined(TUNDRA_WIN32)
  typedef unsigned int ThreadRoutineReturnType;
#else
  typedef void* ThreadRoutineReturnType;
#endif

  typedef ThreadRoutineReturnType (TUNDRA_STDCALL * ThreadRoutine)(void*);

  ThreadId ThreadStart(ThreadRoutine routine, void *param);

  void ThreadJoin(ThreadId thread_id);

  ThreadId ThreadCurrent();
}

#endif
