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

#if defined(TUNDRA_UNIX)

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

/* Line buffering:
 *
 * This solves the issue of having multiple programs (compilers and such)
 * writing output to both stdout and stderr at the same time. Normally on UNIX
 * the terminal will arbitrate amongst them in a decent fashion, but it really
 * screws up long error messages that span several lines.
 *
 * The solution implemented here works as follows.
 *
 * - We assign the TTY to one job thread which is allowed to echo its child
 *   process text directly. The job id of the current TTY owner is stored in
 *   `printing_job'.
 *
 * - We create pipes for stdout and stderr for all spawned child processes. We
 *   loop around these fds using select() until they both return EOF.
 *
 * - Whenever select() returns we try to read up to LINEBUF_SIZE bytes. If
 *   there isn't anything to read, the fd is closed. The select loop will exit
 *   only when both stdout and stderr have been closed.
 *
 * - The data read from child FDs is either
 *   a) Directly emitted to the build tool's stdout/stderr if the job owns the
 *      TTY (or can become the owner).
 *   b) Buffered, in which case it will be output later.
 *
 * - If all buffers are occupied we will block the thread until one is ready.
 *   This of course wastes time, but this typically only happens when there is
 *   lots and lots of error message output (interactive builds) so it isn't as
 *   bad as it sounds.
 *
 * - When jobs finish they will wait for TTY ownership and flush their queued
 *   messages. This is to avoid the TTY drifting too far off from what's
 *   actually being executed.
 *
 * For the outside observer this will produce log output with all the output
 * for each job together in a nice and linear fashion. For the case when
 * thread-count is 1, it will naturally copy text right out to stdout/stderr
 * without any buffering.
 */

/* Enable copious amounts of linebuf-related debug messages. Useful when
 * changing this code. */
#define LINEBUF_DEBUG 0

#if LINEBUF_DEBUG
#define LINEBUF_PRINTF(expr) \
		do { printf expr ; } while(0)
#else
#define LINEBUF_PRINTF(expr) \
		do { } while(0)
#endif

/* Static limits for the line buffering. */
enum {
	LINEBUF_SIZE = 8192,
	LINEBUF_COUNT = 64
};

/* Describes a line buffer beloning to some job. The name is somewhat of a
 * misnomer as there can typically be multiple lines in one of these. */
typedef struct line_buffer {
	/* The job that has allocated this buffer. */
	int job_id;

	/* Some integer to control sorting amongst the line buffers for a given
	 * job. This is done so we can ignore sorting the queue until it is time to
	 * flush a job. */
	int sort_key;

	/* If non-zero, this output should go to stderr rather than stdout. */
	int is_stderr;

	/* Number of bytes of data valid for this buffer. */
	int len;

	/* Points into buffer_data, never change this. Kept on the side to make
	 * this struct smaller and more cache friendly.*/
	char *data;
} line_buffer;

/* Protects all linebuf-related stuff. */
static pthread_mutex_t linelock;

/* Signalled when it is possible to make progress on output.
 *
 * Either:
 * a) The TTY can be allocated (printing_job is -1).
 * b) There is atleast one free linebuf slot
 */
static pthread_cond_t can_print;

/* The currently printing job. */
static int printing_job = -1;

/* Number of valid pointers in free_linebufs */
static int free_linebuf_count;
/* Number of valid pointers in queued_linebufs */
static int queued_linebuf_count;
static line_buffer* free_linebufs[LINEBUF_COUNT];
static line_buffer* queued_linebufs[LINEBUF_COUNT];

/* Raw linebufs indexed only through free_linebufs and queued_linebufs */
static line_buffer linebufs[LINEBUF_COUNT];

/* Data block for text kept on the side rather than inside the line_buffer
 * structure; otherwise every line_buffer access would be a cache miss. */
static char buffer_data[LINEBUF_COUNT][LINEBUF_SIZE];

