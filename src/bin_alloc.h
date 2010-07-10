#ifndef TUNDRA_BIN_ALLOC_H
#define TUNDRA_BIN_ALLOC_H

#include <stddef.h>

enum {
	BIN_MAX_PAGES = 128,
	BIN_PAGE_SIZE = 262144,
	BIN_ALLOC_SIZE_MAX = 256,
	BIN_COUNT = 11,
};

typedef struct td_size_bin
{
	int size;
	void **free_list;
	void *pages[BIN_MAX_PAGES];
} td_size_bin;

typedef struct td_bin_allocator
{
	td_size_bin bins[BIN_COUNT];
	unsigned char bin_index[BIN_ALLOC_SIZE_MAX]; /* index with size - 1 */
} td_bin_allocator;

void td_bin_allocator_init(td_bin_allocator *alloc);

void td_bin_allocator_cleanup(td_bin_allocator *alloc);

void *td_bin_alloc(td_bin_allocator *alloc, size_t size);

void td_bin_free(td_bin_allocator *alloc, void* ptr, size_t size);

void *td_lua_alloc(void *ud, void *old_ptr, size_t osize, size_t nsize);


#endif