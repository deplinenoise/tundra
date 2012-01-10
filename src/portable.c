/*
   Copyright 2010 Andreas Fredriksson

   This file is part of Tundra.

   Tundra is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Tundra is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Tundra.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "portable.h"
#include "engine.h"
#include "util.h"
#include "lua.h"
#include "lauxlib.h"
#include "config.h"

#include <string.h>
#include <ctype.h>

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

#if defined(TUNDRA_APPLE)
#include <mach-o/dyld.h>
#endif

#if defined(TUNDRA_WIN32)
#include <windows.h>
#include <stdlib.h>
#include <errno.h>
#include <process.h>
#include <stdio.h>
#include <assert.h>

#if defined(_MSC_VER)
#define snprintf _snprintf
#endif

#if defined(__GNUC__) || WINVER < 0x0600
/* mingw has very old windows headers; declare CONDITION VARIABLE here */
typedef void* CONDITION_VARIABLE;
typedef CONDITION_VARIABLE* PCONDITION_VARIABLE;
#endif

/* Windows Vista condition variable interface. We use this if it is available. */
static int use_vista_apis;
static void (WINAPI *pf_InitializeConditionVariable)(PCONDITION_VARIABLE ConditionVariable);
BOOL (WINAPI *pf_SleepConditionVariableCS)(PCONDITION_VARIABLE ConditionVariable, PCRITICAL_SECTION CriticalSection, DWORD dwMilliseconds);
static void (WINAPI *pf_WakeConditionVariable)(PCONDITION_VARIABLE ConditionVariable);
static void (WINAPI *pf_WakeAllConditionVariable)(PCONDITION_VARIABLE ConditionVariable);

#ifndef KEY_WOW64_64KEY
#define KEY_WOW64_64KEY         (0x0100)
#endif
#ifndef KEY_WOW64_32KEY
#define KEY_WOW64_32KEY         (0x0200)
#endif
#endif

const char * const td_platform_string =
#if defined(TUNDRA_APPLE)
	"macosx";
#elif defined(TUNDRA_LINUX)
	"linux";
#elif defined(TUNDRA_WIN32)
	"windows";
#elif defined(TUNDRA_FREEBSD)
	"freebsd";
#elif defined(TUNDRA_OPENBSD)
	"openbsd";
#else
#error implement me
#endif

#if defined(TUNDRA_WIN32)

/* See http://www.cse.wustl.edu/~schmidt/win32-cv-1.html */

typedef struct
{
	int waiters_count_;
	// Number of waiting threads.

	CRITICAL_SECTION waiters_count_lock_;
	// Serialize access to <waiters_count_>.

	HANDLE sema_;
	// Semaphore used to queue up threads waiting for the condition to
	// become signaled. 

	HANDLE waiters_done_;
	// An auto-reset event used by the broadcast/signal thread to wait
	// for all the waiting thread(s) to wake up and be released from the
	// semaphore. 

	size_t was_broadcast_;
	// Keeps track of whether we were broadcasting or signaling.  This
	// allows us to optimize the code if we're just signaling.
} winxp_pthread_cond_t;


int pthread_mutex_init(pthread_mutex_t *mutex, void *args)
{
	TD_UNUSED(args);

	if (use_vista_apis)
	{
		mutex->handle = (PCRITICAL_SECTION)malloc(sizeof(CRITICAL_SECTION));
		if (!mutex->handle)
			return ENOMEM;
		InitializeCriticalSection((PCRITICAL_SECTION)mutex->handle);
	}
	else
	{
		if (NULL == (mutex->handle = CreateMutex(NULL, FALSE, NULL)))
			return ENOMEM;
	}
	return 0;
}

int pthread_mutex_destroy(pthread_mutex_t *mutex)
{
	if (use_vista_apis)
	{
		DeleteCriticalSection((PCRITICAL_SECTION)mutex->handle);
		free(mutex->handle);
	}
	else
	{
		CloseHandle(mutex->handle);
	}
	mutex->handle = 0;
	return 0;
}

