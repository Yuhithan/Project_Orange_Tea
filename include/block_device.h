#pragma once

#include <stdint.h>

#define BLOCK_SECTOR_SIZE 512u

struct block_device {
    void *context;
    uint64_t sector_count;
    int (*read_sector)(void *context, uint64_t sector, void *buffer);
    int (*write_sector)(void *context, uint64_t sector, const void *buffer);
};