int
td_init_exec(void)
{
	int i;
	for (i = 0; i < LINEBUF_COUNT; ++i)
	{
		free_linebufs[i] = &linebufs[i];
		linebufs[i].data = &buffer_data[i][0];
		linebufs[i].data = &buffer_data[i][0];
	}
	free_linebuf_count = LINEBUF_COUNT;
	queued_linebuf_count = 0;

	if (0 != pthread_mutex_init(&linelock, NULL))
		return 1;
	if (0 != pthread_cond_init(&can_print, NULL))
		return 1;
	return 0;
}

static line_buffer *
alloc_line_buffer(void)
{
	assert(free_linebuf_count + queued_linebuf_count == LINEBUF_COUNT);

	if (free_linebuf_count) {
		return free_linebufs[--free_linebuf_count];
	}
	else
		return NULL;
}

static int
sort_buffers(const void *lp, const void *rp)
{
	const line_buffer* li = *(const line_buffer**)lp;
	const line_buffer* ri = *(const line_buffer**)rp;

	return li->sort_key - ri->sort_key;
}


/* Walk the queue and flush all buffers belonging to job_id to stdout/stderr.
 * Then remove these buffers from the queue. This is done either when a job
 * exists or when it is taking control of the TTY (so older, buffered messages
 * are printed before new ones).
 */
static void
flush_output_queue(int job_id)
{
	int i;
	int count = 0;
	line_buffer *buffers[LINEBUF_COUNT];

	LINEBUF_PRINTF(("linebuf: flush queue for job %d\n", job_id));

	for (i = 0; i < queued_linebuf_count; ++i)
	{
		line_buffer *b = queued_linebufs[i];
		if (job_id == b->job_id) {
			buffers[count++] = b;
		}
	}

	LINEBUF_PRINTF(("found %d lines (out of %d total) buffered for job %d\n", count, queued_linebuf_count, job_id));

	/* Flush these buffers to the TTY */

	qsort(&buffers[0], count, sizeof(buffers[0]), sort_buffers);

	for (i = 0; i < count; ++i)
	{
		line_buffer *b = buffers[i];
		write(b->is_stderr ? STDERR_FILENO : STDOUT_FILENO, b->data, b->len);
	}

	/* Compact the queue, removing the buffers from this job. */
	for (i = 0; i < queued_linebuf_count; )
	{
		line_buffer *b = queued_linebufs[i];

		if (b->job_id == job_id)
		{
			/* Move the last pointer to this slot, decrement the queued count.
			 *
			 * This works even for the pathological case of a single queued
			 * value, it will just overwrite itself at index 0.
			 */
			queued_linebufs[i] = queued_linebufs[--queued_linebuf_count];
			free_linebufs[free_linebuf_count++] = b;

#if LINEBUF_DEBUG
			int k, j;
			for (j = 0; j < queued_linebuf_count; ++j)
			{
				for (k = 0; k < free_linebuf_count; ++k)
				{
					assert(queued_linebufs[j] != free_linebufs[k]);
				}
			}
#endif
		}
		else
			++i;
	}

	if (1 == count)
		pthread_cond_broadcast(&can_print);

#if LINEBUF_DEBUG
	LINEBUF_PRINTF(("%d records still queued: [", queued_linebuf_count));
	for (i = 0; i < queued_linebuf_count; ++i) {
		LINEBUF_PRINTF(("%d ", queued_linebufs[i]->job_id));
	}
	LINEBUF_PRINTF(("]\n"));
#endif
}