int pthread_mutex_lock(pthread_mutex_t *lock)
{
	if (use_vista_apis)
		EnterCriticalSection((PCRITICAL_SECTION)lock->handle);
	else
		WaitForSingleObject(lock->handle, INFINITE);
	return 0;
}

int pthread_mutex_unlock(pthread_mutex_t *lock)
{
	if (use_vista_apis)
		LeaveCriticalSection((PCRITICAL_SECTION)lock->handle);
	else
		ReleaseMutex(lock->handle);
	return 0;
}

int pthread_cond_init(pthread_cond_t *cond, void *args)
{
	TD_UNUSED(args);
	assert(args == NULL);

	if (use_vista_apis)
	{
		if (NULL == (cond->handle = malloc(sizeof(CONDITION_VARIABLE))))
			return ENOMEM;

		pf_InitializeConditionVariable((PCONDITION_VARIABLE) cond->handle);
	}
	else
	{
		winxp_pthread_cond_t *cv = malloc(sizeof(winxp_pthread_cond_t));
		if (!cv)
			return ENOMEM;
		cv->waiters_count_ = 0;
		cv->was_broadcast_ = 0;
		cv->sema_ = CreateSemaphore(NULL, 0, 0x7fffffff, NULL);
		InitializeCriticalSection(&cv->waiters_count_lock_);
		cv->waiters_done_ = CreateEvent(NULL, FALSE, FALSE, NULL);

		cond->handle = cv;
	}
	return 0;
}

int pthread_cond_destroy(pthread_cond_t *cond)
{
	if (!use_vista_apis)
	{
		winxp_pthread_cond_t *cv = cond->handle;
		DeleteCriticalSection(&cv->waiters_count_lock_);
		CloseHandle(cv->sema_);
		CloseHandle(cv->waiters_done_);
	}
	free(cond->handle);
	return 0;
}

int pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t* lock)
{
	if (use_vista_apis)
	{
		BOOL result = pf_SleepConditionVariableCS((PCONDITION_VARIABLE) cond->handle, (PCRITICAL_SECTION) lock->handle, INFINITE);
		return !result;
	}
	else
	{
		int last_waiter;
		winxp_pthread_cond_t *cv = cond->handle;
		HANDLE external_mutex = lock->handle;

		// Avoid race conditions.
		EnterCriticalSection(&cv->waiters_count_lock_);
		cv->waiters_count_++;
		LeaveCriticalSection(&cv->waiters_count_lock_);

		// This call atomically releases the mutex and waits on the
		// semaphore until <pthread_cond_signal> or <pthread_cond_broadcast>
		// are called by another thread.
		SignalObjectAndWait(external_mutex, cv->sema_, INFINITE, FALSE);

		// Reacquire lock to avoid race conditions.
		EnterCriticalSection(&cv->waiters_count_lock_);

		// We're no longer waiting...
		cv->waiters_count_--;

		// Check to see if we're the last waiter after <pthread_cond_broadcast>.
		last_waiter = cv->was_broadcast_ && cv->waiters_count_ == 0;

		LeaveCriticalSection(&cv->waiters_count_lock_);

		// If we're the last waiter thread during this particular broadcast
		// then let all the other threads proceed.
		if (last_waiter)
			// This call atomically signals the <waiters_done_> event and waits until
			// it can acquire the <external_mutex>.  This is required to ensure fairness. 
			SignalObjectAndWait(cv->waiters_done_, external_mutex, INFINITE, FALSE);
		else
			// Always regain the external mutex since that's the guarantee we
			// give to our callers. 
			WaitForSingleObject(external_mutex, INFINITE);

		return 0;
	}
}

