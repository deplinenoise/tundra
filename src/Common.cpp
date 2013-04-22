#include "Common.hpp"
#include "PathUtil.hpp"
#include "FileInfo.hpp"

#include <cstdio>
#include <cstdarg>
#include <cstdlib>
#include <cstring>

#if defined(TUNDRA_UNIX)
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <libgen.h>
#include <errno.h>
#endif

#if defined(TUNDRA_FREEBSD)
#include <sys/types.h>
#include <sys/sysctl.h>
#endif

#if defined(TUNDRA_WIN32)
#include <windows.h>
#endif

#if defined(TUNDRA_APPLE)
#include <mach-o/dyld.h>
#endif

namespace t2
{

#if defined(TUNDRA_WIN32)
static double s_PerfFrequency;
#endif

static bool DebuggerAttached()
{
#if defined(TUNDRA_WIN32)
  return IsDebuggerPresent() ? true : false;
#else
  return false;
#endif
}

void InitCommon(void)
{
#if defined(TUNDRA_WIN32)
  static LARGE_INTEGER freq;
	if (!QueryPerformanceFrequency(&freq))
		CroakErrno("QueryPerformanceFrequency failed");
  s_PerfFrequency = double(freq.QuadPart);

  SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX | SEM_NOALIGNMENTFAULTEXCEPT);
#endif
}

void NORETURN Croak(const char* fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  vfprintf(stderr, fmt, args);
  va_end(args);
  fprintf(stderr, "\n");
  if (DebuggerAttached())
    abort();
  else
    exit(1);
}

void NORETURN CroakErrno(const char* fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  vfprintf(stderr, fmt, args);
  va_end(args);
  fprintf(stderr, "\n");
  fprintf(stderr, "errno: %d (%s)\n", errno, strerror(errno));
  if (DebuggerAttached())
    abort();
  else
    exit(1);
}

void NORETURN CroakAbort(const char* fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  vfprintf(stderr, fmt, args);
  va_end(args);
  fprintf(stderr, "\n");
  abort();
}

uint32_t Djb2Hash(const char *str_)
{
	const uint8_t *str = (const uint8_t *) str_;
	uint32_t hash      = 5381;
	int c;

	while (0 != (c = *str++))
	{
		hash = (hash * 33) + c;
	}

	return hash;
}

uint64_t Djb2Hash64(const char *str_)
{
	const uint8_t *str = (const uint8_t *) str_;
	uint64_t hash      = 5381;
	int c;

	while (0 != (c = *str++))
	{
		hash = (hash * 33) + c;
	}

	return hash;
}

uint32_t Djb2HashNoCase(const char *str_)
{
	const uint8_t *str = (const uint8_t *) str_;
	uint32_t hash = 5381;
	int c;

	while (0 != (c = *str++))
	{
    // Branch free case folding for ASCII
    const int is_upper     = -(uint32_t(c - 'A') <= uint32_t('Z' - 'A'));
    const int lower_case_c = c | 0x20;
    const int nocase_c     = (lower_case_c & is_upper) | (c & ~is_upper);

		hash = (hash * 33) + nocase_c;
	}

	return hash;
}

uint64_t Djb2HashNoCase64(const char *str_)
{
	const uint8_t *str = (const uint8_t *) str_;
	uint64_t hash = 5381;
	int c;

	while (0 != (c = *str++))
	{
    // Branch free case folding for ASCII
    const int is_upper     = -(uint32_t(c - 'A') <= uint32_t('Z' - 'A'));
    const int lower_case_c = c | 0x20;
    const int nocase_c     = (lower_case_c & is_upper) | (c & ~is_upper);

		hash = (hash * 33) + nocase_c;
	}

	return hash;
}

static int s_LogFlags = 0;

void SetLogFlags(int log_flags)
{
  s_LogFlags = log_flags;
}

void Log(LogLevel level, const char* fmt, ...)
{
  if (s_LogFlags & level)
  {
    const char* prefix = "?";

    switch (level)
    {
      case kError   : prefix = "E"; break;
      case kWarning : prefix = "W"; break;
      case kInfo    : prefix = "I"; break;
      case kDebug   : prefix = "D"; break;
      case kSpam    : prefix = "S"; break;
      default       : break;
    }
    fprintf(stderr, "[%s] ", prefix);

    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);

    fprintf(stderr, "\n");
  }
}

void GetCwd(char* buffer, size_t buffer_size)
{
#if defined(TUNDRA_WIN32)
	DWORD res = GetCurrentDirectoryA((DWORD)buffer_size, buffer);
	if (0 == res || ((DWORD)buffer_size) <= res)
		Croak("couldn't get working directory");
#elif defined(TUNDRA_UNIX)
	if (NULL == getcwd(buffer, buffer_size))
		Croak("couldn't get working directory");
#else
#error Unsupported platform
#endif
}

bool SetCwd(const char* dir)
{
#if defined(TUNDRA_WIN32)
  return TRUE == SetCurrentDirectoryA(dir);
#elif defined(TUNDRA_UNIX)
  return 0 == chdir(dir);
#else
#error Unsupported platform
#endif
}

static char s_TundraHomeDir[kMaxPathLength];
static char s_TundraExePath[kMaxPathLength];

static const char *
SetPath(char (&var)[kMaxPathLength], const char* dir)
{
	strncpy(var, dir, sizeof var);
	var[(sizeof var) - 1] = '\0';
	return var;
}

