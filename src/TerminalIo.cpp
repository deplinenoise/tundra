#include "TerminalIo.hpp"

#include "Mutex.hpp"
#include "ConditionVar.hpp"

#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>

/* Line buffer handling to linearize output from overlapped command execution
 *
 * This solves the issue of having multiple programs (compilers and such)
 * writing output to both stdout and stderr at the same time. Normally on UNIX
 * the terminal will arbitrate amongst them in a decent fashion, but it really
 * screws up long error messages that span several lines. On Win32, commands
 * will happily write all over each other and there's no help from the
 * operating system console so a custom solution must be devised.
 *
 * The solution implemented here works as follows:
 *
 * - We assign the TTY to one job thread which is allowed to echo its child
 *   process text directly. The job id of the current TTY owner is stored in
 *   `s_PrintingJobIndex'.
 *
 * - exec_unix.c: We create pipes for stdout and stderr for all spawned child processes. We
 *   loop around these fds using select() until they both return EOF.
 *
 * - exec_unix.c: Whenever select() returns we try to read up to kLineBufSize bytes. If
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

namespace t2
{

/* Static limits for the line buffering. */
enum {
	kLineBufSize = 8192,
	kLineBufCount = 64
};

/* Describes a line buffer beloning to some job. The name is somewhat of a
 * misnomer as there can typically be multiple lines in one of these. */
struct LineBuffer {
	/* The job that has allocated this buffer. */
	int m_JobId;

	/* Some integer to control sorting amongst the line buffers for a given
	 * job. This is done so we can ignore sorting the queue until it is time to
	 * flush a job. */
	int m_SortKey;

	/* If non-zero, this output should go to stderr rather than stdout. */
	int m_IsStderr;

	/* Number of bytes of data valid for this buffer. */
	int m_Len;

	/* Points into s_BufferData, never change this. Kept on the side to make
	 * this struct smaller and more cache friendly.*/
	char *m_Data;
};

/* Protects all linebuf-related stuff. */
static Mutex s_LineLock;

/* Signalled when it is possible to make progress on output.
 *
 * Either:
 * a) The TTY can be allocated (s_PrintingJobIndex is -1).
 * b) There is atleast one free linebuf slot
 */
static ConditionVariable s_CanPrint;

/* The currently printing job. */
static int s_PrintingJobIndex = -1;

/* Number of valid pointers in s_FreeLinebufs */
static int s_FreeLinebufCount;

/* Number of valid pointers in s_QueuedLinebufs */
static int s_QueuedLinebufCount;
static LineBuffer* s_FreeLinebufs[kLineBufCount];
static LineBuffer* s_QueuedLinebufs[kLineBufCount];

/* Raw s_Linebufs indexed only through s_FreeLinebufs and s_QueuedLinebufs */
static LineBuffer s_Linebufs[kLineBufCount];

/* Data block for text kept on the side rather than inside the LineBuffer
 * structure; otherwise every LineBuffer access would be a cache miss. */
static char s_BufferData[kLineBufCount][kLineBufSize];

void TerminalIoInit(void)
{
	int i;
	for (i = 0; i < kLineBufCount; ++i)
	{
		s_FreeLinebufs[i] = &s_Linebufs[i];
		s_Linebufs[i].m_Data = &s_BufferData[i][0];
		s_Linebufs[i].m_Data = &s_BufferData[i][0];
	}
	s_FreeLinebufCount = kLineBufCount;
	s_QueuedLinebufCount = 0;


  MutexInit(&s_LineLock);
  CondInit(&s_CanPrint);
}

void TerminalIoDestroy(void)
{
  CondDestroy(&s_CanPrint);
  MutexDestroy(&s_LineLock);
}

static LineBuffer* AllocLineBuffer(void)
{
	assert(s_FreeLinebufCount + s_QueuedLinebufCount == kLineBufCount);

	if (s_FreeLinebufCount) {
		return s_FreeLinebufs[--s_FreeLinebufCount];
	}
	else
		return NULL;
}

static int CompareBuffers(const void *lp, const void *rp)
{
	const LineBuffer* li = *(const LineBuffer**)lp;
	const LineBuffer* ri = *(const LineBuffer**)rp;

	return li->m_SortKey - ri->m_SortKey;
}


/* Walk the queue and flush all buffers belonging to job_id to stdout/stderr.
 * Then remove these buffers from the queue. This is done either when a job
 * exits or when it is taking control of the TTY (so older, buffered messages
 * are printed before new ones).
 */