int pthread_cond_broadcast(pthread_cond_t *cond)
{
	if (use_vista_apis)
		pf_WakeAllConditionVariable((PCONDITION_VARIABLE) cond->handle);
	else
	{
		winxp_pthread_cond_t *cv = cond->handle;
		int have_waiters = 0;

		// This is needed to ensure that <waiters_count_> and <was_broadcast_> are
		// consistent relative to each other.
		EnterCriticalSection(&cv->waiters_count_lock_);

		if (cv->waiters_count_ > 0)
		{
			// We are broadcasting, even if there is just one waiter...
			// Record that we are broadcasting, which helps optimize
			// <pthread_cond_wait> for the non-broadcast case.
			cv->was_broadcast_ = 1;
			have_waiters = 1;
		}

		if (have_waiters)
		{
			// Wake up all the waiters atomically.
			ReleaseSemaphore(cv->sema_, cv->waiters_count_, 0);

			LeaveCriticalSection(&cv->waiters_count_lock_);

			// Wait for all the awakened threads to acquire the counting
			// semaphore. 
			WaitForSingleObject(cv->waiters_done_, INFINITE);

			// This assignment is okay, even without the <waiters_count_lock_> held 
			// because no other waiter threads can wake up to access it.
			cv->was_broadcast_ = 0;
		}
		else
			LeaveCriticalSection(&cv->waiters_count_lock_);
	}
	return 0;
}

int pthread_cond_signal(pthread_cond_t *cond)
{
	if (use_vista_apis)
		pf_WakeConditionVariable((PCONDITION_VARIABLE) cond->handle);
	else
	{
		winxp_pthread_cond_t *cv = cond->handle;
		int have_waiters;

		EnterCriticalSection(&cv->waiters_count_lock_);
		have_waiters = cv->waiters_count_ > 0;
		LeaveCriticalSection(&cv->waiters_count_lock_);

		// If there aren't any waiters, then this is a no-op.  
		if (have_waiters)
			ReleaseSemaphore(cv->sema_, 1, 0);
	}
	return 0;
}

typedef struct
{
	uintptr_t thread;
	pthread_thread_routine func;
	void *input_arg;
	void *result;
	volatile LONG taken;
} thread_data;

enum { W32_MAX_THREADS = 32 };
static thread_data setup[W32_MAX_THREADS];

static thread_data* alloc_thread(void)
{
	int i;
	for (i = 0; i < W32_MAX_THREADS; ++i)
	{
		thread_data* p = &setup[i];
		if (0 == InterlockedExchange(&p->taken, 1))
			return p;
	}

	fprintf(stderr, "fatal: too many threads allocated (limit: %d)\n", W32_MAX_THREADS);
	exit(1);
}

static unsigned __stdcall thread_start(void *args_)
{
	thread_data* args = (thread_data*) args_;
	args->result = (*args->func)(args->input_arg);
	return 0;
}

int pthread_create(pthread_t *result, void* options, pthread_thread_routine start, void *routine_arg)
{
	thread_data* data = alloc_thread();
	TD_UNUSED(options);
	data->func = start;
	data->input_arg = routine_arg;
	data->thread = _beginthreadex(NULL, 0, thread_start, data, 0, NULL);
	result->index = (int) (data - &setup[0]);
	return 0;
}

int pthread_join(pthread_t thread, void **result_out)
{
	thread_data *data = &setup[thread.index];
	assert(thread.index >= 0 && thread.index < W32_MAX_THREADS);
	assert(data->taken);

	if (WAIT_OBJECT_0 == WaitForSingleObject((HANDLE) data->thread, INFINITE))
	{
		CloseHandle((HANDLE) data->thread);
		*result_out = data->result;
		memset(data, 0, sizeof(thread_data));
		return 0;
	}
	else
	{
		return EINVAL;
	}
}

#endif