static int
emit_data(int job_id, int is_stderr, int sort_key, int fd)
{
	char text[LINEBUF_SIZE];
	ssize_t count;
	line_buffer *buf = NULL;

	count = read(fd, text, LINEBUF_SIZE);

	if (count <= 0)
		return -1;

	td_mutex_lock_or_die(&linelock);

	/* Wait for a line buffer to become available, or for the tty to become free. */
	for (;;)
	{
		/* If we already have the tty, or we can get it: break */
		if (-1 == printing_job || job_id == printing_job)
			break;

		/* If we can allocate a buffer: break */

		if (NULL != (buf = alloc_line_buffer()))
			break;

		/* Otherwise wait */
		pthread_cond_wait(&can_print, &linelock);
	}

	if (-1 == printing_job)
	{
		/* Let this job own the output channel */
		LINEBUF_PRINTF(("job %d is taking the tty\n", job_id));
		printing_job = job_id;

		flush_output_queue(job_id);
	}
	else if (job_id != printing_job)
	{
		assert(buf);
	}

	if (!buf)
	{
		/* This thread owns the TTY, so just print. We don't need to keep the
		 * mutex locked as this job will not finsh and reset the currently
		 * printing job until later. Releasing the mutex now means other
		 * threads can come in and buffer their data. */

		td_mutex_unlock_or_die(&linelock);

		LINEBUF_PRINTF(("copying %d bytes of data from job_id %d, fd %d, sort_key %d\n",
					(int) count, job_id, fd, sort_key));

		write(is_stderr ? STDERR_FILENO : STDOUT_FILENO, text, count);
	}
	else
	{
		/* We can't print this data as we don't own the TTY so we buffer it. */
		LINEBUF_PRINTF(("buffering %d bytes of data from job_id %d, fd %d, sort_key %d\n",
					(int) count, job_id, fd, sort_key));

		/* PERF: Could release mutex around this memcpy, not sure if it's a win. */
		buf->job_id = job_id;
		buf->sort_key = sort_key;
		buf->len = count;
		buf->is_stderr = is_stderr;
		memcpy(buf->data, text, count);

		/* Queue this line for output */
		queued_linebufs[queued_linebuf_count++] = buf;

		td_mutex_unlock_or_die(&linelock);
	}
	return 0;
}

static void
on_job_exit(int job_id)
{
	/* Find any queued buffers for this job and flush them. */
	td_mutex_lock_or_die(&linelock);

	LINEBUF_PRINTF(("exit job %d\n", job_id));

	if (job_id != printing_job) {
		/* Wait until we can grab the TTY */
		while (-1 != printing_job) {
			pthread_cond_wait(&can_print, &linelock);
		}
	}

	printing_job = job_id;

	flush_output_queue(job_id);

	printing_job = -1;

	td_mutex_unlock_or_die(&linelock);

	pthread_cond_broadcast(&can_print);
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

		if (annotation)
			puts(annotation);

		if (echo_cmdline)
			puts(cmd_line);

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
		int return_code;
		int sort_key = 0;
		int rfd_count = 2;
		int rfds[2];
		fd_set read_fds;

		rfds[0] = stdout_pipe[pipe_read];
		rfds[1] = stderr_pipe[pipe_read];

		/* Close write end of the pipe, we're just going to be reading */
		close(stdout_pipe[pipe_write]);
		close(stderr_pipe[pipe_write]);

		/* Sit in a select loop over the two fds */
		while (rfd_count > 0)
		{
			int fd;
			int count;
			int max_fd = 0;

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

			count = select(max_fd, &read_fds, NULL, NULL, NULL);

			if (-1 == count) // happens in gdb due to syscall interruption
				continue;

			for (fd = 0; fd < 2; ++fd)
			{
				if (!FD_ISSET(rfds[fd], &read_fds))
					continue;

				if (0 == emit_data(job_id, /*is_stderr:*/ 1 == fd, sort_key++, rfds[fd]))
					continue;

				/* Done with this FD. */
				rfds[fd] = 0;
				--rfd_count;
			}
		}

		close(stdout_pipe[pipe_read]);
		close(stderr_pipe[pipe_read]);

		p = waitpid(child, &return_code, 0);
		if (p != child)
		{
			perror("waitpid failed");
			return 1;
		}

		on_job_exit(job_id);
	
		*was_signalled_out = WIFSIGNALED(return_code);
		return return_code;
	}
}

#endif /* TUNDRA_UNIX */

