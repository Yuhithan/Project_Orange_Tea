#include "memory.h"

#define EARLY_PAGE_COUNT 128
static uint8_t early_pages[EARLY_PAGE_COUNT][OR_PAGE_SIZE] __attribute__((aligned(OR_PAGE_SIZE)));
static uint8_t page_used[EARLY_PAGE_COUNT];

void memory_init(void) { for (size_t i = 0; i < EARLY_PAGE_COUNT; i++) page_used[i] = 0; }
void *page_alloc(void)
{
    for (size_t i = 0; i < EARLY_PAGE_COUNT; i++) if (!page_used[i]) {
        page_used[i] = 1;
        for (size_t byte = 0; byte < OR_PAGE_SIZE; byte++) early_pages[i][byte] = 0;
        return early_pages[i];
    }
    return 0;
}
int page_free(void *page)
{
    if (!page || ((uintptr_t)page & (OR_PAGE_SIZE - 1u))) return -1;
    for (size_t i = 0; i < EARLY_PAGE_COUNT; i++) if (page == early_pages[i]) {
        if (!page_used[i]) return -1;
        page_used[i] = 0; return 0;
    }
    return -1;
}
size_t page_free_count(void) { size_t count = 0; for (size_t i = 0; i < EARLY_PAGE_COUNT; i++) if (!page_used[i]) count++; return count; }
int memory_range_valid(const void *address, size_t length)
{
    if (!address || length == 0) return length == 0;
    uintptr_t start = (uintptr_t)address, end = start + length;
    if (end < start) return 0;
    for (size_t i = 0; i < EARLY_PAGE_COUNT; i++) {
        uintptr_t first = (uintptr_t)early_pages[i], last = first + OR_PAGE_SIZE;
        if (page_used[i] && start >= first && end <= last) return 1;
    }
    return 0;
}