int
td_mkdir(const char *path)
{
#if defined(TUNDRA_UNIX)
	int rc = mkdir(path, 0777);
	if (0 == rc || EEXIST == errno)
		return 0;
	else
	{
		perror("mkdir");
		return rc;
	}
#elif defined(TUNDRA_WIN32)
	/* pretend we can always create device roots */
	if (isalpha(path[0]) && 0 == memcmp(&path[1], ":\\\0", 3))
		return 0;

	if (!CreateDirectoryA(path, NULL))
	{
		switch (GetLastError())
		{
		case ERROR_ALREADY_EXISTS:
			return 0;
		default:
			return 1;
		}
	}
	else
		return 0;
#endif
}

int
td_mkdir_lua(lua_State *L)
{
	const char *path = luaL_checkstring(L, 1);
	lua_pushinteger(L, td_mkdir(path));
	return 1;
}

int
td_rmdir(const char *path)
{
#if defined(TUNDRA_UNIX)
	return rmdir(path);
#elif defined(TUNDRA_WIN32)
	if (RemoveDirectoryA(path))
		return 0;
	else
		return 1;
#else
#error meh
#endif

}

int
fs_stat_file(const char *path, td_stat *out)
{
#if defined(TUNDRA_UNIX)
	int err;
	struct stat s;
	if (0 != (err = stat(path, &s)))
		return err;

	out->flags = TD_STAT_EXISTS | ((s.st_mode & S_IFDIR) ? TD_STAT_DIR : 0);
	out->size = s.st_size;
	out->timestamp = s.st_mtime;
	return 0;
#elif defined(TUNDRA_WIN32)
#define EPOCH_DIFF 0x019DB1DED53E8000LL /* 116444736000000000 nsecs */
#define RATE_DIFF 10000000 /* 100 nsecs */
	WIN32_FIND_DATAA data;
	HANDLE h = FindFirstFileA(path, &data);
	unsigned __int64 ft;
	if (h == INVALID_HANDLE_VALUE)
		return 1;
	out->flags = TD_STAT_EXISTS | ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? TD_STAT_DIR : 0);
	out->size = ((unsigned __int64)(data.nFileSizeHigh) << 32) | data.nFileSizeLow;
	ft = ((unsigned __int64)(data.ftLastWriteTime.dwHighDateTime) << 32) | data.ftLastWriteTime.dwLowDateTime;
	out->timestamp = (unsigned long) ((ft - EPOCH_DIFF) / RATE_DIFF);
	FindClose(h);
	return 0;
#else
#error meh
	return 0;
#endif
}

int td_move_file(const char *source, const char *dest)
{
#if defined(TUNDRA_UNIX)
	return rename(source, dest);
#elif defined(TUNDRA_WIN32)
	if (MoveFileExA(source, dest, MOVEFILE_REPLACE_EXISTING))
		return 0;
	else
		return GetLastError();
#else
#error meh
#endif

}

#if defined(TUNDRA_WIN32)
static double perf_to_secs;
static LARGE_INTEGER initial_time;
#elif defined(TUNDRA_UNIX)
static double start_time;
static double dtime_now(void)
{
	static const double micros = 1.0/1000000.0;
	struct timeval t;
	if (0 != gettimeofday(&t, NULL))
		td_croak("gettimeofday failed");
	return t.tv_sec + t.tv_usec * micros;
}
#endif

static void init_timer(void)
{
#if defined(TUNDRA_UNIX)
	start_time = dtime_now();
#elif defined(TUNDRA_WIN32)
	LARGE_INTEGER perf_freq;
	if (!QueryPerformanceFrequency(&perf_freq))
		td_croak("QueryPerformanceFrequency failed: %d", (int) GetLastError());
	if (!QueryPerformanceCounter(&initial_time))
		td_croak("QueryPerformanceCounter failed: %d", (int) GetLastError());
	perf_to_secs = 1.0 / (double) perf_freq.QuadPart;
#endif
}

#if defined(TUNDRA_WIN32)
char *win32_env_block;
#endif

