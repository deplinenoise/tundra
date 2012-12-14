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

/* exec-win32.c -- Windows subprocess handling
 *
 * This module is complicated due to how Win32 handles child I/O and because
 * its command processor cannot handle long command lines, requiring tools to
 * support so-called "response files" which are basically just the contents of
 * the command line, but written to a file. Tundra implements this via the
 * @RESPONSE handling.
 *
 * Also, rather than using the tty merging module that unix uses, this module
 * handles output merging via temporary files.  This removes the pipe
 * read/write deadlocks I've seen from time to time when using the TTY merging
 * code. Rather than losing my last sanity points on debugging win32 internals
 * I've opted for this much simpler approach.
 *
 * Instead of buffering stuff in memory via pipe babysitting, this new code
 * first passes the stdout handle straight down to the first process it spawns.
 * Subsequent child processes that are spawned when the TTY is busy instead get
 * to inherit a temporary file handle. Once the process completes, it will wait
 * to get the TTY and flush its temporary output file to the console. The
 * temporary is then deleted.
 */

#include "portable.h"
#include "config.h"
#include "build.h"

#if defined(TUNDRA_WIN32)

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

static char temp_path[MAX_PATH];
static DWORD tundra_pid;
static pthread_mutex_t fd_mutex;
static pthread_cond_t tty_free;
static int tty_owner = -1;

static HANDLE temp_files[TD_MAX_THREADS];

static void get_fn(int job_id, char buf[MAX_PATH])
{
	_snprintf(buf, MAX_PATH, "%stundra.%u.%d", temp_path, tundra_pid, job_id);
}

static HANDLE alloc_fd(int job_id)
{
	HANDLE result = temp_files[job_id];

	if (!result)
	{
		char temp_path[MAX_PATH];
		DWORD access, sharemode, disp, flags;

		get_fn(job_id, temp_path);

		access		= GENERIC_WRITE | GENERIC_READ;
		sharemode = FILE_SHARE_WRITE;
		disp			= CREATE_ALWAYS;
		flags			= FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE;

		result = CreateFileA(temp_path, access, sharemode, NULL, disp, flags, NULL);

		if (INVALID_HANDLE_VALUE == result)
		{
			fprintf(stderr, "failed to create temporary file %s\n", temp_path);
			return INVALID_HANDLE_VALUE;
		}

		SetHandleInformation(result, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);

		temp_files[job_id] = result;
	}

	return result;
}

static void free_fd(int job_id, HANDLE h)
{
	char buf[1024];
	HANDLE target = GetStdHandle(STD_OUTPUT_HANDLE);
	SetFilePointer(h, 0, NULL, FILE_BEGIN);

	// Wait until we can take the TTY.
	td_mutex_lock_or_die(&fd_mutex);

	while (tty_owner != -1)
		pthread_cond_wait(&tty_free, &fd_mutex);

	tty_owner = job_id;
	td_mutex_unlock_or_die(&fd_mutex);

	// Dump the contents of the temporary file to the TTY.
	for (;;)
	{
		DWORD rb, wb;
		if (!ReadFile(h, buf, sizeof(buf), &rb, NULL) || rb == 0)
			break;
		if (!WriteFile(target, buf, rb, &wb, NULL))
			break;
	}

	// Mark the TTY as free again and wake waiters.
	td_mutex_lock_or_die(&fd_mutex);
	tty_owner = -1;
	pthread_cond_signal(&tty_free);
	td_mutex_unlock_or_die(&fd_mutex);

	// Truncate the temporary file for reuse
	SetFilePointer(h, 0, NULL, FILE_BEGIN);
	SetEndOfFile(h);
}

int
td_init_exec(void)
{
	int i;
	tundra_pid = GetCurrentProcessId();

	if (0 == GetTempPathA(sizeof(temp_path), temp_path)) {
		fprintf(stderr, "error: couldn't get temporary directory path\n");
		return 1;
	}

	if (0 != pthread_cond_init(&tty_free, NULL))
		return 1;

	td_mutex_init_or_die(&fd_mutex, NULL);

	return 0;
}

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

extern char* win32_env_block;

