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

/* build.c - threaded build engine */

/*
 * Here are the main components of this module:
 *
 * The build queue: A ring buffer of nodes that can be advanced; protected by a
 * mutex. There's a condition variable signalling that there is work to be done
 * (or the queue should be aborted). This mutex also protects all node job state.
 *
 * The build_worker function: All build threads (including the main thread) go here
 *
 * Data access rules:
 *
 * td_engine:
 *   - settings are fine to read (invariant)
 *   - stats: must lock engine->lock
 *
 * td_file:
 *   - create/stat/signature: go through accessors in engine.h so engine can lock proper object-hashed lock.
 *   - hash, path etc: fine to read (invariant)
 *
 * td_node:
 *   - input data (inputs, outputs, ancestors) can be read at will (it is invariant)
 *   - job data (job sub-struct) -- queue lock must be held
 *
 * queue:
 *   - queue lock must be held at all times when accesses
 */

#include "build.h"
#include "engine.h"
#include "util.h"
#include "scanner.h"
#include "md5.h"
#include "files.h"

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
	int thread_count;

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
	int collect_stats;
	double t1 = 0.0, t2 = 0.0;
	td_engine *engine = queue->engine;
	td_scanner *scanner = node->scanner;
	int result;

	if (!scanner)
		return 0;

	collect_stats = td_debug_check(engine, TD_DEBUG_STATS);

	td_mutex_unlock_or_die(&queue->mutex);

	if (collect_stats)
		t1 = td_timestamp();

	if (!queue->engine->settings.dry_run)
		result = (*scanner->scan_fn)(queue->engine, node, scanner);
	else
		result = 0;

	td_mutex_lock_or_die(&queue->mutex);

	if (collect_stats)
	{
		t2 = td_timestamp();
		td_mutex_lock_or_die(engine->stats_lock);
		engine->stats.scan_time += t2 - t1;
		td_mutex_unlock_or_die(engine->stats_lock);
	}

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
		td_touch_file(engine, dir);
		return 0;
	}
}

static void
touch_outputs(td_engine *engine, td_node *node)
{
	int i, count;
	for (i = 0, count = node->output_count; i < count; ++i)
		td_touch_file(engine, node->outputs[i]);
}

static void
delete_outputs(td_node *node)
{
	int i, count;
	for (i = 0, count = node->output_count; i < count; ++i)
		remove(node->outputs[i]->path);
}

