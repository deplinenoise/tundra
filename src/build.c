
#include "build.h"
#include "engine.h"
#include "util.h"
#include "scanner.h"
#include "md5.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "portable.h"

typedef struct td_job_queue
{
	pthread_mutex_t mutex;
	pthread_cond_t work_avail;
	td_engine *engine;
	td_sighandler_info siginfo;

	int head;
	int tail;
	int array_size;
	td_node **array;

	int jobs_run;
	int fail_count;
} td_job_queue;

static int
scan_implicit_deps(td_job_queue *queue, td_node *node)
{
	double t1, t2;
	td_scanner *scanner = node->scanner;
	int result;

	if (!scanner)
		return 0;

	t1 = td_timestamp();
	if (!queue->engine->settings.dry_run)
		result = (*scanner->scan_fn)(queue->engine, &queue->mutex, node, scanner);
	else
		result = 0;
	t2 = td_timestamp();

	queue->engine->stats.scan_time += t2 - t1;

	return result;
}

static int
ensure_dir_exists(td_engine *engine, td_file *dir)
{
	int result;
	const td_stat *stat;
	td_file *parent_dir;

	if (engine->settings.dry_run)
		return 0;

	parent_dir = td_parent_dir(engine, dir);
	if (parent_dir)
	{
		if (0 != (result = ensure_dir_exists(engine, parent_dir)))
			return result;
	}

	stat = td_stat_file(engine, dir);
	if (TD_STAT_EXISTS & stat->flags)
	{
		if (0 == (stat->flags & TD_STAT_DIR))
		{
			fprintf(stderr, "%s: couldn't create directory; file exists\n", dir->path);
			return 1;
		}
		else
			return 0;
	}
	else
	{
		if (0 != (result = td_mkdir(dir->path)))
		{
			fprintf(stderr, "%s: couldn't create directory\n", dir->path);
			return result;
		}

		/* could optimize to just set as a directory rather than stat again */
		td_touch_file(dir);
		return 0;
	}
}

static int
run_job(td_job_queue *queue, td_node *node)
{
	double t1, t2;
	td_engine *engine = queue->engine;
	int i, count, result, was_signalled = 0;
	const char *command = node->action;

	if (!command || '\0' == command[0])
		return 0;

	t1 = td_timestamp();
	/* ensure directories for output files exist */
	for (i = 0, count = node->output_count; i < count; ++i)
	{
		td_file *dir = td_parent_dir(engine, node->outputs[i]);
		if (0 != (result = ensure_dir_exists(engine, dir)))
			return result;
	}
	t2 = td_timestamp();
	engine->stats.mkdir_time += t2 - t1;

	++queue->jobs_run;

	pthread_mutex_unlock(&queue->mutex);
	if (td_verbosity_check(engine, 1))
		printf("%s\n", node->annotation);
	t1 = td_timestamp();
	if (td_verbosity_check(engine, 2))
		printf("%s\n", command);
	if (!engine->settings.dry_run)
		result = td_exec(command, &was_signalled);
	else
		result = 0;
	t2 = td_timestamp();
	pthread_mutex_lock(&queue->mutex);

	if (0 != result)
	{
		/* Maintain a fail count so we can track why we stopped building if
		 * we're stopping after the first error. Otherwise it might appear as
		 * we succeeded. */
		++queue->fail_count;

		/* If the command failed or was signalled (e.g. Ctrl+C), abort the build */
		if (was_signalled)
			queue->siginfo.flag = -1;
		else if (!engine->settings.continue_on_error)
			queue->siginfo.flag = 1;
	}

	engine->stats.build_time += t2 - t1;

	if (0 != result)
	{
		pthread_mutex_unlock(&queue->mutex);
		/* remove all output files */
		for (i = 0, count = node->output_count; i < count; ++i)
			remove(node->outputs[i]->path);
		pthread_mutex_lock(&queue->mutex);
	}

	/* mark all output files as dirty */
	for (i = 0, count = node->output_count; i < count; ++i)
		td_touch_file(node->outputs[i]);

	return result;
}

