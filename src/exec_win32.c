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
 */

#include "portable.h"
#include "config.h"

#if defined(TUNDRA_WIN32)

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

int
td_init_exec(void) {
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

/*
   win32_spawn -- spawn and wait for a a sub-process on Windows.

   We would like to say:

	  return (int) _spawnlpe(_P_WAIT, "cmd", "/c", cmd_line, NULL, env);

   but it turns out spawnlpe() isn't thread-safe (MSVC2008) when setting
   environment data!  So we're doing it the hard way. It also would set the
   environment in the right way as we want to define our environment strings in
   addition to whatever is already set, not replace everything.
 */

static char *
emit_lines(int job_id, char* buffer, size_t size)
{
	size_t i, start = 0;
	for (i = 0; i < size; ++i)
	{
		if ('\n' == buffer[i])
		{
			buffer[i] = '\0';
			printf("%d> %s\n", job_id, &buffer[start]);
			start = i+1;
		}
	}

	if (start > 0)
		memmove(buffer, buffer + start, size - start);
	return buffer + size - start;
}

static void
pump_stdio(int job_id,  HANDLE input, HANDLE proc)
{
	char buffer[1024];
	char *bufp = buffer;
	int remain = sizeof(buffer);
	DWORD bytes_read;
	for (;;)
	{
		if (!ReadFile(input, bufp, remain, &bytes_read, NULL) || !bytes_read)
			break;
	
		bufp = emit_lines(job_id, buffer, (size_t) bytes_read + (bufp - buffer));
		remain = (int) (buffer + sizeof(buffer) - bufp);
	}
}

int win32_spawn(int job_id, const char *cmd_line, const char **env, int env_count)
{
	int result_code = 1;
	char buffer[8192];
	char env_block[128*1024];
	HANDLE std_in_rd = NULL, std_in_wr = NULL, std_out_rd = NULL, std_out_wr = NULL;
	STARTUPINFO sinfo;
	PROCESS_INFORMATION pinfo;
	
	if (0 != make_env_block(env_block, sizeof(env_block) - 2, env, env_count))
	{
		fprintf(stderr, "%d: env block error; too big?\n", job_id);
		return 1;
	}

	_snprintf(buffer, sizeof(buffer), "cmd.exe /c %s", cmd_line);

	{
		/* Set the bInheritHandle flag so pipe handles are inherited. */
		SECURITY_ATTRIBUTES pipe_attr; 
		pipe_attr.nLength = sizeof(SECURITY_ATTRIBUTES); 
		pipe_attr.bInheritHandle = TRUE; 
		pipe_attr.lpSecurityDescriptor = NULL; 
		if (!CreatePipe(&std_in_rd, &std_in_wr, &pipe_attr, 0) ||
			!CreatePipe(&std_out_rd, &std_out_wr, &pipe_attr, 0))
		{
			fprintf(stderr, "%d: couldn't create pipe; win32 error = %d\n", job_id, (int) GetLastError());
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

		pump_stdio(job_id, std_out_rd, pinfo.hProcess);
		
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

leave:
	if (std_in_rd) CloseHandle(std_in_rd);
	if (std_in_wr) CloseHandle(std_in_wr);
	if (std_out_rd) CloseHandle(std_out_rd);
	if (std_out_wr) CloseHandle(std_out_wr);

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

			cmd_result = win32_spawn(job_id, new_cmd, env, env_count);

			remove(response_file);
			return cmd_result;
		}
		else
		{
			int copy_len = min((int) (response - cmd_line), (int) (sizeof(command_buf) - 1));
			strncpy(command_buf, cmd_line, copy_len);
			command_buf[copy_len] = '\0';
			_snprintf(new_cmd, sizeof(new_cmd), "%s%s", command_buf, option_end + 1);
			new_cmd[sizeof(new_cmd)-1] = '\0';
			return win32_spawn(job_id, new_cmd, env, env_count);
		}
	}
	else
	{
		/* no section in command line at all, just run it */
		return win32_spawn(job_id, cmd_line, env, env_count);
	}
}

#endif /* TUNDRA_WIN32 */