static void FlushOutputQueue(int job_id)
{
	int i;
	int count = 0;
	int result;
	LineBuffer *buffers[kLineBufCount];

	TTY_PRINTF(("linebuf: flush queue for job %d\n", job_id));

	for (i = 0; i < s_QueuedLinebufCount; ++i)
	{
		LineBuffer *b = s_QueuedLinebufs[i];
		if (job_id == b->m_JobId) {
			buffers[count++] = b;
		}
	}

	TTY_PRINTF(("found %d lines (out of %d total) buffered for job %d\n", count, s_QueuedLinebufCount, job_id));

	/* Flush these buffers to the TTY */

	qsort(&buffers[0], count, sizeof(buffers[0]), CompareBuffers);

	for (i = 0; i < count; ++i)
	{
		LineBuffer *b = buffers[i];
		result = write(b->m_IsStderr ? STDERR_FILENO : STDOUT_FILENO, b->m_Data, b->m_Len);
		(void) result; /* tty disappeared; not fatal */
	}

	/* Compact the queue, removing the buffers from this job. */
	for (i = 0; i < s_QueuedLinebufCount; )
	{
		LineBuffer *b = s_QueuedLinebufs[i];

		if (b->m_JobId == job_id)
		{
			/* Move the last pointer to this slot, decrement the queued count.
			 *
			 * This works even for the pathological case of a single queued
			 * value, it will just overwrite itself at index 0.
			 */
			s_QueuedLinebufs[i] = s_QueuedLinebufs[--s_QueuedLinebufCount];
			s_FreeLinebufs[s_FreeLinebufCount++] = b;

#if TTY_DEBUG
			int k, j;
			for (j = 0; j < s_QueuedLinebufCount; ++j)
			{
				for (k = 0; k < s_FreeLinebufCount; ++k)
				{
					assert(s_QueuedLinebufs[j] != s_FreeLinebufs[k]);
				}
			}
#endif
		}
		else
			++i;
	}

	if (1 == count)
		CondSignal(&s_CanPrint);
	else if (count > 1)
		CondBroadcast(&s_CanPrint);

#if TTY_DEBUG
	TTY_PRINTF(("%d records still queued: [", s_QueuedLinebufCount));
	for (i = 0; i < s_QueuedLinebufCount; ++i) {
		TTY_PRINTF(("%d ", s_QueuedLinebufs[i]->m_JobId));
	}
	TTY_PRINTF(("]\n"));
#endif
}

void TerminalIoEmit(int job_id, int is_stderr, int sort_key, const char *data, int len)
{
	int result;
	LineBuffer *buf = NULL;

  MutexLock(&s_LineLock);

	while (len > 0)
	{
		/* Wait for a line buffer to become available, or for the tty to become free. */
		for (;;)
		{
			/* If we already have the tty, or we can get it: break */
			if (-1 == s_PrintingJobIndex || job_id == s_PrintingJobIndex)
				break;

			/* If we can allocate a buffer: break */

			if (NULL != (buf = AllocLineBuffer()))
				break;

			/* Otherwise wait */
      CondWait(&s_CanPrint, &s_LineLock);
		}

		if (-1 == s_PrintingJobIndex)
		{
			/* Let this job own the output channel */
			TTY_PRINTF(("job %d is taking the tty\n", job_id));
			s_PrintingJobIndex = job_id;

			FlushOutputQueue(job_id);
		}
		else if (job_id != s_PrintingJobIndex)
		{
			assert(buf);
		}

		if (!buf)
		{
			/* This thread owns the TTY, so just print. We don't need to keep the
			 * mutex locked as this job will not finsh and reset the currently
			 * printing job until later. Releasing the mutex now means other
			 * threads can come in and buffer their data. */

			MutexUnlock(&s_LineLock);

			TTY_PRINTF(("copying %d bytes of data from job_id %d, stderr=%d, sort_key %d\n",
						len, job_id, is_stderr, sort_key));

			result = write(is_stderr ? STDERR_FILENO : STDOUT_FILENO, data, len);
			(void) result; /* tty disappeared; not fatal */
			return; /* finish the loop immediately */
		}
		else
		{
			/* We can't print this data as we don't own the TTY so we buffer it. */
			TTY_PRINTF(("buffering %d bytes of data from job_id %d, stderr=%d, sort_key %d\n",
						len, job_id, is_stderr, sort_key));

			/* PERF: Could release mutex around this memcpy, not sure if it's a win. */
			buf->m_JobId = job_id;
			buf->m_SortKey = sort_key;
			buf->m_Len = len > kLineBufSize - 1 ? kLineBufSize - 1 : len;
			buf->m_IsStderr = is_stderr;
			memcpy(buf->m_Data, data, buf->m_Len);
			buf->m_Data[buf->m_Len] = '\0';

			len -= buf->m_Len;
			data += buf->m_Len;

			/* Queue this line for output */
			s_QueuedLinebufs[s_QueuedLinebufCount++] = buf;
		}
	}

	MutexUnlock(&s_LineLock);

	return;
}

void TerminalIoJobExit(int job_id)
{
	/* Find any queued buffers for this job and flush them. */
	MutexLock(&s_LineLock);

	TTY_PRINTF(("exit job %d\n", job_id));

	if (job_id != s_PrintingJobIndex) {
		/* Wait until we can grab the TTY */
		while (-1 != s_PrintingJobIndex) {
			CondWait(&s_CanPrint, &s_LineLock);
		}
	}

	s_PrintingJobIndex = job_id;

	FlushOutputQueue(job_id);

	s_PrintingJobIndex = -1;

	MutexUnlock(&s_LineLock);

	CondBroadcast(&s_CanPrint);
}

void TerminalIoPrintf(int job_id, int sort_key, const char *format, ...)
{
	char buffer[16384];
	va_list a;
	va_start(a, format);
	vsnprintf(buffer, sizeof(buffer), format, a);
	buffer[sizeof(buffer)-1] = '\0';
	TerminalIoEmit(job_id, 0, sort_key, buffer, (int) strlen(buffer));
	va_end(a);
}

}