static int is_queued(td_node *node) { return node->job.flags & TD_JOBF_QUEUED; }
static int is_root(td_node *node) { return node->job.flags & TD_JOBF_ROOT; }
static int is_completed(td_node *node) { return node->job.state >= TD_JOB_COMPLETED; }
static int is_failed(td_node *node) { return node->job.state == TD_JOB_FAILED; }

static void
enqueue(td_job_queue *queue, td_node *node)
{
	assert(is_root(node) || !is_queued(node));

	node->job.flags |= TD_JOBF_QUEUED;

	assert((queue->tail - queue->head) < queue->array_size);

	if (td_debug_check(queue->engine, TD_DEBUG_QUEUE))
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
	case TD_JOB_UPTODATE: return "up-to-date";
	}
	assert(0);
	return "";
}

static void
transition_job(td_job_queue *queue, td_node *node, td_jobstate new_state)
{
	if (td_debug_check(queue->engine, TD_DEBUG_QUEUE))
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
update_input_signature(td_engine *engine, td_node *node)
{
	static unsigned char zero_byte = 0;
	int i, count;
	MD5_CTX context;

	MD5Init(&context);

	for (i = 0, count = node->input_count; i < count; ++i)
	{
		td_file *input_file = node->inputs[i];
		td_digest *digest = td_get_signature(engine, input_file);
		MD5Update(&context, digest->data, sizeof(digest->data));
	}

	/* add a separator between the inputs and implicit deps */
	MD5Update(&context, &zero_byte, 1);

	for (i = 0, count = node->job.idep_count; i < count; ++i)
	{
		td_file *dep = node->job.ideps[i];
		td_digest *digest = td_get_signature(engine, dep);
		MD5Update(&context, digest->data, sizeof(digest->data));
	}

	MD5Final(node->job.input_signature.data, &context);
}

static int
is_up_to_date(td_job_queue *queue, td_node *node)
{
	double t1, t2;

	int i, count;
	const td_digest *prev_signature = NULL;
	td_engine *engine = queue->engine;
	const td_ancestor_data *ancestor;

	int up_to_date = 0;

	t1 = td_timestamp();
	/* rebuild if any output files are missing */
	for (i = 0, count = node->output_count; i < count; ++i)
	{
		td_file *file = node->outputs[i];
		const td_stat *stat = td_stat_file(engine, file);
		if (0 == (stat->flags & TD_STAT_EXISTS))
		{
			if (td_debug_check(engine, TD_DEBUG_REASON))
				printf("%s: output file %s is missing\n", node->annotation, file->path);
			goto leave;
		}
	}

	if (NULL != (ancestor = node->ancestor_data))
		prev_signature = &ancestor->input_signature;

	/* rebuild if there is no stored signature */
	if (!prev_signature)
	{
		if (td_debug_check(engine, TD_DEBUG_REASON))
			printf("%s: no previous input signature\n", node->annotation);
		goto leave;
	}

	/* rebuild if the job failed last time */
	if (TD_JOB_FAILED == ancestor->job_result)
	{
		if (td_debug_check(engine, TD_DEBUG_REASON))
			printf("%s: build failed last time\n", node->annotation);
		goto leave;
	}

	/* rebuild if the input signatures have changed */
	if (0 != memcmp(prev_signature->data, node->job.input_signature.data, sizeof(td_digest)))
	{
		if (td_debug_check(engine, TD_DEBUG_REASON))
			printf("%s: input signature differs\n", node->annotation);
		goto leave;
	}

	/* otherwise, the node is up to date */
	up_to_date = 1;

leave:
	t2 = td_timestamp();
	engine->stats.up2date_check_time += t2 - t1;
	return up_to_date;
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
						if (!is_queued(dep) && dep->job.state < TD_JOB_BLOCKED)
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
			if (0 == node->job.failed_deps)
				transition_job(queue, node, TD_JOB_SCANNING);
			else
				transition_job(queue, node, TD_JOB_FAILED);
			break;

		case TD_JOB_SCANNING:

			if (0 == scan_implicit_deps(queue, node))
			{
				update_input_signature(queue->engine, node);

				if (is_up_to_date(queue, node))
					transition_job(queue, node, TD_JOB_UPTODATE);
				else
					transition_job(queue, node, TD_JOB_RUNNING);
			}
			else
			{
				/* implicit dependency scanning failed */
				transition_job(queue, node, TD_JOB_FAILED);
			}
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

	if (is_completed(node))
	{
		int qcount = 0;
		td_job_chain *chain = node->job.pending_jobs;

		if (td_debug_check(queue->engine, TD_DEBUG_QUEUE))
			printf("%s completed - enqueing blocked jobs\n", node->annotation);

		/* unblock all jobs that are waiting for this job and enqueue them */
		while (chain)
		{
			td_node *n = chain->node;

			if (is_failed(node))
				n->job.failed_deps++;

			/* nodes blocked on this node can't be completed yet */
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

	while (!queue->siginfo.flag)
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
			queue->siginfo.flag = 1;
	}

	pthread_mutex_unlock(&queue->mutex);
	pthread_cond_broadcast(&queue->work_avail);

	return NULL;
}

#define TD_MAX_THREADS (32)

td_build_result
td_build(td_engine *engine, td_node *node, int *jobs_run)
{
	int i;
	int thread_count;
	pthread_t threads[TD_MAX_THREADS];
	td_job_queue queue;

	if (engine->stats.build_called)
		td_croak("build() called more than once on same engine");

	engine->stats.build_called = 1;

	memset(&queue, 0, sizeof(queue));
	queue.engine = engine;

	pthread_mutex_init(&queue.mutex, NULL);
	pthread_cond_init(&queue.work_avail, NULL);
	queue.array_size = engine->node_count;
	queue.array = (td_node **) calloc(engine->node_count, sizeof(td_node*));

	queue.siginfo.mutex = &queue.mutex;
	queue.siginfo.cond = &queue.work_avail;

	thread_count = engine->settings.thread_count;
	if (thread_count > TD_MAX_THREADS)
		thread_count = TD_MAX_THREADS;

	if (td_debug_check(engine, TD_DEBUG_QUEUE))
		printf("using %d build threads\n", thread_count);

	td_block_signals(1);
	td_install_sighandler(&queue.siginfo);

	for (i = 0; i < thread_count-1; ++i)
	{
		int rc;
		if (td_debug_check(engine, TD_DEBUG_QUEUE))
			printf("starting thread %d\n", i);
		rc = pthread_create(&threads[i], NULL, build_worker, &queue);
		if (0 != rc)
			td_croak("couldn't start thread %d: %s", i, strerror(rc));
	}

	pthread_mutex_lock(&queue.mutex);

	node->job.flags |= TD_JOBF_ROOT;
	enqueue(&queue, node);

	pthread_mutex_unlock(&queue.mutex);
	pthread_cond_broadcast(&queue.work_avail);

	build_worker(&queue);

	if (td_verbosity_check(engine, 2) && -1 == queue.siginfo.flag)
		printf("*** aborted on signal %s\n", queue.siginfo.reason);

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

	*jobs_run = queue.jobs_run;

	/* there is a tiny race condition here if a user presses Ctrl-C just
	 * before the write to the static pointer occurs, but that's in nanosecond
	 * land */
	td_remove_sighandler();

	if (queue.siginfo.flag < 0)
		return TD_BUILD_ABORTED;
	else if (queue.fail_count)
		return TD_BUILD_FAILED;
	else
		return TD_BUILD_SUCCESS;
}

const char * const td_build_result_names[] =
{
	"success",
	"failed",
	"aborted on signal"
};