static int
run_job(td_job_queue *queue, td_node *node, int job_id)
{
	double t1, mkdir_time, cmd_time;
	td_engine *engine = queue->engine;
	int i, count, result, was_signalled = 0;
	const char *command = node->action;

	if (!command || '\0' == command[0])
		return 0;

	++queue->jobs_run;
	pthread_mutex_unlock(&queue->mutex);

	t1 = td_timestamp();
	/* ensure directories for output files exist */
	for (i = 0, count = node->output_count; i < count; ++i)
	{
		td_file *dir = td_parent_dir(engine, node->outputs[i]);

		if (!dir)
			continue;

		if (0 != (result = ensure_dir_exists(engine, dir)))
			goto leave;
	}
	mkdir_time = td_timestamp() - t1;

	t1 = td_timestamp();

	/* If the outputs of this node can't be overwritten; delete them now */
	if (0 == (TD_NODE_OVERWRITE & node->flags))
	{
		delete_outputs(node);
		touch_outputs(engine, node);
	}

	if (!engine->settings.dry_run)
	{
		result = td_exec(
				command,
				node->env_count,
				node->env,
				&was_signalled,
				job_id,
				td_verbosity_check(engine, 2),
				td_verbosity_check(engine, 1) ? node->annotation : NULL);
	}
	else
		result = 0;

	cmd_time = td_timestamp() - t1;

	if (0 != result)
	{
		td_mutex_lock_or_die(&queue->mutex);

		/* Maintain a fail count so we can track why we stopped building if
		 * we're stopping after the first error. Otherwise it might appear as
		 * we succeeded. */
		++queue->fail_count;

		/* If the command failed or was signalled (e.g. Ctrl+C), abort the build */
		if (was_signalled)
			queue->siginfo.flag = -1;
		else if (!engine->settings.continue_on_error)
			queue->siginfo.flag = 1;

		td_mutex_unlock_or_die(&queue->mutex);
	}

	/* If the build failed, and the node isn't set to keep all its output files
	 * in all possible cases (precious), then delete all output files as we
	 * can't assume anything about their state. */
	if (0 != result && 0 == (TD_NODE_PRECIOUS & node->flags))
		delete_outputs(node);

	/* Mark all output files as dirty regardless of whether the build succeeded
	 * or not. If it succeeded, we must assume the build overwrote them.
	 * Otherwise, it's likely we've deleted them. In any case, touching them
	 * again isn't going to hurt anything.*/
	touch_outputs(engine, node);

	/* Update engine stats. */
	if (td_debug_check(engine, TD_DEBUG_STATS))
	{
		td_mutex_lock_or_die(engine->stats_lock);
		engine->stats.mkdir_time += mkdir_time;
		engine->stats.build_time += cmd_time;
		td_mutex_unlock_or_die(engine->stats_lock);
	}

leave:
	td_mutex_lock_or_die(&queue->mutex);
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
update_input_signature(td_job_queue *queue, td_node *node)
{
	static unsigned char zero_byte = 0;
	td_engine *engine = queue->engine;
	FILE* sign_debug_file = (FILE*) engine->sign_debug_file;
	int i, count;
	MD5_CTX context;

	td_mutex_unlock_or_die(&queue->mutex);

	MD5_Init(&context);

	if (sign_debug_file)
		fprintf(sign_debug_file, "begin signing \"%s\"\n", node->annotation);

	for (i = 0, count = node->input_count; i < count; ++i)
	{
		td_file *input_file = node->inputs[i];
		td_digest *digest = td_get_signature(engine, input_file);
		MD5_Update(&context, digest->data, sizeof(digest->data));

		if (sign_debug_file)
		{
			char buffer[33];
			td_digest_to_string(digest, buffer);
			fprintf(sign_debug_file, "input[%d] = %s (\"%s\")\n", i, buffer, input_file->path);
		}
	}

	/* add a separator between the inputs and implicit deps */
	MD5_Update(&context, &zero_byte, 1);

	/* We technically invalidate the threading rules here and read the idep array.
	 *
	 * This is OK for the following reasons:
	 * - The implicit dependencies for the node have already been scanned.
	 * - They will never be scanned again (the DAG ensures this)
	 * - The memory view of this array is already guaranteed consistent as we just released the lock.
	 */
	for (i = 0, count = node->job.idep_count; i < count; ++i)
	{
		td_file *dep = node->job.ideps[i];
		td_digest *digest = td_get_signature(engine, dep);
		MD5_Update(&context, digest->data, sizeof(digest->data));

		if (sign_debug_file)
		{
			char buffer[33];
			td_digest_to_string(digest, buffer);
			fprintf(sign_debug_file, "implicit_input[%d] = %s (\"%s\")\n", i, buffer, dep->path);
		}
	}

	/* Grab the queue lock again before we publish the input signature */
	td_mutex_lock_or_die(&queue->mutex);

	MD5_Final(node->job.input_signature.data, &context);

	if (sign_debug_file)
	{
		char buffer[33];
		td_digest_to_string(&node->job.input_signature, buffer);
		fprintf(sign_debug_file, "resulting input signature = %s\n\n", buffer);
	}
}

static int
is_up_to_date(td_job_queue *queue, td_node *node)
{
	double t1 = 0.0, t2 = 0.0;
	int collect_stats;
	int i, count;
	const td_digest *prev_signature = NULL;
	td_engine *engine = queue->engine;
	const td_ancestor_data *ancestor;
	int up_to_date = 0;

	collect_stats = td_debug_check(engine, TD_DEBUG_STATS);

	if (collect_stats)
		t1 = td_timestamp();

	/* We can safely drop the build queue lock in here as no job state is accessed. */
	td_mutex_unlock_or_die(&queue->mutex);

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
	if (collect_stats)
	{
		t2 = td_timestamp();
		td_mutex_lock_or_die(engine->lock);
		engine->stats.up2date_check_time += t2 - t1;
		td_mutex_unlock_or_die(engine->lock);
	}

	td_mutex_lock_or_die(&queue->mutex);
	return up_to_date;
}

static void
advance_job(td_job_queue *queue, td_node *node, int job_id)
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
				update_input_signature(queue, node);

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
			if (0 != run_job(queue, node, job_id))
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

typedef struct
{
	td_job_queue *queue;
	int job_id;
} thread_start_arg;

static void *
build_worker(void *arg_)
{
	thread_start_arg *arg = (thread_start_arg *) arg_;

	td_job_queue * const queue = arg->queue;
	const int job_id = arg->job_id;

	pthread_mutex_lock(&queue->mutex);

	++queue->thread_count;

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

		advance_job(queue, node, job_id);

		if (is_completed(node) && is_root(node))
			queue->siginfo.flag = 1;
	}

	--queue->thread_count;
	pthread_mutex_unlock(&queue->mutex);
	pthread_cond_broadcast(&queue->work_avail);

	return NULL;
}

#define TD_MAX_THREADS (32)

static thread_start_arg thread_arg[TD_MAX_THREADS];
static pthread_t threads[TD_MAX_THREADS];

td_build_result
td_build(td_engine *engine, td_node *node, int *jobs_run)
{
	int i;
	int thread_count;
	td_job_queue queue;

	if (engine->stats.build_called)
		td_croak("build() called more than once on same engine");

	if (0 != td_init_exec())
		td_croak("couldn't initialize command execution");

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

		thread_arg[i].job_id = i + 2;
		thread_arg[i].queue = &queue;

		rc = pthread_create(&threads[i], NULL, build_worker, &thread_arg[i]);
		if (0 != rc)
			td_croak("couldn't start thread %d: %s", i, strerror(rc));
	}

	pthread_mutex_lock(&queue.mutex);

	node->job.flags |= TD_JOBF_ROOT;
	enqueue(&queue, node);

	pthread_mutex_unlock(&queue.mutex);
	pthread_cond_broadcast(&queue.work_avail);

	{
		thread_start_arg main_arg;
		main_arg.job_id = 1;
		main_arg.queue = &queue;

		build_worker(&main_arg);
	}

	if (td_verbosity_check(engine, 2) && -1 == queue.siginfo.flag)
		printf("*** aborted on signal %s\n", queue.siginfo.reason);

	for (i = thread_count - 2; i >= 0; --i)
	{
		void *res;
	   	int rc = pthread_join(threads[i], &res);
		if (0 != rc)
			td_croak("couldn't join thread %d: %s", i, strerror(rc));
	}

	if (0 != queue.thread_count)
		td_croak("threads are still running");

	/* there is a tiny race condition here if a user presses Ctrl-C just
	 * before the write to the static pointer occurs, but that's in nanosecond
	 * land */
	td_remove_sighandler();

	free(queue.array);
	pthread_cond_destroy(&queue.work_avail);
	pthread_mutex_destroy(&queue.mutex);

	*jobs_run = queue.jobs_run;

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

