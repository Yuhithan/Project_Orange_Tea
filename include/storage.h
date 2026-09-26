#pragma once

#include <stddef.h>
#include <stdint.h>
#include "block_device.h"

#define STORAGE_MAX_PATH 64
#define STORAGE_MAX_ENTRIES 64
#define STORAGE_MAX_FILE_BLOCKS 8

#define STORAGE_SELFTEST_DEVICE (1u << 0)
#define STORAGE_SELFTEST_INFO (1u << 1)
#define STORAGE_SELFTEST_BLOCK_IO (1u << 2)
#define STORAGE_SELFTEST_MOUNT (1u << 3)
#define STORAGE_SELFTEST_FILES (1u << 4)
#define STORAGE_SELFTEST_SPACE (1u << 5)
#define STORAGE_SELFTEST_CACHE (1u << 6)
#define STORAGE_SELFTEST_FSCK (1u << 7)
#define STORAGE_SELFTEST_ERRORS (1u << 8)
#define STORAGE_SELFTEST_ALL ((1u << 9) - 1u)

enum {
    STORAGE_OK = 0,
    STORAGE_ERR_INVAL = -1,
    STORAGE_ERR_NOENT = -2,
    STORAGE_ERR_EXIST = -3,
    STORAGE_ERR_NOTDIR = -4,
    STORAGE_ERR_ISDIR = -5,
    STORAGE_ERR_NOSPC = -6,
    STORAGE_ERR_BADFD = -7,
    STORAGE_ERR_NOTEMPTY = -8,
    STORAGE_ERR_IO = -9,
    STORAGE_ERR_NOTMOUNTED = -10,
    STORAGE_ERR_CORRUPT = -11
};

struct storage_device_info {
    const char *name;
    const char *type;
    uint64_t sectors;
    uint32_t sector_size;
    int present;
    int mounted;
};

struct storage_stats {
    uint64_t total_sectors;
    uint64_t used_sectors;
    uint64_t free_sectors;
    uint32_t sector_size;
    uint32_t file_count;
    uint32_t directory_count;
};

struct storage_entry_info {
    const char *name;
    const char *path;
    char type;
    size_t size;
    uint32_t blocks_used;
};

void storage_init(void);
int storage_attach_block_device(const struct block_device *device);
int storage_read_blocks(const struct block_device *device, uint64_t block, void *buffer, size_t count);
int storage_write_blocks(const struct block_device *device, uint64_t block, const void *buffer, size_t count);
int storage_mount(void);
int storage_unmount(void);
int storage_format(void);
int storage_is_mounted(void);
int storage_get_device_info(struct storage_device_info *info);
int storage_get_stats(struct storage_stats *stats);
int storage_fsck(int repair, int *errors);
int storage_sync(void);
unsigned int storage_self_test(void);
int storage_create_file(const char *path, const void *buffer, size_t length);
int storage_create_entry(const char* path, char type, const char* content);
int storage_find_entry(const char* path);
int storage_get_entry_info(const char *path, struct storage_entry_info *info);
int storage_unlink(const char *path);
int storage_rmdir(const char *path);
int storage_remove_tree(const char *path);
int storage_remove_entry(const char* path);
int storage_get_entry_count(void);
const char* storage_get_entry_name(int index);
const char* storage_get_entry_path(int index);
char storage_get_entry_type(int index);
const char* storage_get_entry_content(int index);

/* Bounded in-memory VFS interface used by future kernel/user APIs. */
int storage_open(const char* path, int create);
int storage_close(int fd);
int storage_read(int fd, void* buffer, size_t length);
int storage_write(int fd, const void* buffer, size_t length);
int storage_seek(int fd, size_t offset);
int storage_mkdir(const char* path);
int storage_write_file(const char *path, const void *buffer, size_t length, int append);
int storage_read_file(const char *path, void *buffer, size_t capacity, size_t *length);
int storage_rename(const char *source, const char *destination);
int storage_copy(const char *source, const char *destination);
