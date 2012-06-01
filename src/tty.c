/*
   Copyright 2010-2011 Andreas Fredriksson

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
#include "config.h"

#ifndef TUNDRA_WIN32

#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>

/* tty.c - line buffer handling to linearize output from overlapped command
 * execution
 *
 * This solves the issue of having multiple programs (compilers and such)
 * writing output to both stdout and stderr at the same time. Normally on UNIX
 * the terminal will arbitrate amongst them in a decent fashion, but it really
 * screws up long error messages that span several lines. On Win32, commands
 * will happily write all over each other and there's no help from the
 * operating system console so a custom solution must be devised.
 *
 * The solution implemented here works as follows (on UNIX, win32 is similar).
 *
 * - We assign the TTY to one job thread which is allowed to echo its child
 *   process text directly. The job id of the current TTY owner is stored in
 *   `printing_job'.
 *
 * - exec_unix.c: We create pipes for stdout and stderr for all spawned child processes. We
 *   loop around these fds using select() until they both return EOF.
 *
 * - exec_unix.c: Whenever select() returns we try to read up to LINEBUF_SIZE bytes. If
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
#define TTY_DEBUG 0

#if TTY_DEBUG
#define TTY_PRINTF(expr) \
		do { printf expr ; } while(0)
#else
#define TTY_PRINTF(expr) \
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
tty_init(void)
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
 * exits or when it is taking control of the TTY (so older, buffered messages
 * are printed before new ones).
 */
static void
flush_output_queue(int job_id)
{
	int i;
	int count = 0;
	int result;
	line_buffer *buffers[LINEBUF_COUNT];

	TTY_PRINTF(("linebuf: flush queue for job %d\n", job_id));

	for (i = 0; i < queued_linebuf_count; ++i)
	{
		line_buffer *b = queued_linebufs[i];
		if (job_id == b->job_id) {
			buffers[count++] = b;
		}
	}

	TTY_PRINTF(("found %d lines (out of %d total) buffered for job %d\n", count, queued_linebuf_count, job_id));

	/* Flush these buffers to the TTY */

	qsort(&buffers[0], count, sizeof(buffers[0]), sort_buffers);

	for (i = 0; i < count; ++i)
	{
		line_buffer *b = buffers[i];
		result = write(b->is_stderr ? STDERR_FILENO : STDOUT_FILENO, b->data, strlen(b->data));
		(void) result; /* tty disappeared; not fatal */
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

#if TTY_DEBUG
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
		pthread_cond_signal(&can_print);
	else if (count > 1)
		pthread_cond_broadcast(&can_print);

#if TTY_DEBUG
	TTY_PRINTF(("%d records still queued: [", queued_linebuf_count));
	for (i = 0; i < queued_linebuf_count; ++i) {
		TTY_PRINTF(("%d ", queued_linebufs[i]->job_id));
	}
	TTY_PRINTF(("]\n"));
#endif
}

void
tty_emit(int job_id, int is_stderr, int sort_key, const char *data, int len)
{
	int result;
	line_buffer *buf = NULL;

	td_mutex_lock_or_die(&linelock);

	while (len > 0)
	{
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
			TTY_PRINTF(("job %d is taking the tty\n", job_id));
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

			TTY_PRINTF(("copying %d bytes of data from job_id %d, stderr=%d, sort_key %d\n",
						len, job_id, is_stderr, sort_key));

			result = write(is_stderr ? STDERR_FILENO : STDOUT_FILENO, data, strlen(data));
			(void) result; /* tty disappeared; not fatal */
			return; /* finish the loop immediately */
		}
		else
		{
			/* We can't print this data as we don't own the TTY so we buffer it. */
			TTY_PRINTF(("buffering %d bytes of data from job_id %d, stderr=%d, sort_key %d\n",
						len, job_id, is_stderr, sort_key));

			/* PERF: Could release mutex around this memcpy, not sure if it's a win. */
			buf->job_id = job_id;
			buf->sort_key = sort_key;
			buf->len = len > LINEBUF_SIZE - 1 ? LINEBUF_SIZE - 1 : len;
			buf->is_stderr = is_stderr;
			memcpy(buf->data, data, buf->len);
			buf->data[buf->len] = '\0';

			len -= buf->len;
			data += buf->len;

			/* Queue this line for output */
			queued_linebufs[queued_linebuf_count++] = buf;
		}
	}

	td_mutex_unlock_or_die(&linelock);

	return;
}

void
tty_job_exit(int job_id)
{
	/* Find any queued buffers for this job and flush them. */
	td_mutex_lock_or_die(&linelock);

	TTY_PRINTF(("exit job %d\n", job_id));

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

void tty_printf(int job_id, int sort_key, const char *format, ...)
{
	char buffer[2048];
	va_list a;
	va_start(a, format);
	vsnprintf(buffer, sizeof(buffer), format, a);
	buffer[sizeof(buffer)-1] = '\0';
	tty_emit(job_id, 0, sort_key, buffer, (int) strlen(buffer));
	va_end(a);
}

#endif
