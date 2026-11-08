#pragma once

#include "storage.h"

#define FAT32_MAX_PATH 512
#define FAT32_MAX_NAME 256

int fat32_mount(const struct block_device *device);
int fat32_format(const struct block_device *device);
int fat32_is_mounted(void);
int fat32_unmount(void);
int fat32_sync(void);
int fat32_get_stats(struct storage_stats *stats);
int fat32_fsck(int repair, int *errors);
int fat32_create_file(const char *path, const void *buffer, size_t length);
int fat32_create_entry(const char *path, char type, const char *content);
int fat32_find_entry(const char *path);
int fat32_get_entry_info(const char *path, struct storage_entry_info *info);
int fat32_unlink(const char *path);
int fat32_rmdir(const char *path);
int fat32_remove_tree(const char *path);
int fat32_get_entry_count(void);
int fat32_get_entry_info_at(int index, struct storage_entry_info *info);
const char *fat32_get_entry_name(int index);
const char *fat32_get_entry_path(int index);
char fat32_get_entry_type(int index);
const char *fat32_get_entry_content(int index);
int fat32_open(const char *path, int create);
int fat32_close(int fd);
int fat32_read(int fd, void *buffer, size_t length);
int fat32_write(int fd, const void *buffer, size_t length);
int fat32_seek(int fd, size_t offset);
int fat32_mkdir(const char *path);
int fat32_write_file(const char *path, const void *buffer, size_t length, int append);
int fat32_read_file(const char *path, void *buffer, size_t capacity, size_t *length);
int fat32_rename(const char *source, const char *destination);
int fat32_copy(const char *source, const char *destination);