void td_init_portable(void)
{
	init_timer();

#if defined(TUNDRA_WIN32)
	/* Grab the environment block once and just let it leak. */
	win32_env_block = GetEnvironmentStringsA();

	/* Try to locate Vista condition variable API. */
	{
		int i;
		int api_count = 0;
		HMODULE kernel32;

		static const struct { FARPROC *ptr; const char *symbol; } init_table[] = {
			{ (FARPROC*) &pf_InitializeConditionVariable, "InitializeConditionVariable" },
			{ (FARPROC*) &pf_SleepConditionVariableCS, "SleepConditionVariableCS" },
			{ (FARPROC*) &pf_WakeConditionVariable, "WakeConditionVariable" },
			{ (FARPROC*) &pf_WakeAllConditionVariable, "WakeAllConditionVariable" },
		};

		kernel32 = GetModuleHandleA("kernel32.dll");

		for (i = 0; i < (sizeof(init_table)/sizeof(init_table[0])); ++i)
		{
			const char *symbol = init_table[i].symbol;
			if (NULL != (*init_table[i].ptr = GetProcAddress(kernel32, symbol)))
				++api_count;
		}

		if (api_count == (sizeof(init_table)/sizeof(init_table[0])))
		{
			use_vista_apis = 1;
		}
	}

#endif
}

double td_timestamp(void)
{
#if defined(TUNDRA_UNIX)
	return dtime_now() - start_time;
#elif defined(TUNDRA_WIN32)
	LARGE_INTEGER c;
	if (!QueryPerformanceCounter(&c))
		td_croak("QueryPerformanceCounter failed: %d", (int) GetLastError());
	return (double) c.QuadPart * perf_to_secs;
#else
#error Meh
#endif
}

static td_sighandler_info * volatile siginfo;

#if defined(TUNDRA_UNIX)
static void* signal_handler_thread_fn(void *arg)
{
	int sig, rc;
	sigset_t sigs;
	sigemptyset(&sigs);
	sigaddset(&sigs, SIGINT);
	sigaddset(&sigs, SIGTERM);
	sigaddset(&sigs, SIGQUIT);
	if (0 == (rc = sigwait(&sigs, &sig)))
	{
		td_sighandler_info* info = siginfo;
		if (info)
		{
			pthread_mutex_lock(info->mutex);
			info->flag = -1;
			switch (sig)
			{
				case SIGINT: info->reason = "SIGINT"; break;
				case SIGTERM: info->reason = "SIGTERM"; break;
				case SIGQUIT: info->reason = "SIGQUIT"; break;
			}
			pthread_mutex_unlock(info->mutex);
			pthread_cond_broadcast(info->cond);
		}
	}
	else
		printf("sigwait failed: %d\n", rc);
	return NULL;
}
#endif

void td_install_sighandler(td_sighandler_info *info)
{
	siginfo = info;

#if defined(TUNDRA_UNIX)
	{
		pthread_t sigthread;
		if (0 != pthread_create(&sigthread, NULL, signal_handler_thread_fn, NULL))
			td_croak("couldn't start signal handler thread");
		pthread_detach(sigthread);
	}
#elif defined(TUNDRA_WIN32)
#else
#error Meh
#endif
}

void td_remove_sighandler(void)
{
	siginfo = NULL;
}

void td_block_signals(int block)
{
#if defined(TUNDRA_UNIX)
	sigset_t sigs;
	sigemptyset(&sigs);
	sigaddset(&sigs, SIGINT);
	sigaddset(&sigs, SIGTERM);
	sigaddset(&sigs, SIGQUIT);
	if  (0 != pthread_sigmask(block ? SIG_BLOCK : SIG_UNBLOCK, &sigs, 0))
		td_croak("pthread_sigmask failed");
#endif
	TD_UNUSED(block);
}

