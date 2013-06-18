#ifndef SIGNALHANDLER_HPP
#define SIGNALHANDLER_HPP

#include "Config.hpp"

namespace t2
{
  struct Mutex;
  struct ConditionVariable;

  // Return non-null if the processes has been signalled to quit.
  const char* SignalGetReason(void);

  // Call to explicitly mark the current process as signalled.  Useful to pick
  // up child processes dying after being signalled and propagating that up to
  // all build threads.
  void SignalSet(const char* reason);

  // Init the signal handler.
  void SignalHandlerInit(void);

#if defined(TUNDRA_WIN32)
  // Init the signal handler with a parent canary process to watch for sudden termination.
  void SignalHandlerInitWithParentProcess(void* parent_handle);

  // Get a win32 event handle which will be signalled when the build is aborted, so we can terminate launched programs immediately.
  void* SignalGetHandle();
#endif

  // Specify a condition variable which will be broadcast when a signal has
  // arrived.
  void SignalHandlerSetCondition(ConditionVariable *variable);

  // Block (or unblock) all normal interruption signals for the calling thread.
#if defined(TUNDRA_UNIX)
  void SignalBlockThread(bool block);
#else
  // Windows doesn't deliver signals to threads, it creates a new signal
  // handler thread for you.
  inline void SignalBlockThread(bool) {}
#endif
}

#endif
