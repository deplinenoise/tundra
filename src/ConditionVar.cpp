#include "ConditionVar.hpp"
#include "ReadWriteLock.hpp"

#if defined(TUNDRA_WIN32) && (defined(__GNUC__) || WINVER < 0x0600)

// All of this is a ghetto hack to support MinGW.

#include <stdio.h>
#include <stdlib.h>

extern "C" {
  void (WINAPI *InitializeConditionVariable)(PCONDITION_VARIABLE ConditionVariable);
  BOOL (WINAPI *SleepConditionVariableCS)(PCONDITION_VARIABLE ConditionVariable, PCRITICAL_SECTION CriticalSection, DWORD dwMilliseconds);
  void (WINAPI *WakeConditionVariable)(PCONDITION_VARIABLE ConditionVariable);
  void (WINAPI *WakeAllConditionVariable)(PCONDITION_VARIABLE ConditionVariable);

  void (WINAPI *InitializeSRWLock)(PSRWLOCK);
  void (WINAPI *AcquireSRWLockExclusive)(PSRWLOCK);
  void (WINAPI *AcquireSRWLockShared)(PSRWLOCK);
  void (WINAPI *ReleaseSRWLockExclusive)(PSRWLOCK);
  void (WINAPI *ReleaseSRWLockShared)(PSRWLOCK);
}

static bool DoInitConditionVars()
{
  // Try to locate Vista and later condition variable APIs
  static const struct
  {
    FARPROC *ptr; const char *symbol;
  }
  init_table[] =
  {
    { (FARPROC*) &InitializeConditionVariable, "InitializeConditionVariable" },
    { (FARPROC*) &SleepConditionVariableCS,    "SleepConditionVariableCS"    },
    { (FARPROC*) &WakeConditionVariable,       "WakeConditionVariable"       },
    { (FARPROC*) &WakeAllConditionVariable,    "WakeAllConditionVariable"    },
    { (FARPROC*) &InitializeSRWLock,           "InitializeSRWLock"           },
    { (FARPROC*) &AcquireSRWLockExclusive,     "AcquireSRWLockExclusive"     },
    { (FARPROC*) &AcquireSRWLockShared,        "AcquireSRWLockShared"        },
    { (FARPROC*) &ReleaseSRWLockExclusive,     "ReleaseSRWLockExclusive"     },
    { (FARPROC*) &ReleaseSRWLockShared,        "ReleaseSRWLockShared"        },
  };

  HMODULE kernel32 = GetModuleHandleA("kernel32.dll");

  for (size_t i = 0; i < ARRAY_SIZE(init_table); ++i)
  {
    const char *symbol = init_table[i].symbol;

    if (NULL == (*init_table[i].ptr = GetProcAddress(kernel32, symbol)))
    {
      char message[512];
      _snprintf(message, sizeof message, "Missing function: %s\n\nYour version of Windows is not supported.",
          symbol);
      MessageBox(NULL, message, "Tundra Init Error", MB_OK|MB_ICONEXCLAMATION);
      exit(1);
    }
  }

  return true;
}

bool s_InitConditionVarsFuncs = DoInitConditionVars();

#endif
