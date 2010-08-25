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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define TD_ANCESTOR_FILE ".tundra-ancestors"

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
	engine->ancestor_used = (td_node **)calloc(sizeof(td_node *), count);
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

	if (node->job.flags & TD_JOBF_ANCESTOR_UPDATED)
		return;

	node->job.flags |= TD_JOBF_ANCESTOR_UPDATED;

	/* If this node had an ancestor record, flag it as visited. This way it
	 * will be disregarded when writing out all the other ancestors that
	 * weren't used this build session. */
	{
		const td_ancestor_data *data;
		if (NULL != (data = node->ancestor_data))
		{
			int index = (int) (data - engine->ancestors);
			assert(index < engine->ancestor_count);
			assert(0 == visited[index]);
			visited[index] = 1;
		}
	}

	output_index = *cursor;
	*cursor += 1;
	memset(&output[output_index], 0, sizeof(td_ancestor_data));

	memcpy(&output[output_index].guid, &node->guid, sizeof(td_digest));
	memcpy(&output[output_index].input_signature, &node->job.input_signature, sizeof(td_digest));
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

