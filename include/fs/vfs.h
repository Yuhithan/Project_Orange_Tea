#pragma once

#include "storage.h"

void vfs_init(void);
int vfs_attach_block_device(const struct block_device *device);
int vfs_resolve_path(const char *cwd, const char *path, char *out, size_t capacity);
int vfs_open(const char *path, int create);
int vfs_close(int fd);
int vfs_read(int fd, void *buffer, size_t length);
int vfs_write(int fd, const void *buffer, size_t length);
int vfs_seek(int fd, size_t offset);
int vfs_create(const char *path, const void *buffer, size_t length);
int vfs_write_file(const char *path, const void *buffer, size_t length, int append);
int vfs_mkdir(const char *path);
int vfs_unlink(const char *path);
int vfs_rmdir(const char *path);
int vfs_remove_tree(const char *path);
int vfs_rename(const char *source, const char *destination);
int vfs_copy(const char *source, const char *destination);
int vfs_stat(const char *path, struct storage_entry_info *info);
int vfs_entry_count(void);
int vfs_readdir(int index, struct storage_entry_info *info);
int vfs_get_stats(struct storage_stats *stats);
int vfs_get_device_info(struct storage_device_info *info);
int vfs_mount(void);
int vfs_unmount(void);
int vfs_format(void);
int vfs_sync(void);
int vfs_fsck(int repair, int *errors);