static int
make_env_block(char* env_block, size_t block_size, const char **env, int env_count)
{
	size_t cursor = 0;
	char *p = win32_env_block;
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

static void
println_to_handle(HANDLE h, const char *str)
{
	DWORD bw;
	WriteFile(h, str, (DWORD) strlen(str), &bw, NULL);
	WriteFile(h, "\r\n", 2, &bw, NULL);
}

/*
	 win32_spawn -- spawn and wait for a a sub-process on Windows.

	 We would like to say:

		return (int) _spawnlpe(_P_WAIT, "cmd", "/c", cmd_line, NULL, env);

	 but it turns out spawnlpe() isn't thread-safe (MSVC2008) when setting
	 environment data!	So we're doing it the hard way. It also would set the
	 environment in the right way as we want to define our environment strings in
	 addition to whatever is already set, not replace everything.
 */

int win32_spawn(int job_id, const char *cmd_line, const char **env, int env_count, const char *annotation, const char* echo_cmdline)
{
	int result_code = 1;
	char buffer[8192];
	char env_block[128*1024];
	HANDLE output_handle;
	STARTUPINFO sinfo;
	PROCESS_INFORMATION pinfo;

	if (0 != make_env_block(env_block, sizeof(env_block) - 2, env, env_count))
	{
		fprintf(stderr, "%d: env block error; too big?\n", job_id);
		return 1;
	}

	_snprintf(buffer, sizeof(buffer), "cmd.exe /c \"%s\"", cmd_line);

	output_handle = alloc_fd(job_id);

	memset(&pinfo, 0, sizeof(pinfo));
	memset(&sinfo, 0, sizeof(sinfo));
	sinfo.cb = sizeof(sinfo);
	sinfo.hStdInput = NULL;
	sinfo.hStdOutput = sinfo.hStdError = output_handle;
	sinfo.dwFlags = STARTF_USESTDHANDLES;

	if (annotation)
		println_to_handle(output_handle, annotation);

	if (echo_cmdline)
		println_to_handle(output_handle, echo_cmdline);

	if (CreateProcess(NULL, buffer, NULL, NULL, TRUE, 0, env_block, NULL, &sinfo, &pinfo))
	{
		DWORD result;
		CloseHandle(pinfo.hThread);

		while (WAIT_OBJECT_0 != WaitForSingleObject(pinfo.hProcess, INFINITE))
			/* nop */;

		GetExitCodeProcess(pinfo.hProcess, &result);
		CloseHandle(pinfo.hProcess);
		result_code = (int) result;
	}
	else
	{
		fprintf(stderr, "%d: Couldn't launch process; Win32 error = %d\n", job_id, (int) GetLastError());
	}

	free_fd(job_id, output_handle);

	return result_code;
}

int td_exec(
		const char* cmd_line,
		int env_count,
		const char **env,
		int *was_signalled_out,
		int job_id,
		int echo_cmdline,
		const char *annotation)
{
	static const char response_prefix[] = "@RESPONSE|";
	static const size_t response_prefix_len = sizeof(response_prefix) - 1;
	char command_buf[512];
	char option_buf[32];
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

			{
				int copy_len = min((int) (response - cmd_line), (int) (sizeof(command_buf) - 1));
				strncpy(command_buf, cmd_line, copy_len);
				command_buf[copy_len] = '\0';
				copy_len = min((int) (option_end - option), (int) (sizeof(option_buf) - 1));
				strncpy(option_buf, option, copy_len);
				option_buf[copy_len] = '\0';
			}

			_snprintf(new_cmd, sizeof(new_cmd), "%s %s%s", command_buf, option_buf, response_file);
			new_cmd[sizeof(new_cmd)-1] = '\0';

			cmd_result = win32_spawn(job_id, new_cmd, env, env_count, annotation, echo_cmdline ? cmd_line : NULL);

			remove(response_file);
			return cmd_result;
		}
		else
		{
			size_t i, len;
			int copy_len = min((int) (response - cmd_line), (int) (sizeof(command_buf) - 1));
			strncpy(command_buf, cmd_line, copy_len);
			command_buf[copy_len] = '\0';
			_snprintf(new_cmd, sizeof(new_cmd), "%s%s", command_buf, option_end + 1);
			new_cmd[sizeof(new_cmd)-1] = '\0';

			/* Drop any newlines in the command line. They are needed for response
			 * files only to make sure the max length doesn't exceed 128 kb */
			for (i = 0, len = strlen(new_cmd); i < len; ++i)
			{
				if (new_cmd[i] == '\n')
				{
					new_cmd[i] = ' ';
				}
			}

			return win32_spawn(job_id, new_cmd, env, env_count, annotation, echo_cmdline ? cmd_line : NULL);
		}
	}
	else
	{
		size_t i, len;

		/* Drop any newlines in the command line. They are needed for response
		 * files only to make sure the max length doesn't exceed 128 kb */
		strncpy(new_cmd, cmd_line, sizeof(new_cmd));
		new_cmd[sizeof(new_cmd)-1] = '\0';

		for (i = 0, len = strlen(new_cmd); i < len; ++i)
		{
			if (new_cmd[i] == '\n')
			{
				new_cmd[i] = ' ';
			}
		}

		return win32_spawn(job_id, new_cmd, env, env_count, annotation, echo_cmdline ? cmd_line : NULL);
	}
}

#endif /* TUNDRA_WIN32 */
