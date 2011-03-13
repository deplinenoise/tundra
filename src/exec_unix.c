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

/* exec_unix.c -- subprocess spawning and output handling for unix-like systems */

#include "portable.h"
#include "config.h"
#include "util.h"
#include "tty.h"

#if defined(TUNDRA_UNIX)

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

#include <unistd.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <libgen.h>
#include <errno.h>

static void set_fd_nonblocking(int fd)
{
	int flags;
	
	flags = fcntl(fd, F_GETFL);
	flags |= O_NONBLOCK;
	if (-1 == fcntl(fd, F_SETFL, flags))
		td_croak("couldn't unblock fd %d", fd);
}

int
td_init_exec(void)
{
	return tty_init();
}

static int
emit_data(int job_id, int is_stderr, int sort_key, int fd)
{
	char text[8192];
	ssize_t count;

	count = read(fd, text, sizeof(text)-1);

	if (count <= 0)
	{
		if (EAGAIN == errno)
			return 0;
		else
			return -1;
	}

	text[count] = '\0';

	tty_emit(job_id, is_stderr, sort_key, text, count);

	return 0;
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
	pid_t child;
	const int pipe_read = 0;
	const int pipe_write = 1;

	/* Create a pair of pipes to read back stdout, stderr */
	int stdout_pipe[2], stderr_pipe[2];

	if (-1 == pipe(stdout_pipe))
	{
		perror("pipe failed");
		return 1;
	}

	if (-1 == pipe(stderr_pipe))
	{
		perror("pipe failed");
		close(stdout_pipe[0]);
		close(stdout_pipe[1]);
		return 1;
	}

	if (0 == (child = fork()))
	{
		int i;
		const char *args[] = { "/bin/sh", "-c", cmd_line, NULL };
		char name_block[1024];

		close(stdout_pipe[pipe_read]);
		close(stderr_pipe[pipe_read]);

		if (-1 == dup2(stdout_pipe[pipe_write], STDOUT_FILENO))
			perror("dup2 failed");
		if (-1 == dup2(stderr_pipe[pipe_write], STDERR_FILENO))
			perror("dup2 failed");

		close(stdout_pipe[pipe_write]);
		close(stderr_pipe[pipe_write]);

		sigset_t sigs;
		sigfillset(&sigs);
		if (0 != sigprocmask(SIG_UNBLOCK, &sigs, 0))
			perror("sigprocmask failed");

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

		fflush(stdout);

		if (-1 == execv("/bin/sh", (char **) args))
			exit(1);
		/* we never get here */
		abort();
	}
	else if (-1 == child)
	{
		perror("fork failed");
		close(stdout_pipe[pipe_read]);
		close(stderr_pipe[pipe_read]);
		close(stdout_pipe[pipe_write]);
		close(stderr_pipe[pipe_write]);
		return 1;
	}
	else
	{
		pid_t p;
		int return_code = 0;
		int sort_key = 0;
		int rfd_count = 2;
		int rfds[2];
		fd_set read_fds;

		rfds[0] = stdout_pipe[pipe_read];
		rfds[1] = stderr_pipe[pipe_read];

		set_fd_nonblocking(rfds[0]);
		set_fd_nonblocking(rfds[1]);

		/* Close write end of the pipe, we're just going to be reading */
		close(stdout_pipe[pipe_write]);
		close(stderr_pipe[pipe_write]);

		if (annotation)
			tty_printf(job_id, -200, "%s\n", annotation);

		if (echo_cmdline)
			tty_printf(job_id, -199, "%s\n", cmd_line);

		/* Sit in a select loop over the two fds */
		while (rfd_count > 0)
		{
			int fd;
			int count;
			int max_fd = 0;
			struct timeval timeout;

			FD_ZERO(&read_fds);

			for (fd = 0; fd < 2; ++fd)
			{
				if (rfds[fd])
				{
					if (rfds[fd] > max_fd)
						max_fd = rfds[fd];
					FD_SET(rfds[fd], &read_fds);
				}
			}

			++max_fd;
			
			timeout.tv_sec = 0;
			timeout.tv_usec = 500000;

			count = select(max_fd, &read_fds, NULL, NULL, &timeout);

			if (-1 == count) // happens in gdb due to syscall interruption
				continue;

			for (fd = 0; fd < 2; ++fd)
			{
				if (0 != rfds[fd] && FD_ISSET(rfds[fd], &read_fds))
				{
					if (0 != emit_data(job_id, /*is_stderr:*/ 1 == fd, sort_key++, rfds[fd]))
					{
						/* Done with this FD. */
						rfds[fd] = 0;
						--rfd_count;
					}
				}
			}

			p = waitpid(child, &return_code, WNOHANG);

			if (0 == p)
			{
				/* child still running */
				continue;
			}
			else if (p != child)
			{
				return_code = 1;
				perror("waitpid failed");
				break;
			}
			else
				break;
		}

		close(stdout_pipe[pipe_read]);
		close(stderr_pipe[pipe_read]);

		tty_job_exit(job_id);
	
		*was_signalled_out = WIFSIGNALED(return_code);
		return return_code;
	}
}

#endif /* TUNDRA_UNIX */

