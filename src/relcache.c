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

#include "relcache.h"
#include "engine.h"
#include "util.h"
#include "portable.h"
#include "files.h"

#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define TD_RELCACHE_FILE ".tundra-relcache"

enum {
	TD_RELCACHE_TTL_DAYS = 7,
	TD_RELCACHE_TTL_SECS = 60 * 60 * 24 * TD_RELCACHE_TTL_DAYS,
};

static const uint32_t td_relcache_magic =
	0xffed0000u + (sizeof(void*) << 8) + (sizeof(long) << 4) + sizeof(time_t);

/*
 * Frozen file format for relation cache entries.
 *
 * This format is platform-specific and the resulting files cannot be moved
 * between platforms. Even if the format would allow moving it wouldn't make
 * sense anyway as it contains platform-specific paths. If multiple Tundra
 * builds want to access the same build directory, we should salt the filename
 * instead so each build configuration gets a unique file.
 *
 * - header (td_frozen_rel_header)
 *
 * - string block (header.string_block_size bytes)
 *
 *   This block contains null-terminated strings, back to back that are indexed
 *   from relation entries. Becaure they are null-terminated, the entire block
 *   can be made resident and pointers set up straight into the block.
 *
 * - relation block (header.relation_count * sizeof(unsigned int) bytes)
 *
 *   This block contains string block indices for children; e.g. if node 0 has
 *   to children, 80 and 90 and node 1 has one child, 400, the first three
 *   indices of this block will be 80, 90, 400. Later when reading the nodes
 *   each node encodes an index and range into this block to indicate what
 *   files are related. The purpose of this whole affair is to avoid having
 *   variable length reads for each node. We want to do as few read calls as
 *   possible.
 *
 * - node block (header.cell_count)
 *
 */

typedef struct td_frozen_rel_header {
	uint32_t magic;
	uint32_t string_block_size;
	uint32_t relation_count;
	uint32_t node_count;
} td_frozen_rel_header;

typedef struct td_frozen_relation {
	uint32_t string_index;
	uint32_t salt;
	time_t access_time;
	uint32_t first_relation_offset;
	uint32_t relation_count;
	td_digest signature;
} td_frozen_relation;

typedef struct td_frozen_reldata {
	td_frozen_rel_header header;
	char *string_block;
	uint32_t *index_block;
	td_frozen_relation *relations;
} td_frozen_reldata;

/* Caches a relation between a file and set of other files (such as set of
 * included files) */
typedef struct td_relcell
{
	/* source file */
	struct td_file *file;

	/* a salt value to make this relation unique */
	uint32_t salt;

	/* the related files */
	int count;
	struct td_file **files;

	/* a timestamp when this node was last touched, for gc */
	time_t timestamp;

	uint32_t child_list_start;

	/* the signature of the file when this relation was cached */
	td_digest signature;

	/* for hash table linked list maintenance */
	struct td_relcell *bucket_next;
} td_relcell;

td_file **
td_engine_get_relations(td_engine *engine, td_file *file, unsigned int salt, int *count_out)
{
	td_relcell *chain;
	int bucket;
	int count_result = 0;
	td_file **result = NULL;
	unsigned int hash;

	td_mutex_lock_or_die(engine->lock);

	hash = file->hash ^ salt;
	bucket = hash % engine->relhash_size;
	assert(bucket >= 0 && bucket < engine->relhash_size);
	chain = engine->relhash[bucket];

	while (chain)
	{
		if (salt == chain->salt && file == chain->file)
		{
			const td_digest *signature = td_get_signature(engine, chain->file);

			/* Check that the cached file signature still is valid. This
			 * prevents using stale cached information for generated header
			 * files. */
			if (0 == memcmp(&chain->signature, signature, sizeof(td_digest)))
			{
				count_result = chain->count;
				result = chain->files;
			}

			break;
		}
		chain = chain->bucket_next;
	}

	td_mutex_unlock_or_die(engine->lock);
	*count_out = count_result;
	return result;
}

static void
populate_relcell(
		td_engine *engine,
		td_relcell* cell,
		td_file *file,
		unsigned int salt,
		int count,
		td_file **files,
		const td_digest *sig)
{
	size_t memsize = sizeof(td_file*) * count;
	cell->file = file;
	cell->salt = salt;
	cell->count = count;
	cell->files = (td_file **) td_page_alloc(&engine->alloc, memsize);
	cell->timestamp = engine->start_time;

	memcpy(&cell->signature, sig, sizeof(td_digest));

	memcpy(&cell->files[0], files, memsize);
}

