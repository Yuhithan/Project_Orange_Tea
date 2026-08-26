#pragma once

#include <stdint.h>

enum { SYS_OPEN = 1, SYS_CLOSE, SYS_READ, SYS_WRITE, SYS_SEEK, SYS_EXIT, SYS_WAIT };
/* Dispatchable kernel syscall surface. A ring-3 instruction entry is intentionally not exposed until user mode exists. */
long syscall_dispatch(uint64_t number, uint64_t arg0, uint64_t arg1, uint64_t arg2);
