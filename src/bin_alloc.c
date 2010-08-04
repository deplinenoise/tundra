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

#include "bin_alloc.h"
#include "util.h"

#include <stdlib.h>
#include <assert.h>
#include <string.h>

static const int bin_sizes[BIN_COUNT] = { 16, 32, 40, 48, 56, 64, 80, 88, 128, 160, 256 };

static void* prep_page(void *page, int slot_size)
{
	int i;
	unsigned char *p = (unsigned char *) page;
	unsigned char *prev = NULL;

	for (i = 0; (i + slot_size) < BIN_PAGE_SIZE; i += slot_size)
	{
		unsigned char *here = &p[i];
		*((unsigned char **) here) = prev;
		prev = here;
	}

	return prev;
}

static void *size_bin_alloc(td_size_bin *bin)
{
	void **slot;

	if (NULL == (slot = bin->free_list))
	{
		int i;
		for (i = 0; i < BIN_MAX_PAGES; ++i)
		{
			if (!bin->pages[i])
			{
				bin->pages[i] = malloc(BIN_PAGE_SIZE);
				slot = prep_page(bin->pages[i], bin->size);
				break;
			}
		}

		if (BIN_MAX_PAGES == i)
			td_croak("out of memory; bin size %d exhausted", bin->size);
	}

	bin->free_list = *slot;
	return slot;
}

static void size_bin_free(td_size_bin *bin, void *ptr)
{
	void ** slot = (void **) ptr;
	*slot = bin->free_list;
	bin->free_list = slot;
}


static void size_bin_init(td_size_bin *bin, int size)
{
	assert(size != 0 && size <= BIN_ALLOC_SIZE_MAX);
	bin->size = size;
	bin->free_list = NULL;
	memset(bin->pages, 0, sizeof(bin->pages));
}

static void size_bin_cleanup(td_size_bin *bin)
{
	int i;
	for (i = 0; i < BIN_MAX_PAGES; ++i)
	{
		void *ptr;
		if (NULL != (ptr = bin->pages[i]))
			free(ptr);
	}
}
void td_bin_allocator_init(td_bin_allocator *alloc)
{
	int i;

	for (i = 0; i < BIN_COUNT; ++i)
		size_bin_init(&alloc->bins[i], bin_sizes[i]);

	for (i = 1; i <= BIN_ALLOC_SIZE_MAX; ++i)
	{
		int s;
		for (s = 0; s < BIN_COUNT; ++s)
		{
			if (i <= bin_sizes[s])
			{
				alloc->bin_index[i-1] = (unsigned char) s;
				break;
			}
		}

		assert(s != BIN_COUNT);
	}
}

void td_bin_allocator_cleanup(td_bin_allocator *alloc)
{
	int i;
	for (i = 0; i < BIN_COUNT; ++i)
		size_bin_cleanup(&alloc->bins[i]);
}

void *td_bin_alloc(td_bin_allocator *alloc, size_t size)
{
	unsigned char bini;
	td_size_bin *bin;

	assert(size && size <= BIN_ALLOC_SIZE_MAX);
	bini = alloc->bin_index[size-1];
	bin = &alloc->bins[bini];
	return size_bin_alloc(bin);
}

void td_bin_free(td_bin_allocator *alloc, void* ptr, size_t size)
{
	unsigned char bini;
	td_size_bin *bin;

	assert(size && size <= BIN_ALLOC_SIZE_MAX);
	bini = alloc->bin_index[size-1];
	bin = &alloc->bins[bini];
	size_bin_free(bin, ptr);
}

void *td_lua_alloc(void *ud, void *old_ptr, size_t osize, size_t nsize)
{
	void *result = NULL;
	td_bin_allocator *allocator = (td_bin_allocator *) ud;

	if (nsize > 0)
	{
		if (nsize <= BIN_ALLOC_SIZE_MAX)
			result = td_bin_alloc(allocator, nsize);
		else
			result = malloc(nsize);
	}

	if (old_ptr)
	{
		assert(osize);

		if (result)
			memcpy(result, old_ptr, osize > nsize ? nsize : osize);

		if (osize <= BIN_ALLOC_SIZE_MAX)
			td_bin_free(allocator, old_ptr, osize);
		else
			free(old_ptr);
	}

	return result;
}