#if defined(TUNDRA_WIN32)
static void push_win32_error(lua_State *L, DWORD err, const char *context)
{
	int chars;
	char buf[1024];
	lua_pushstring(L, context);
	lua_pushstring(L, ": ");
	if (0 != (chars = (int) FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, err, LANG_NEUTRAL, buf, sizeof(buf), NULL)))
		lua_pushlstring(L, buf, chars);
	else
		lua_pushfstring(L, "win32 error %d", (int) err);
	lua_concat(L, 3);
}

int td_win32_register_query(lua_State *L)
{
	HKEY regkey, root_key;
	LONG result = 0;
	const char *key_name, *subkey_name, *value_name = NULL;
	int i;
	static const REGSAM sams[] = { KEY_READ, KEY_READ|KEY_WOW64_32KEY, KEY_READ|KEY_WOW64_64KEY };

	key_name = luaL_checkstring(L, 1);

	if (0 == strcmp(key_name, "HKLM") || 0 == strcmp(key_name, "HKEY_LOCAL_MACHINE"))
		root_key = HKEY_LOCAL_MACHINE;
	else if (0 == strcmp(key_name, "HKCU") || 0 == strcmp(key_name, "HKEY_CURRENT_USER"))
		root_key = HKEY_CURRENT_USER;
	else
		return luaL_error(L, "%s: unsupported root key; use HKLM, HKCU or HKEY_LOCAL_MACHINE, HKEY_CURRENT_USER", key_name);

	subkey_name = luaL_checkstring(L, 2);

	if (lua_gettop(L) >= 3 && lua_isstring(L, 3))
		value_name = lua_tostring(L, 3);

	for (i = 0; i < sizeof(sams)/sizeof(sams[0]); ++i)
	{
		result = RegOpenKeyExA(root_key, subkey_name, 0, sams[i], &regkey);

		if (ERROR_SUCCESS == result)
		{
			DWORD stored_type;
			BYTE data[8192];
			DWORD data_size = sizeof(data);
			result = RegQueryValueExA(regkey, value_name, NULL, &stored_type, &data[0], &data_size);
			RegCloseKey(regkey);

			if (ERROR_FILE_NOT_FOUND != result)
			{
				if (ERROR_SUCCESS != result)
				{
					lua_pushnil(L);
					push_win32_error(L, (DWORD) result, "RegQueryValueExA");
					return 2;
				}

				switch (stored_type)
				{
				case REG_DWORD:
					if (4 != data_size)
						luaL_error(L, "expected 4 bytes for integer key but got %d", data_size);
					lua_pushinteger(L, *(int*)data);
					return 1;

				case REG_SZ:
					/* don't use lstring because that would include potential null terminator */
					lua_pushstring(L, (const char*) data);
					return 1;

				default:
					return luaL_error(L, "unsupported registry value type (%d)", (int) stored_type);
				}
			}
		}
		else if (ERROR_FILE_NOT_FOUND == result)
		{
			continue;
		}
		else
		{
			lua_pushnil(L);
			push_win32_error(L, (DWORD) result, "RegOpenKeyExA");
			return 2;
		}

	}


	lua_pushnil(L);
	push_win32_error(L, ERROR_FILE_NOT_FOUND, "RegOpenKeyExA");
	return 2;
}
#endif

static char homedir[260];

static const char *
set_homedir(const char* dir)
{
	strncpy(homedir, dir, sizeof homedir);
	homedir[sizeof homedir - 1] = '\0';
	return homedir;
}

