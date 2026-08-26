#include "syscall.h"
#include "storage.h"
#include "process.h"

long syscall_dispatch(uint64_t number, uint64_t arg0, uint64_t arg1, uint64_t arg2)
{
    switch (number) {
    case SYS_OPEN: return storage_open((const char *)(uintptr_t)arg0, (int)arg1);
    case SYS_CLOSE: return storage_close((int)arg0);
    case SYS_READ: return storage_read((int)arg0, (void *)(uintptr_t)arg1, (size_t)arg2);
    case SYS_WRITE: return storage_write((int)arg0, (const void *)(uintptr_t)arg1, (size_t)arg2);
    case SYS_SEEK: return storage_seek((int)arg0, (size_t)arg1);
    case SYS_EXIT: return process_exit((uint32_t)arg0, (int)arg1);
    case SYS_WAIT: return process_wait((uint32_t)arg0, (uint32_t)arg1, (int *)(uintptr_t)arg2);
    default: return STORAGE_ERR_INVAL;
    }
}
