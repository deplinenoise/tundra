#include "SignalHandler.hpp"
#include "Config.hpp"
#include "Mutex.hpp"
#include "ConditionVar.hpp"
#include <stdio.h>

#if defined(TUNDRA_UNIX)
#include <signal.h>
#include <sys/signal.h>
#include <pthread.h>
#endif

namespace t2
{

#if defined(TUNDRA_WIN32)
HANDLE s_SignalHandle;
#endif

static struct
{
  bool                    m_Signalled;
  const char*             m_Reason;
} s_SignalInfo;

static Mutex              s_SignalMutex;
static ConditionVariable *s_SignalCond;

const char* SignalGetReason(void)
{
  MutexLock(&s_SignalMutex);
  const char* result = s_SignalInfo.m_Reason;
  MutexUnlock(&s_SignalMutex);
  return result;
}

void SignalSet(const char* reason)
{
  MutexLock(&s_SignalMutex);

  s_SignalInfo.m_Signalled = true;
  s_SignalInfo.m_Reason    = reason;

  if (ConditionVariable* cvar = s_SignalCond)
    CondBroadcast(cvar);

  MutexUnlock(&s_SignalMutex);

#if defined(TUNDRA_WIN32)
  // Also signal this event - build threads waiting on external programs are stuck in WaitForMultipleObjects()
  // and can't wait for a condition variable at the same time.
  SetEvent(s_SignalHandle);
#endif
}

#if defined(TUNDRA_UNIX)
static void* PosixSignalHandlerThread(void *arg)
{
  (void)arg; // unused

  int sig, rc;
  sigset_t sigs;
  sigemptyset(&sigs);
  sigaddset(&sigs, SIGINT);
  sigaddset(&sigs, SIGTERM);
  sigaddset(&sigs, SIGQUIT);
  if (0 == (rc = sigwait(&sigs, &sig)))
  {
    const char *reason = "unknown";
    switch (sig)
    {
      case SIGINT:  reason = "SIGINT";  break;
      case SIGTERM: reason = "SIGTERM"; break;
      case SIGQUIT: reason = "SIGQUIT"; break;
    }
    SignalSet(reason);
  }
  else
    CroakErrno("sigwait() failed");

  return nullptr;
}
#endif

#if defined(TUNDRA_WIN32)
BOOL WINAPI WindowsSignalHandlerFunc(DWORD ctrl_type)
{
  const char *reason = NULL;

  switch (ctrl_type)
  {
    case CTRL_C_EVENT:
      reason = "Ctrl+C";
      break;

    case CTRL_BREAK_EVENT:
      reason = "Ctrl+Break";
      break;
  }

  if (reason)
  {
    SignalSet(reason);
    return TRUE;
  }

  return FALSE;
}
#endif

void SignalHandlerInit()
{
  MutexInit(&s_SignalMutex);

#if defined(TUNDRA_UNIX)
	{
		pthread_t sigthread;
		if (0 != pthread_create(&sigthread, NULL, PosixSignalHandlerThread, NULL))
			CroakErrno("couldn't start signal handler thread");
		pthread_detach(sigthread);
	}
#elif defined(TUNDRA_WIN32)
  s_SignalHandle = CreateEvent(NULL, TRUE, FALSE, NULL);
  SetConsoleCtrlHandler(WindowsSignalHandlerFunc, TRUE);
#else
#error Meh
#endif
}

#if defined(TUNDRA_WIN32)
static DWORD WINAPI CanaryWatcherThread(LPVOID parent_handle)
{
  WaitForSingleObject(parent_handle, INFINITE);
  SignalSet("Process terminated");
  return 0;
}

void* SignalGetHandle()
{
  return s_SignalHandle;
}

void SignalHandlerInitWithParentProcess(void* parent_handle)
{
  SignalHandlerInit();
  HANDLE t = CreateThread(NULL, 16 * 1024, CanaryWatcherThread, parent_handle, 0, NULL);

  if (nullptr == t)
    CroakErrno("Failed to create canary watcher thread");

  CloseHandle(t);
}
#endif

void SignalHandlerShutdown()
{
  MutexInit(&s_SignalMutex);
}

void SignalHandlerSetCondition(ConditionVariable *cvar)
{
  MutexLock(&s_SignalMutex);
  s_SignalCond = cvar;
  MutexUnlock(&s_SignalMutex);
}

#if defined(TUNDRA_UNIX)
void SignalBlockThread(bool block)
{
	sigset_t sigs;
	sigemptyset(&sigs);
	sigaddset(&sigs, SIGINT);
	sigaddset(&sigs, SIGTERM);
	sigaddset(&sigs, SIGQUIT);
	if  (0 != pthread_sigmask(block ? SIG_BLOCK : SIG_UNBLOCK, &sigs, 0))
		CroakErrno("pthread_sigmask failed");
}
#endif

}
