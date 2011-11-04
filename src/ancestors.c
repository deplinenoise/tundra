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


#include "ancestors.h"
#include "engine.h"
#include "md5.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define TD_ANCESTOR_FILE ".tundra-ancestors"

static void
md5_string(MD5_CTX *context, const char *string)
{
	static unsigned char zero_byte = 0;

	if (string)
		MD5_Update(context, (unsigned char*) string, (int) strlen(string)+1);
	else
		MD5_Update(context, &zero_byte, 1);
}

static void
compute_node_guid(td_engine *engine, td_node *node)
{
	MD5_CTX context;
	MD5_Init(&context);
	md5_string(&context, node->action);
	md5_string(&context, node->annotation);
	md5_string(&context, node->salt);
	MD5_Final(node->guid.data, &context);

	if (td_debug_check(engine, TD_DEBUG_NODES))
	{
		char guidstr[33];
		td_digest_to_string(&node->guid, guidstr);
		printf("%s with guid %s\n", node->annotation, guidstr);
	}
}

void
td_load_ancestors(td_engine *engine)
{
	FILE* f;
	int i, count;
	long file_size;
	size_t read_count;

	if (NULL == (f = fopen(TD_ANCESTOR_FILE, "rb")))
	{
		if (td_debug_check(engine, TD_DEBUG_ANCESTORS))
			printf("couldn't open %s; no ancestor information present\n", TD_ANCESTOR_FILE);
		return;
	}

	fseek(f, 0, SEEK_END);
	file_size = ftell(f);
	rewind(f);

	if (file_size % sizeof(td_ancestor_data) != 0)
		td_croak("illegal ancestor file: %d not a multiple of %d bytes",
				(int) file_size, sizeof(td_ancestor_data));

	engine->ancestor_count = count = (int) (file_size / sizeof(td_ancestor_data));
	engine->ancestors = malloc(file_size);
	engine->ancestor_used = calloc(sizeof(td_node *), count);
	read_count = fread(engine->ancestors, sizeof(td_ancestor_data), count, f);

	if (td_debug_check(engine, TD_DEBUG_ANCESTORS))
		printf("read %d ancestors\n", count);

	if (read_count != (size_t) count)
		td_croak("only read %d items, wanted %d", read_count, count);

	for (i = 1; i < count; ++i)
	{
		int cmp = td_compare_ancestors(&engine->ancestors[i-1], &engine->ancestors[i]);
		if (cmp == 0)
			td_croak("bad ancestor file; duplicate item (%d/%d)", i, count);
		if (cmp > 0)
			td_croak("bad ancestor file; bad sort order on item (%d/%d)", i, count);
	}

	if (td_debug_check(engine, TD_DEBUG_ANCESTORS))
	{
		printf("full ancestor dump on load:\n");
		for (i = 0; i < count; ++i)
		{
			char guid[33], sig[33];
			td_digest_to_string(&engine->ancestors[i].guid, guid);
			td_digest_to_string(&engine->ancestors[i].input_signature, sig);
			printf("%s %s %ld %d\n", guid, sig, (long) engine->ancestors[i].access_time, engine->ancestors[i].job_result);
		}
	}

	fclose(f);
}

static void
update_ancestors(
		td_engine *engine,
		td_node *node,
		time_t now,
		int *cursor,
		td_ancestor_data *output,
		unsigned char *visited)
{
	int i, count, output_index;
	const td_ancestor_data *ancestor_data;

	if (node->job.flags & TD_JOBF_ANCESTOR_UPDATED)
		return;

	node->job.flags |= TD_JOBF_ANCESTOR_UPDATED;

	/* If this node had an ancestor record, flag it as visited. This way it
	 * will be disregarded when writing out all the other ancestors that
	 * weren't used this build session. */
	if (NULL != (ancestor_data = node->ancestor_data))
	{
		int index = (int) (ancestor_data - engine->ancestors);
		assert(index < engine->ancestor_count);
		assert(0 == visited[index]);
		visited[index] = 1;
	}

	output_index = *cursor;
	*cursor += 1;
	memset(&output[output_index], 0, sizeof(td_ancestor_data));

	memcpy(&output[output_index].guid, &node->guid, sizeof(td_digest));

	/* Decide what input signature to save. If the job reached the scanning
	 * state, that means the input signature was updated, so save that for
	 * the next run.
	 *
	 * Otherwise if these is ancestor information for this node, save the old
	 * input signature again. If this isn't done it will trigger future
	 * sporadic rebuilds on nodes that are up-to-date but are never visited in
	 * the DAG before a build failure.
	 *
	 * Otherwise nothing is known about this node, so leaving the input
	 * signature cleared is the right thing to do.
	 */
	if (node->job.state > TD_JOB_SCANNING)
		memcpy(&output[output_index].input_signature, &node->job.input_signature, sizeof(td_digest));
	else if (ancestor_data)
		memcpy(&output[output_index].input_signature, &ancestor_data->input_signature, sizeof(td_digest));

	output[output_index].access_time = now;
	output[output_index].job_result = node->job.state;

	for (i = 0, count = node->dep_count; i < count; ++i)
		update_ancestors(engine, node->deps[i], now, cursor, output, visited);
}

