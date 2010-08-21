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

#include <string.h>

#if defined(__APPLE__) || defined(linux)
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

#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

#ifdef _WIN32
#include <windows.h>
#include <stdlib.h>
#include <errno.h>
#include <process.h>
#include <stdio.h>
#include <assert.h>
#define snprintf _snprintf
#endif

const char * const td_platform_string =
#if defined(__APPLE__)
	"macosx";
#elif defined(linux)
	"linux";
#elif defined(_WIN32)
	"windows";
#else
#error implement me
#endif

#if defined(_WIN32)
int pthread_mutex_init(pthread_mutex_t *mutex, void *args)
{
	TD_UNUSED(args);

	mutex->handle = (PCRITICAL_SECTION)malloc(sizeof(CRITICAL_SECTION));
	if (!mutex->handle)
		return ENOMEM;
	InitializeCriticalSection((PCRITICAL_SECTION)mutex->handle);
	return 0;
}

int pthread_mutex_destroy(pthread_mutex_t *mutex)
{
	DeleteCriticalSection((PCRITICAL_SECTION)mutex->handle);
	free(mutex->handle);
	mutex->handle = 0;
	return 0;
}

int pthread_mutex_lock(pthread_mutex_t *lock)
{
	EnterCriticalSection((PCRITICAL_SECTION)lock->handle);
	return 0;
}

int pthread_mutex_unlock(pthread_mutex_t *lock)
{
	LeaveCriticalSection((PCRITICAL_SECTION)lock->handle);
	return 0;
}

int pthread_cond_init(pthread_cond_t *cond, void *args)
{
	TD_UNUSED(args);
	assert(args == NULL);

	if (NULL == (cond->handle = malloc(sizeof(CONDITION_VARIABLE))))
		return ENOMEM;

	InitializeConditionVariable((PCONDITION_VARIABLE) cond->handle);
	return 0;
}

int pthread_cond_destroy(pthread_cond_t *cond)
{
	free (cond->handle);
	return 0;
}

int pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t* lock)
{
	BOOL result = SleepConditionVariableCS((PCONDITION_VARIABLE) cond->handle, (PCRITICAL_SECTION) lock->handle, INFINITE);
	return !result;
}

int pthread_cond_broadcast(pthread_cond_t *cond)
{
	WakeAllConditionVariable((PCONDITION_VARIABLE) cond->handle);
	return 0;
}

int pthread_cond_signal(pthread_cond_t *cond)
{
	WakeConditionVariable((PCONDITION_VARIABLE) cond->handle);
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
#if defined(__APPLE__) || defined(linux)
	return mkdir(path, 0777);
#elif defined(_WIN32)
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
#else
#error meh
#endif
}

int
td_rmdir(const char *path)
{
#if defined(__APPLE__) || defined(linux)
	return rmdir(path);
#elif defined(_WIN32)
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
#if defined(__APPLE__) || defined(linux)
	int err;
	struct stat s;
	if (0 != (err = stat(path, &s)))
		return err;

	out->flags = TD_STAT_EXISTS | ((s.st_mode & S_IFDIR) ? TD_STAT_DIR : 0);
	out->size = s.st_size;
	out->timestamp = s.st_mtime;
	return 0;
#elif defined(_WIN32)
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
#if defined(__APPLE__) || defined(linux)
	return rename(source, dest);
#elif defined(_WIN32)
	if (MoveFileExA(source, dest, MOVEFILE_REPLACE_EXISTING))
		return 0;
	else
		return GetLastError();
#else
#error meh
#endif

}

#if defined(_WIN32)
static double perf_to_secs;
static LARGE_INTEGER initial_time;
#elif defined(__APPLE__) || defined(linux)
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
#if defined(__APPLE__) || defined(linux)
	start_time = dtime_now();
#elif defined(_WIN32)
	LARGE_INTEGER perf_freq;
	if (!QueryPerformanceFrequency(&perf_freq))
		td_croak("QueryPerformanceFrequency failed: %d", (int) GetLastError());
	if (!QueryPerformanceCounter(&initial_time))
		td_croak("QueryPerformanceCounter failed: %d", (int) GetLastError());
	perf_to_secs = 1.0 / (double) perf_freq.QuadPart;
#endif
}

#if defined(_WIN32)
static char *s_env_block;
#endif

