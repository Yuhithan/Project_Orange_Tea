#pragma once

#include <stddef.h>
#include <stdint.h>

#define OR_PAGE_SIZE 4096u

/* Early allocator: a reserved, page-aligned kernel pool until a Multiboot memory-map allocator exists. */
void memory_init(void);
void *page_alloc(void);
int page_free(void *page);
size_t page_free_count(void);
int memory_range_valid(const void *address, size_t length);