enum {
	TD_ANCESTOR_TTL_DAYS = 7,
	TD_SECONDS_PER_DAY = (60 * 60 * 24),
	TD_ANCESTOR_TTL_SECS = TD_SECONDS_PER_DAY * TD_ANCESTOR_TTL_DAYS,
};

static int
ancestor_timed_out(const td_ancestor_data *data, time_t now)
{
	return data->access_time + TD_ANCESTOR_TTL_SECS < now;
}

void
td_save_ancestors(td_engine *engine, td_node *root)
{
	FILE* f;
	int i, count, max_count;
	int output_cursor, write_count;
	td_ancestor_data *output;
	unsigned char *visited;
	time_t now = time(NULL);
	const int dbg = td_debug_check(engine, TD_DEBUG_ANCESTORS);

	if (NULL == (f = fopen(TD_ANCESTOR_FILE ".tmp", "wb")))
	{
		fprintf(stderr, "warning: couldn't save ancestors\n");
		return;
	}

	max_count = engine->node_count + engine->ancestor_count;
	output = (td_ancestor_data *) malloc(sizeof(td_ancestor_data) * max_count);
	visited = (unsigned char *) calloc(engine->ancestor_count, 1);

	output_cursor = 0;
	update_ancestors(engine, root, now, &output_cursor, output, visited);

	if (dbg)
		printf("refreshed %d ancestors\n", output_cursor);

	for (i = 0, count = engine->ancestor_count; i < count; ++i)
	{
		const td_ancestor_data *a = &engine->ancestors[i];
		if (!visited[i] && !ancestor_timed_out(a, now))
			output[output_cursor++] = *a;
	}

	if (dbg)
		printf("%d ancestors to save in total\n", output_cursor);

	qsort(output, output_cursor, sizeof(td_ancestor_data), td_compare_ancestors);

	if (dbg)
	{
		printf("full ancestor dump on save:\n");
		for (i = 0; i < output_cursor; ++i)
		{
			char guid[33], sig[33];
			td_digest_to_string(&output[i].guid, guid);
			td_digest_to_string(&output[i].input_signature, sig);
			printf("%s %s %ld %d\n", guid, sig, (long) output[i].access_time, output[i].job_result);
		}
	}

	write_count = (int) fwrite(output, sizeof(td_ancestor_data), output_cursor, f);

	fclose(f);
	free(visited);
	free(output);

	if (write_count != output_cursor)
		td_croak("couldn't write %d entries; only wrote %d", output_cursor, write_count);

	if (0 != td_move_file(TD_ANCESTOR_FILE ".tmp", TD_ANCESTOR_FILE))
		td_croak("couldn't rename %s to %s", TD_ANCESTOR_FILE ".tmp", TD_ANCESTOR_FILE);
}

void
td_setup_ancestor_data(td_engine *engine, td_node *node)
{
	compute_node_guid(engine, node);

	++engine->stats.ancestor_checks;

	if (engine->ancestors)
	{
		td_ancestor_data key;
		key.guid = node->guid; /* only key field is relevant */

		node->ancestor_data = (td_ancestor_data *)
			bsearch(&key, engine->ancestors, engine->ancestor_count, sizeof(td_ancestor_data), td_compare_ancestors);

		if (node->ancestor_data)
		{
			int index = (int) (node->ancestor_data - engine->ancestors);
			td_node *other;
			if (NULL != (other = engine->ancestor_used[index]))
				td_croak("node error: nodes \"%s\" and \"%s\" share the same ancestor", node->annotation, other->annotation);
			engine->ancestor_used[index] = node;
			++engine->stats.ancestor_nodes;
		}
		else
		{
			if (td_debug_check(engine, TD_DEBUG_ANCESTORS))
			{
				char guidstr[33];
				td_digest_to_string(&node->guid, guidstr);
				printf("no ancestor for %s with guid %s\n", node->annotation, guidstr);
			}
		}
	}
	else
	{
		/* We didn't load any ancestor data, just set the ancestor to NULL.
		 * Everything will rebuild without ancestry. */
		node->ancestor_data = NULL;
	}
}

