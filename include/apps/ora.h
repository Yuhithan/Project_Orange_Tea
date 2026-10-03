#pragma once

#include <stddef.h>
#include <stdint.h>

#define ORA_MAGIC 0x41524f4fu /* "ORA\0" in little endian */
#define ORA_VERSION 1u
#define ORA_ARCH_X86_64 0x003eu
#define ORA_HEADER_SIZE 52u

enum {
    ORA_OK = 0,
    ORA_ERR_ARGUMENT = -1,
    ORA_ERR_FORMAT = -2,
    ORA_ERR_RANGE = -3,
    ORA_ERR_IO = -4
};

struct __attribute__((packed)) ora_header {
    uint32_t magic;
    uint16_t version;
    uint16_t architecture;
    uint16_t header_size;
    uint16_t flags;
    uint64_t entry_point;
    uint64_t text_size;
    uint64_t data_size;
    uint64_t bss_size;
    uint64_t alignment;
};

typedef int (*ora_read_fn)(void *context, uint64_t offset, void *buffer, size_t size);
typedef int (*ora_load_fn)(void *context, uint64_t address, const void *data, size_t size);
typedef int (*ora_zero_fn)(void *context, uint64_t address, size_t size);
typedef int (*ora_enter_fn)(void *context, uint64_t entry_point);

struct ora_loader {
    void *context;
    ora_read_fn read;
    ora_load_fn load;
    ora_zero_fn zero;
    ora_enter_fn enter;
    uint64_t image_base;
    uint64_t memory_limit;
};

int ora_validate_header(const struct ora_header *header, uint64_t file_size);
int ora_load(const struct ora_loader *loader, uint64_t file_size);