void
set_relations(td_engine *engine, td_file *file, unsigned int salt, int count, td_file **files, const td_digest* digest)
{
	unsigned int hash;
	td_relcell *chain;
	int bucket;

	hash = file->hash ^ salt;
	bucket = hash % engine->relhash_size;
	chain = engine->relhash[bucket];

	while (chain)
	{
		if (salt == chain->salt && file == chain->file)
		{
			populate_relcell(engine, chain, file, salt, count, files, digest);
			return;
		}

		chain = chain->bucket_next;
	}

	if (td_debug_check(engine, TD_DEBUG_STATS))
	{
		td_mutex_lock_or_die(engine->stats_lock);
		++engine->stats.relation_count;
		td_mutex_unlock_or_die(engine->stats_lock);
	}

	chain = (td_relcell*) td_page_alloc(&engine->alloc, sizeof(td_relcell));
	populate_relcell(engine, chain, file, salt, count, files, digest);
	chain->bucket_next = engine->relhash[bucket];
	engine->relhash[bucket] = chain;
}

void
td_engine_set_relations(td_engine *engine, td_file *file, unsigned int salt, int count, td_file **files)
{
	td_mutex_lock_or_die(engine->lock);
	set_relations(engine, file, salt, count, files, td_get_signature(engine, file));
	td_mutex_unlock_or_die(engine->lock);
}

static void
persist_filename(FILE *rcf, td_file *file, uint32_t *cursor)
{
	if (~0u == file->frozen_relstring_index)
	{
		uint32_t write_len = (uint32_t) (file->path_len + 1); /* also write the nul byte */
		file->frozen_relstring_index = *cursor;
		fwrite(file->path, 1, write_len, rcf);
		(*cursor) += write_len;
	}
}

static void
write_relcache_strings(td_engine *engine, FILE* f, td_frozen_rel_header *header)
{
	/* Write the string block. While visting all relcache nodes, discard those
	 * that are too old to save. Also compute how big the block of relations
	 * indices needs to be (the sum of all child counts.) */

	int i, count;
	uint32_t child_list_offset = 0;
	uint32_t running_offset = 0;
	uint32_t node_count = 0;

	for (i = 0, count = engine->relhash_size; i < count; ++i)
	{
		td_relcell *chain = engine->relhash[i];
		while (chain)
		{
			if (chain->timestamp + TD_RELCACHE_TTL_SECS > engine->start_time)
			{
				++node_count;
				persist_filename(f, chain->file, &running_offset);
				chain->child_list_start = child_list_offset;
				child_list_offset += (uint32_t) chain->count;
			}
			chain = chain->bucket_next;
		}
	}

	header->string_block_size = running_offset;
	assert(ftell(f) == (long)(running_offset + sizeof(td_frozen_rel_header)));
	header->node_count = node_count;
	header->relation_count = child_list_offset;
}

static void
write_relcache_indices(td_engine *engine, FILE* f)
{
	int i, count;
	uint32_t string_indices[1024];

	for (i = 0, count = engine->relhash_size; i < count; ++i)
	{
		const td_relcell *chain = engine->relhash[i];

		while (chain)
		{
			int ki, ke;

			if (chain->timestamp + TD_RELCACHE_TTL_SECS <= engine->start_time)
				continue;

			if (chain->count > sizeof(string_indices) / sizeof(string_indices[0]))
				td_croak("too many relations in file %s", chain->file->path);

			for (ki = 0, ke = chain->count; ki < ke; ++ki)
			{
				string_indices[ki] = chain->files[ki]->frozen_relstring_index;
				assert(~0u != string_indices[ki]);
			}

			fwrite(string_indices, sizeof(string_indices[0]), chain->count, f);

			chain = chain->bucket_next;
		}
	}
}

static void
write_relcache_nodes(td_engine *engine, FILE* f)
{
	int i, count;

	for (i = 0, count = engine->relhash_size; i < count; ++i)
	{
		const td_relcell *chain = engine->relhash[i];

		while (chain)
		{
			td_frozen_relation data;

			if (chain->timestamp + TD_RELCACHE_TTL_SECS <= engine->start_time)
				continue;

			data.string_index = chain->file->frozen_relstring_index;
			data.salt =	chain->salt;
			data.access_time = chain->timestamp;
			data.first_relation_offset = chain->child_list_start;
			data.relation_count = chain->count;
			memcpy(&data.signature, &chain->signature, sizeof(td_digest));

			fwrite(&data, 1, sizeof(data), f);

			chain = chain->bucket_next;
		}
	}
}