void td_init_portable(void)
{
	init_timer();

#if defined(_WIN32)
	/* Grab the environment block once and just let it leak. */
	s_env_block = GetEnvironmentStringsA();
#endif
}

double td_timestamp(void)
{
#if defined(__APPLE__) || defined(linux)
	return dtime_now() - start_time;
#elif defined(_WIN32)
	LARGE_INTEGER c;
	if (!QueryPerformanceCounter(&c))
		td_croak("QueryPerformanceCounter failed: %d", (int) GetLastError());
	return (double) c.QuadPart * perf_to_secs;
#else
#error Meh
#endif
}

static td_sighandler_info * volatile siginfo;

#if defined(__APPLE__) || defined(linux)
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
		td_croak("sigwait failed: %d", rc);
	return NULL;
}
#endif

void td_install_sighandler(td_sighandler_info *info)
{
	siginfo = info;

#if defined(__APPLE__) || defined(linux)
	{
		pthread_t sigthread;
		if (0 != pthread_create(&sigthread, NULL, signal_handler_thread_fn, NULL))
			td_croak("couldn't start signal handler thread");
		pthread_detach(sigthread);
	}
#elif defined(_WIN32)
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
#if defined(__APPLE__) || defined(linux)
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

#if defined(_WIN32)
static int
append_string(char* block, size_t block_size, size_t *cursor, const char *src)
{
	size_t len = strlen(src);
	if (*cursor + len + 1 > block_size)
		return 1;

	memcpy(block + *cursor, src, len + 1);
	(*cursor) += len + 1;
	return 0;
}

static int
make_env_block(char* env_block, size_t block_size, const char **env, int env_count)
{
	size_t cursor = 0;
	char *p = s_env_block;
	unsigned char used_env[1024];

	if (env_count > sizeof(used_env))
		return 1;

	memset(used_env, 0, sizeof(used_env));

	while (*p)
	{
		int i;
		int replaced = 0;
		size_t len;

		for (i = 0; i < env_count; ++i)
		{
			const char *equals;
			
			if (used_env[i])
				continue;

			equals = strchr(env[i], '=');

			if (!equals)
				continue;

			if (0 == _strnicmp(p, env[i], equals - env[i] + 1))
			{
				if (0 != append_string(env_block, block_size, &cursor, env[i]))
					return 1;
				used_env[i] = 1;
				replaced = 1;
				break;
			}
		}

		len = strlen(p);

		/* skip items without name that win32 seems to include for itself */
		if (!replaced && p[0] != '=')
		{
			if (0 != append_string(env_block, block_size, &cursor, p))
				return 1;
		}

		p = p + len + 1;
	}

	{
		int i;
		for (i = 0; i < env_count; ++i)
		{
			if (used_env[i])
				continue;
			if (0 != append_string(env_block, block_size, &cursor, env[i]))
				return 1;
		}
	}

	env_block[cursor] = '\0';
	env_block[cursor+1] = '\0';
	return 0;
}

/*
   win32_spawn -- spawn and wait for a a sub-process on Windows. 
 
   We would like to say:

	  return (int) _spawnlpe(_P_WAIT, "cmd", "/c", cmd_line, NULL, env);

   but it turns out spawnlpe() isn't thread-safe (MSVC2008) when setting environment data!
   So we're doing it the hard way.
 */

static char *
emit_lines(const char *prefix, char* buffer, size_t size)
{
	size_t i, start = 0;
	for (i = 0; i < size; ++i)
	{
		if ('\n' == buffer[i])
		{
			buffer[i] = '\0';
			printf("%s%s\n", prefix, &buffer[start]);
			start = i+1;
		}
	}

	if (start > 0)
		memmove(buffer, buffer + start, size - start);
	return buffer + size - start;
}

static void
pump_stdio(const char *prefix, HANDLE input, HANDLE proc)
{
	char buffer[1024];
	char *bufp = buffer;
	int remain = sizeof(buffer);
	DWORD bytes_read;
	for (;;)
	{
		if (!ReadFile(input, bufp, remain, &bytes_read, NULL) || !bytes_read)
			break;
	
		bufp = emit_lines(prefix, buffer, (size_t) bytes_read + (bufp - buffer));
		remain = (int) (buffer + sizeof(buffer) - bufp);
	}
}

int win32_spawn(const char *prefix, const char *cmd_line, const char **env, int env_count)
{
	int result_code = 1;
	char buffer[8192];
	char env_block[128*1024];
	HANDLE std_in_rd = NULL, std_in_wr = NULL, std_out_rd = NULL, std_out_wr = NULL;
	STARTUPINFO sinfo;
	PROCESS_INFORMATION pinfo;
	
	if (0 != make_env_block(env_block, sizeof(env_block) - 2, env, env_count))
	{
		fprintf(stderr, "%senv block error; too big?\n", prefix);
		return 1;
	}

	snprintf(buffer, sizeof(buffer), "cmd.exe /c %s", cmd_line);

	{
		/* Set the bInheritHandle flag so pipe handles are inherited. */
		SECURITY_ATTRIBUTES pipe_attr; 
		pipe_attr.nLength = sizeof(SECURITY_ATTRIBUTES); 
		pipe_attr.bInheritHandle = TRUE; 
		pipe_attr.lpSecurityDescriptor = NULL; 
		if (!CreatePipe(&std_in_rd, &std_in_wr, &pipe_attr, 0) ||
			!CreatePipe(&std_out_rd, &std_out_wr, &pipe_attr, 0))
		{
			fprintf(stderr, "%scouldn't create pipe; win32 error = %d\n", prefix, (int) GetLastError());
			result_code = 1;
			goto leave;
		}
	}

	if (!SetHandleInformation(std_out_rd, HANDLE_FLAG_INHERIT, 0))
		goto leave;
	if (!SetHandleInformation(std_in_wr, HANDLE_FLAG_INHERIT, 0))
		goto leave;

	memset(&pinfo, 0, sizeof(pinfo));

	memset(&sinfo, 0, sizeof(sinfo));
	sinfo.cb = sizeof(sinfo);
	sinfo.hStdInput = std_in_rd;
	sinfo.hStdOutput = std_out_wr;
	sinfo.hStdError = std_out_wr;
	sinfo.dwFlags = STARTF_USESTDHANDLES;

	if (CreateProcess(NULL, buffer, NULL, NULL, TRUE, 0, env_block, NULL, &sinfo, &pinfo))
	{
		DWORD result;
		CloseHandle(pinfo.hThread);

		CloseHandle(std_in_wr);
		std_in_wr = NULL;
		CloseHandle(std_out_wr);
		std_out_wr = NULL;

		pump_stdio(prefix, std_out_rd, pinfo.hProcess);
		
		while (WAIT_OBJECT_0 != WaitForSingleObject(pinfo.hProcess, INFINITE))
			/* nop */;

		GetExitCodeProcess(pinfo.hProcess, &result);
		CloseHandle(pinfo.hProcess);
		result_code = (int) result;
	}
	else
	{
		fprintf(stderr, "%sCouldn't launch process; Win32 error = %d\n", prefix, (int) GetLastError());
	}

leave:
	if (std_in_rd) CloseHandle(std_in_rd);
	if (std_in_wr) CloseHandle(std_in_wr);
	if (std_out_rd) CloseHandle(std_out_rd);
	if (std_out_wr) CloseHandle(std_out_wr);

	return result_code;
}
#endif

int td_exec(const char* cmd_line, int env_count, const char **env, int *was_signalled_out, const char *prefix)
{
#if defined(__APPLE__) || defined(linux)
	pid_t child;
	if (0 == (child = fork()))
	{
		sigset_t sigs;
		sigfillset(&sigs);
		if (0 != sigprocmask(SIG_UNBLOCK, &sigs, 0))
			perror("sigprocmask failed");

		const char *args[] = { "/bin/sh", "-c", cmd_line, NULL };

		int i;
		char name_block[1024];
		for (i = 0; i < env_count; ++i)
		{
			const char *var = env[i];
			const char *equals = strchr(var, '=');

			if (!equals)
				continue;

			if (equals - var >= sizeof(name_block))
			{
				fprintf(stderr, "Name for environment setting '%s' too long\n", var);
				continue;
			}

			memcpy(name_block, var, equals - var);
			name_block[equals - var] = '\0';

			setenv(name_block, equals + 1, 1);
		}


		if (-1 == execv("/bin/sh", (char **) args))
			exit(1);
		/* we never get here */
		abort();
	}
	else if (-1 == child)
	{
		perror("fork failed");
		return 1;
	}
	else
	{
		pid_t p;
		int return_code;
		p = waitpid(child, &return_code, 0);
		if (p != child)
		{
			perror("waitpid failed");
			return 1;
		}
	
		*was_signalled_out = WIFSIGNALED(return_code);
		return return_code;
	}

#elif defined(_WIN32)
	static const char response_prefix[] = "@RESPONSE|";
	static const size_t response_prefix_len = sizeof(response_prefix) - 1;
	char new_cmd[8192];
	const char* response;
	*was_signalled_out = 0;

	/* scan for a @RESPONSE|<option>|.... section at the end of the command line */
	if (NULL != (response = strstr(cmd_line, response_prefix)))
	{
		const size_t cmd_len = strlen(cmd_line);
		const char *option, *option_end;

		option = response + response_prefix_len;

		if (NULL == (option_end = strchr(option, '|')))
		{
			fprintf(stderr, "badly formatted @RESPONSE section; no comma after option: %s\n", cmd_line);
			return 1;
		}

		/* Limit on XP and later is 8191 chars; but play it safe */
		if (cmd_len > 8000)
		{
			char tmp_dir[MAX_PATH];
			char response_file[MAX_PATH];
			int cmd_result;
			FILE* tmp;
			DWORD rc;

			rc = GetTempPath(sizeof(tmp_dir), tmp_dir);
			if (rc >= sizeof(tmp_dir) || 0 == rc)
			{
				fprintf(stderr, "couldn't get temporary directory for response file; win32 error=%d", (int) GetLastError());
				return 1;
			}

			rc = GetTempFileName(tmp_dir, "tundra_resp", 0, response_file);
			if (0 == rc)
			{
				fprintf(stderr, "couldn't create temporary file for response file in dir %s; win32 error=%d", tmp_dir, (int) GetLastError());
				return 1;
			}

			if (NULL == (tmp = fopen(response_file, "w")))
			{
				fprintf(stderr, "couldn't create response file %s", response_file);
				return 1;
			}

			fputs(option_end + 1, tmp);
			fclose(tmp);

			strncpy_s(new_cmd, sizeof(new_cmd), cmd_line, response - cmd_line);
			strcat_s(new_cmd, sizeof(new_cmd), " ");
			strncat_s(new_cmd, sizeof(new_cmd), option, option_end - option);
			strcat_s(new_cmd, sizeof(new_cmd), response_file);

			cmd_result = win32_spawn(prefix, new_cmd, env, env_count);

			remove(response_file);
			return cmd_result;
		}
		else
		{
			strncpy_s(new_cmd, sizeof(new_cmd), cmd_line, response - cmd_line);
			strcat_s(new_cmd, sizeof(new_cmd), option_end + 1);
			return win32_spawn(prefix, new_cmd, env, env_count);
		}
	}
	else
	{
		/* no section in command line at all, just run it */
		return win32_spawn(prefix, cmd_line, env, env_count);
	}
#else
#error meh
#endif
}

#if defined(_WIN32)
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

#if defined(_WIN32)
	if (0 == GetModuleFileNameA(NULL, homedir, (DWORD)sizeof(homedir)))
		return NULL;

	if (NULL != (tmp = strrchr(homedir, '\\')))
		*tmp = 0;
	return homedir;

#elif defined(__APPLE__)
	char path[1024], resolved_path[1024];
	uint32_t size = sizeof(path);
	if (_NSGetExecutablePath(path, &size) != 0)
		return NULL;
	if ((tmp = dirname(realpath(path, resolved_path))))
		return set_homedir(tmp);
	else
		return NULL;
	

#elif defined(linux)
	if (-1 == readlink("/proc/self/exe", homedir, sizeof(homedir)))
		return NULL;

	if ((tmp = strrchr(homedir, '/')))
		*tmp = 0;
	return homedir;
#else
#error "unsupported platform"
#endif
}

int
td_get_cwd(struct lua_State *L)
{
	char buffer[512];
#if defined(_WIN32)
	DWORD res = GetCurrentDirectoryA(sizeof(buffer), buffer);
	if (0 == res || sizeof(buffer) <= res)
		return luaL_error(L, "couldn't get working dir: win32 error=%d", (int) GetLastError());
#else
	if (NULL == getcwd(buffer, sizeof(buffer)))
		return luaL_error(L, "couldn't get working dir: %s", strerror(errno));
#endif
	lua_pushstring(L, buffer);
	return 1;
}

int
td_set_cwd(struct lua_State *L)
{
	const char *dir = luaL_checkstring(L, 1);
#if defined(_WIN32)
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
#if defined(_WIN32)
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
