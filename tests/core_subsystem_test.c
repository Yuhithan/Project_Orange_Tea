#include <assert.h>
#include <string.h>
#include "memory.h"
#include "process.h"
#include "storage.h"
#include "syscall.h"

int main(void)
{
    memory_init();
    size_t free_before = page_free_count();
    void *page = page_alloc();
    assert(page != 0 && memory_range_valid(page, 32));
    assert(page_free_count() == free_before - 1);
    assert(page_free(page) == 0 && page_free(page) == -1);

    storage_init();
    int fd = syscall_dispatch(SYS_OPEN, (uint64_t)(uintptr_t)"/tmp/note", 1, 0);
    assert(fd >= 0);
    assert(syscall_dispatch(SYS_WRITE, fd, (uint64_t)(uintptr_t)"fire", 4) == 4);
    assert(syscall_dispatch(SYS_SEEK, fd, 0, 0) == 0);
    char text[5] = {0};
    assert(syscall_dispatch(SYS_READ, fd, (uint64_t)(uintptr_t)text, 4) == 4);
    assert(strcmp(text, "fire") == 0);
    assert(syscall_dispatch(SYS_CLOSE, fd, 0, 0) == 0);
    assert(storage_open("/tmp", 0) == STORAGE_ERR_ISDIR);

    process_init();
    process_t *child = process_create(1);
    assert(child != 0 && process_exit(child->pid, 42) == 0);
    int status = 0;
    assert(process_wait(1, child->pid, &status) == 0 && status == 42);
    return 0;
}