const char *
td_init_homedir()
{
	char* tmp;
	if (NULL != (tmp = getenv("TUNDRA_HOME")))
		return set_homedir(tmp);

#if defined(TUNDRA_WIN32)
	if (0 == GetModuleFileNameA(NULL, homedir, (DWORD)sizeof(homedir)))
		return NULL;

	if (NULL != (tmp = strrchr(homedir, '\\')))
		*tmp = 0;
	return homedir;

#elif defined(TUNDRA_APPLE)
	char path[1024], resolved_path[1024];
	uint32_t size = sizeof(path);
	if (_NSGetExecutablePath(path, &size) != 0)
		return NULL;
	if ((tmp = dirname(realpath(path, resolved_path))))
		return set_homedir(tmp);
	else
		return NULL;
	

#elif defined(TUNDRA_LINUX)
	if (-1 == readlink("/proc/self/exe", homedir, sizeof(homedir)))
		return NULL;

	if ((tmp = strrchr(homedir, '/')))
		*tmp = 0;
	return homedir;
#elif defined(TUNDRA_FREEBSD)
	/* FreeBSD has a sysctl query function to get at the ELF path. */
	size_t cb = sizeof(homedir);
	int mib[4];
	mib[0] = CTL_KERN;
	mib[1] = KERN_PROC;
	mib[2] = KERN_PROC_PATHNAME;
	mib[3] = -1;
	if (0 != sysctl(mib, 4, homedir, &cb, NULL, 0))
		return NULL;

	if ((tmp = strrchr(homedir, '/')))
		*tmp = 0;
	return homedir;
#elif defined(TUNDRA_OPENBSD)
	/* Not supporting homedir on OpenBSD right now ... */
	return NULL;
#else
#error "unsupported platform"
#endif
}

void
td_get_cwd_c(char buffer[], int bufsize)
{
#if defined(TUNDRA_WIN32)
	DWORD res = GetCurrentDirectoryA((DWORD)bufsize, buffer);
	if (0 == res || ((DWORD)bufsize) <= res)
		td_croak("couldn't get working directory");
#else
	if (NULL == getcwd(buffer, bufsize))
		td_croak("couldn't get working directory");
#endif
}

int
td_get_cwd(struct lua_State *L)
{
	char buffer[512];
	td_get_cwd_c(buffer, sizeof buffer);
	lua_pushstring(L, buffer);
	return 1;
}

int
td_set_cwd(struct lua_State *L)
{
	const char *dir = luaL_checkstring(L, 1);
#if defined(TUNDRA_WIN32)
	if (!SetCurrentDirectoryA(dir))
		return luaL_error(L, "couldn't change into %s: win32 error=%d", dir, (int) GetLastError());
#else
	if (0 != chdir(dir))
		return luaL_error(L, "couldn't change into %s: %s", dir, strerror(errno));
#endif
	return 0;
}

int
td_get_processor_count(void)
{
#if defined(TUNDRA_WIN32)
	SYSTEM_INFO si;
	GetSystemInfo(&si);
	return (int) si.dwNumberOfProcessors;
#else
	long nprocs_max = sysconf(_SC_NPROCESSORS_CONF);
	if (nprocs_max < 0)
		td_croak("couldn't get CPU count: %s", strerror(errno));
	return (int) nprocs_max;
#endif
}

void
td_mutex_lock_or_die(pthread_mutex_t *lock)
{
	if (0 != pthread_mutex_lock(lock))
		td_croak("couldn't lock mutex");
}

void
td_mutex_unlock_or_die(pthread_mutex_t *lock)
{
	if (0 != pthread_mutex_unlock(lock))
		td_croak("couldn't unlock mutex");
}

void
td_mutex_init_or_die(pthread_mutex_t *mutex, void *args)
{
#if defined(TUNDRA_UNIX)
	pthread_mutexattr_t attr;
	pthread_mutexattr_init(&attr);
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);

	if (0 != pthread_mutex_init(mutex, &attr))
		td_croak("couldn't init mutex");

	pthread_mutexattr_destroy(&attr);
#elif defined(TUNDRA_WIN32)
	if (0 != pthread_mutex_init(mutex, NULL))
		td_croak("couldn't init mutex");
#else
#error implement me
#endif
}

void
td_mutex_destroy_or_die(pthread_mutex_t *mutex)
{
	if (0 != pthread_mutex_destroy(mutex))
		td_croak("couldn't destroy mutex");
}
