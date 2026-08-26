#pragma once

#include <stddef.h>

#define STORAGE_MAX_PATH 64

enum {
    STORAGE_OK = 0,
    STORAGE_ERR_INVAL = -1,
    STORAGE_ERR_NOENT = -2,
    STORAGE_ERR_EXIST = -3,
    STORAGE_ERR_NOTDIR = -4,
    STORAGE_ERR_ISDIR = -5,
    STORAGE_ERR_NOSPC = -6,
    STORAGE_ERR_BADFD = -7,
    STORAGE_ERR_NOTEMPTY = -8
};

void storage_init(void);
int storage_create_entry(const char* path, char type, const char* content);
int storage_find_entry(const char* path);
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