void
td_save_relcache(td_engine *engine)
{
	FILE* f;
	double t1;
	td_frozen_rel_header header;

	t1 = td_timestamp();

	if (NULL == (f = fopen(TD_RELCACHE_FILE, "wb")))
		td_croak("couldn't open %s", TD_RELCACHE_FILE);

	/* write placeholder header */
	memset(&header, 0, sizeof(header));
	fwrite(&header, 1, sizeof(header), f);

	write_relcache_strings(engine, f, &header);
	write_relcache_indices(engine, f);
	write_relcache_nodes(engine, f);

	header.magic = td_relcache_magic;

	rewind(f);
	fwrite(&header, 1, sizeof(header), f);

	fclose(f);
	engine->stats.relcache_save = td_timestamp() - t1;
}

static void
free_data(td_frozen_reldata *data)
{
	free(data->string_block);
	free(data->index_block);
	free(data->relations);
	free(data);
}

static int
read_fully(void *dest, FILE *f, uint32_t size)
{
	return size == fread(dest, 1, size, f);
}

static void
install_relations(td_engine *engine, td_frozen_reldata *data)
{
	uint32_t i, count, total_files = 0;
	td_file* files[1024];

	for (i = 0, count = data->header.node_count; i < count; ++i)
	{
		uint32_t fi, ki, ke;

		const td_frozen_relation *rel = &data->relations[i];
		const char *path = data->string_block + rel->string_index;

		td_file *f = td_engine_get_file(engine, path, TD_BORROW_STRING);

		fi = 0;
		ki = rel->first_relation_offset;
		ke = ki + rel->relation_count;
		total_files += rel->relation_count;
		for (; ki < ke; ++ki, ++fi)
		{
			const uint32_t index = data->index_block[ki];
			const char *fpath = data->string_block + index;
			files[fi] = td_engine_get_file(engine, fpath, TD_BORROW_STRING);
		}

		/* Insert the relation info with the old signature of the file. This
		 * way it's OK to include stale information because if the file is used
		 * it will be signed before the cache is consulted and a digest change
		 * will disregard this node. */
		set_relations(engine, f, rel->salt, (int) rel->relation_count, files, &rel->signature);
	}

	if (td_verbosity_check(engine, 2))
	{
		printf("installed %u relations from cache (%u files preloaded)\n", data->header.node_count, total_files);
	}
}

void
td_load_relcache(td_engine *engine)
{
	double t1;
	td_frozen_reldata *data = NULL;
	FILE *f;

	t1 = td_timestamp();

	if (NULL == (f = fopen(TD_RELCACHE_FILE, "rb")))
		goto leave;

	data = calloc(1, sizeof(td_frozen_reldata));

	if (1 != fread(&data->header, sizeof(data->header), 1, f))
	{
		fprintf(stderr, "warning: bad relation cache file\n");
		goto leave;
	}

	if (td_relcache_magic != data->header.magic)
	{
		fprintf(stderr, "warning: bad relation cache magic\n");
		goto leave;
	}

	if (NULL == (data->string_block = malloc(data->header.string_block_size)))
		goto leave;

	if (NULL == (data->index_block = malloc(sizeof(uint32_t) * data->header.relation_count)))
		goto leave;

	if (NULL == (data->relations = malloc(sizeof(td_frozen_relation) * data->header.node_count)))
		goto leave;

	if (!read_fully(data->string_block, f, data->header.string_block_size))
	{
		fprintf(stderr, "warning: couldn't read string block\n");
		goto leave;
	}

	if (!read_fully(data->index_block, f, data->header.relation_count * sizeof(uint32_t)))
	{
		fprintf(stderr, "warning: couldn't read relation block\n");
		goto leave;
	}

	if (!read_fully(data->relations, f, data->header.node_count * sizeof(td_frozen_relation)))
	{
		fprintf(stderr, "warning: couldn't read relation block\n");
		goto leave;
	}

	/* hook up cached relations */
	install_relations(engine, data);

	engine->relcache_data = data;

leave:
	if (data && !engine->relcache_data)
		free_data(data);

	if (f)
		fclose(f);

	engine->stats.relcache_load = td_timestamp() - t1;
}

void
td_relcache_cleanup(struct td_engine *engine)
{
	if (NULL != engine->relcache_data)
	{
		free_data(engine->relcache_data);
		engine->relcache_data = NULL;
	}
}