const char* GetTundraHomeDirectory()
{
  if (s_TundraHomeDir[0] != '\0')
  {
    return s_TundraHomeDir;
  }

  char* tmp;

  // If TUNDRA_HOME is set, use that value.
  if (NULL != (tmp = getenv("TUNDRA_HOME")))
    return SetPath(s_TundraHomeDir, tmp);

  // Otherwise we need to try a little harder.
  {
    PathBuffer dir;
    PathInit(&dir, GetTundraExePath());

    while (PathStripLast(&dir))
    {
      PathBuffer test_file = dir;
      PathConcat(&test_file, "scripts/tundra.lua");
      char test_file_p[kMaxPathLength];
      PathFormat(test_file_p, &test_file);
      FileInfo info = GetFileInfo(test_file_p);
      if (info.Exists())
      {
        PathFormat(s_TundraHomeDir, &dir);
        return s_TundraHomeDir;
      }
    }
  }

  // On unixy platforms we will define a script home at build time derived
  // from the install PREFIX.
#ifdef TUNDRA_SCRIPT_HOME
  {
    PathBuffer dir;
    PathInit(&dir, TUNDRA_SCRIPT_HOME);

    PathBuffer test_file = dir;
    PathConcat(&test_file, "tundra.lua");
    char test_file_p[kMaxPathLength];
    PathFormat(test_file_p, &test_file);
    FileInfo info = GetFileInfo(test_file_p);
    if (info.Exists())
    {
        PathFormat(s_TundraHomeDir, &dir);
        return s_TundraHomeDir;
    }
  }
#endif

  Croak("Can't detect tundra home directory. Please set TUNDRA_HOME.");
}

const char* GetTundraExePath()
{
  if (s_TundraExePath[0] == '\0')
  {
#if defined(TUNDRA_WIN32)
    if (!GetModuleFileNameA(NULL, s_TundraExePath, (DWORD)sizeof s_TundraExePath))
      Croak("couldn't get module filename");
#elif defined(TUNDRA_APPLE)
    uint32_t size = sizeof s_TundraExePath;
    if (0 != _NSGetExecutablePath(s_TundraExePath, &size))
      Croak("_NSGetExecutablePath failed");
#elif defined(TUNDRA_LINUX)
    if (-1 == readlink("/proc/self/exe", s_TundraExePath, (sizeof s_TundraExePath) - 1))
      Croak("couldn't read /proc/self/exe to get exe path: %s", strerror(errno));
#elif defined(TUNDRA_FREEBSD)
    size_t cb = sizeof s_TundraExePath;
    int mib[4];
    mib[0] = CTL_KERN;
    mib[1] = KERN_PROC;
    mib[2] = KERN_PROC_PATHNAME;
    mib[3] = -1;
    if (0 !=sysctl(mib, 4, s_TundraExePath, &cb, NULL, 0))
      Croak("KERN_PROC_PATHNAME syscall failed");
#else
#error "unsupported platform"
#endif
  }

  return s_TundraExePath;
}

uint32_t NextPowerOfTwo(uint32_t val)
{
  uint32_t mask = val - 1;

  mask |= mask >> 16;
  mask |= mask >> 8;
  mask |= mask >> 4;
  mask |= mask >> 2;
  mask |= mask >> 1;

  uint32_t pow2 = mask + 1;
  CHECK(pow2 >= val);
  CHECK((pow2 & mask) == 0);
  CHECK((pow2 & ~mask) == pow2);

  return pow2;
}

uint64_t TimerGet()
{
#if defined(TUNDRA_UNIX)
	struct timeval t;
	if (0 != gettimeofday(&t, NULL))
		CroakErrno("gettimeofday failed");
  return t.tv_usec + uint64_t(t.tv_sec) * 1000000;
#elif defined(TUNDRA_WIN32)
	LARGE_INTEGER c;
	if (!QueryPerformanceCounter(&c))
		CroakErrno("QueryPerformanceCounter failed");
	return c.QuadPart;
#endif
}

double TimerToSeconds(uint64_t t)
{
#if defined(TUNDRA_UNIX)
  return t / 1000000.0;
#else
  return double(t) / s_PerfFrequency;
#endif
}

double TimerDiffSeconds(uint64_t start, uint64_t end)
{
  return TimerToSeconds(end - start);
}

bool MakeDirectory(const char* path)
{
#if defined(TUNDRA_UNIX)
	int rc = mkdir(path, 0777);
	if (0 == rc || EEXIST == errno)
		return true;
	else
		return false;
#elif defined(TUNDRA_WIN32)
	/* pretend we can always create device roots */
	if (isalpha(path[0]) && 0 == memcmp(&path[1], ":\\\0", 3))
		return true;

	if (!CreateDirectoryA(path, NULL))
	{
		switch (GetLastError())
		{
		case ERROR_ALREADY_EXISTS:
			return true;
		default:
			return false;
		}
	}
	else
		return true;
#endif

}

int GetCpuCount()
{
#if defined(TUNDRA_WIN32)
	SYSTEM_INFO si;
	GetSystemInfo(&si);
	return (int) si.dwNumberOfProcessors;
#else
	long nprocs_max = sysconf(_SC_NPROCESSORS_CONF);
	if (nprocs_max < 0)
		CroakErrno("couldn't get CPU count");
	return (int) nprocs_max;
#endif
}

int CountTrailingZeroes(uint32_t v)
{
  v &= -int32_t(v);

  int bit_index = 32;

  if (v)
    --bit_index;

  if (v & 0x0000ffff) bit_index -= 16;
  if (v & 0x00ff00ff) bit_index -= 8;
  if (v & 0x0f0f0f0f) bit_index -= 4;
  if (v & 0x33333333) bit_index -= 2;
  if (v & 0x55555555) bit_index -= 1;

  return bit_index;
}

}

