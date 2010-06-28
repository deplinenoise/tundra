
#include "engine.h"
#include "util.h"
#include "scanner.h"

#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

typedef struct td_job_queue_tag
{
	pthread_mutex_t mutex;
	pthread_cond_t work_avail;
	td_engine *engine;

	int head;
	int tail;
	int array_size;
	td_node **array;

	int quit;
} td_job_queue;

static int
scan_implicit_deps(td_job_queue *queue, td_node *node)
{
	td_scanner *scanner = node->scanner;
	int result;

	if (!scanner)
		return 0;

	pthread_mutex_unlock(&queue->mutex);

	result = (*scanner->scan_fn)(queue->engine, node, scanner);

	pthread_mutex_lock(&queue->mutex);
	return result;
}

static int
run_job(td_job_queue *queue, td_node *node)
{
	return 1;
}

static int is_queued(td_node *node) { return node->job.flags & TD_JOBF_QUEUED; }
static int is_root(td_node *node) { return node->job.flags >= TD_JOBF_ROOT; }
static int is_completed(td_node *node) { return node->job.state >= TD_JOB_COMPLETED; }

static void
enqueue(td_job_queue *queue, td_node *node)
{
	assert(is_root(node) || !is_queued(node));

	node->job.flags |= TD_JOBF_QUEUED;

	assert((queue->tail - queue->head) < queue->array_size);

	if (queue->engine->debug_level > 10)
		printf("enqueueing %s\n", node->annotation);

	queue->array[queue->tail % queue->array_size] = node;
	++queue->tail;
}

static const char *
jobstate_name(td_jobstate s)
{
	switch (s)
	{
	case TD_JOB_INITIAL: return "initial";
	case TD_JOB_BLOCKED: return "blocked";
	case TD_JOB_SCANNING: return "scanning";
	case TD_JOB_RUNNING: return "running";
	case TD_JOB_COMPLETED: return "completed";
	case TD_JOB_FAILED: return "failed";
	case TD_JOB_CANCELLED: return "cancelled";
	}
	assert(0);
	return "";
}

static void
transition_job(td_job_queue *queue, td_node *node, td_jobstate new_state)
{
	if (queue->engine->debug_level > 9)
	{
		printf("[%s] %s -> %s { %d blockers }\n",
				node->annotation,
				jobstate_name(node->job.state), 
				jobstate_name(new_state),
				node->job.block_count);
	}

	node->job.state = new_state;
}

static void
advance_job(td_job_queue *queue, td_node *node)
{
	td_jobstate state;
	while ((state = node->job.state) < TD_JOB_COMPLETED)
	{
		switch (state)
		{
		case TD_JOB_INITIAL:
			if (node->job.block_count > 0)
			{
				/* enqueue any blocking jobs and transition to the blocked state */
				int i, count, bc = 0;
				td_node **deps = node->deps;
				for (i = 0, count = node->dep_count; i < count; ++i)
				{
					td_node *dep = deps[i];
					if (!is_completed(dep))
					{
						++bc;
						if (!is_queued(dep))
							enqueue(queue, dep);
					}
				}
				assert(bc == node->job.block_count);
				transition_job(queue, node, TD_JOB_BLOCKED);
				pthread_cond_broadcast(&queue->work_avail);
				return;
			}
			else
			{
				/* nothing is blocking this job, so scan implicit deps immediately */
				transition_job(queue, node, TD_JOB_SCANNING);
			}
			break;

		case TD_JOB_BLOCKED:
			assert(0 == node->job.block_count);
			transition_job(queue, node, TD_JOB_SCANNING);
			break;

		case TD_JOB_SCANNING:
			if (0 != scan_implicit_deps(queue, node))
				transition_job(queue, node, TD_JOB_FAILED);
			else
				transition_job(queue, node, TD_JOB_RUNNING);
			break;

		case TD_JOB_RUNNING:
			if (0 != run_job(queue, node))
				transition_job(queue, node, TD_JOB_FAILED);
			else
				transition_job(queue, node, TD_JOB_COMPLETED);
			break;

		default:
			assert(0);
			td_croak("can't get here");
			break;
		}
	}

	/* unblock all jobs that are waiting for this job and enqueue them */
	{
		int qcount = 0;
		td_job_chain *chain = node->job.pending_jobs;
		while (chain)
		{
			td_node *n = chain->node;
			assert(!is_completed(n));
			if (0 == --n->job.block_count)
			{
				if (!is_queued(n))
					enqueue(queue, n);
				++qcount;
			}
			chain = chain->next;
		}

		if (1 < qcount)
			pthread_cond_broadcast(&queue->work_avail);
		else if (1 == qcount)
			pthread_cond_signal(&queue->work_avail);
	}
}

static void *
build_worker(void *arg)
{
	td_job_queue * const queue = (td_job_queue *) arg;

	pthread_mutex_lock(&queue->mutex);

	while (!queue->quit)
	{
		td_node *node;
		int slot;
		int count = queue->tail - queue->head;

		if (0 == count)
		{
			pthread_cond_wait(&queue->work_avail, &queue->mutex);
			continue;
		}

		slot = (queue->head++) % queue->array_size;

		node = queue->array[slot];
		queue->array[slot] = NULL;

		node->job.flags &= ~TD_JOBF_QUEUED;

		advance_job(queue, node);

		if (is_completed(node) && is_root(node))
			queue->quit = 1;
	}

	pthread_mutex_unlock(&queue->mutex);

	return NULL;
}

#define TD_MAX_THREADS (32)

int
td_build(td_engine *engine, td_node *node, int thread_count)
{
	int i;
	pthread_t threads[TD_MAX_THREADS];

	td_job_queue queue;
	memset(&queue, 0, sizeof(queue));
	queue.engine = engine;

	pthread_mutex_init(&queue.mutex, NULL);
	pthread_cond_init(&queue.work_avail, NULL);
	queue.array_size = engine->node_count;
	queue.array = (td_node **) calloc(engine->node_count, sizeof(td_node*));

	if (thread_count > TD_MAX_THREADS)
		thread_count = TD_MAX_THREADS;

	for (i = 0; i < thread_count-1; ++i)
	{
		int rc = pthread_create(&threads[i], NULL, build_worker, &queue);
		if (0 != rc)
			td_croak("couldn't start thread %d: %s", i, strerror(rc));
	}

	pthread_mutex_lock(&queue.mutex);

	node->job.flags |= TD_JOBF_ROOT;
	enqueue(&queue, node);

	pthread_mutex_unlock(&queue.mutex);
	pthread_cond_broadcast(&queue.work_avail);

	build_worker(&queue);

	pthread_cond_broadcast(&queue.work_avail);
	for (i = thread_count - 2; i >= 0; --i)
	{
		void *res;
	   	int rc = pthread_join(threads[i], &res);
		if (0 != rc)
			td_croak("couldn't join thread %d: %s", i, strerror(rc));
	}

	free(queue.array);
	pthread_cond_destroy(&queue.work_avail);
	pthread_mutex_destroy(&queue.mutex);

	return 0;
}
