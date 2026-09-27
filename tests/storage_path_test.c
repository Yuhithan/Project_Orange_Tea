#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "storage.h"
#include "vfs.h"

static unsigned char disk[128 * BLOCK_SECTOR_SIZE];
static int fail_writes;

static int read_sector(void *context, uint64_t sector, void *buffer)
{
    (void)context;
    if (sector >= 128) return -1;
    memcpy(buffer, disk + sector * BLOCK_SECTOR_SIZE, BLOCK_SECTOR_SIZE);
    return 0;
}

static int write_sector(void *context, uint64_t sector, const void *buffer)
{
    (void)context;
    if (sector >= 128 || fail_writes) return -1;
    memcpy(disk + sector * BLOCK_SECTOR_SIZE, buffer, BLOCK_SECTOR_SIZE);
    return 0;
}

int main(void)
{
    char resolved[STORAGE_MAX_PATH];
    char contents[16] = {0};
    size_t length = 0;
    struct storage_entry_info info;
    struct block_device device = { 0, 128, "disk0", "test", read_sector, write_sector, 0 };
    assert(vfs_resolve_path("/home/user", "file.txt", resolved, sizeof(resolved)) == STORAGE_OK);
    assert(strcmp(resolved, "/home/user/file.txt") == 0);
    assert(vfs_resolve_path("/home/user", "./folder//file.txt/", resolved, sizeof(resolved)) == STORAGE_OK);
    assert(strcmp(resolved, "/home/user/folder/file.txt") == 0);
    assert(vfs_resolve_path("/home/user", "../file.txt", resolved, sizeof(resolved)) == STORAGE_OK);
    assert(strcmp(resolved, "/home/file.txt") == 0);
    assert(vfs_resolve_path("/home/user", "/folder/file.txt", resolved, sizeof(resolved)) == STORAGE_OK);
    assert(strcmp(resolved, "/folder/file.txt") == 0);
    assert(vfs_resolve_path("/", "../../../root.txt", resolved, sizeof(resolved)) == STORAGE_OK);
    assert(strcmp(resolved, "/root.txt") == 0);
    assert(vfs_resolve_path("C:/Users/Guest", "C:/System/Config/config.json", resolved, sizeof(resolved)) == STORAGE_OK);
    assert(strcmp(resolved, "/C:/System/Config/config.json") == 0);
    assert(vfs_resolve_path("C:/Users/Guest", "../Documents/file.txt", resolved, sizeof(resolved)) == STORAGE_OK);
    assert(strcmp(resolved, "/C:/Users/Documents/file.txt") == 0);
    assert(vfs_resolve_path("C:/Users/Guest", "C:\\Users\\Guest\\Desktop\\note.txt", resolved, sizeof(resolved)) == STORAGE_OK);
    assert(strcmp(resolved, "/C:/Users/Guest/Desktop/note.txt") == 0);
    assert(vfs_resolve_path("C:/Users/Guest", "../../../../Recovery", resolved, sizeof(resolved)) == STORAGE_OK);
    assert(strcmp(resolved, "/C:/Recovery") == 0);
    assert(vfs_resolve_path("C:/Users/Guest", "D:/Users/file.txt", resolved, sizeof(resolved)) == STORAGE_ERR_INVAL);

    disk[0] = 0x45;
    assert(vfs_attach_block_device(&device) == STORAGE_OK);
    vfs_init();
    assert(!storage_is_mounted());
    assert(disk[0] == 0x45);
    assert(vfs_mkdir("/not-formatted") == STORAGE_ERR_NOTMOUNTED);

    memset(disk, 0, sizeof(disk));
    assert(vfs_attach_block_device(&device) == STORAGE_OK);
    fail_writes = 1;
    vfs_init();
    assert(!storage_is_mounted());
    fail_writes = 0;
    storage_init();
    assert(storage_is_mounted());
    assert(vfs_stat("/", &info) == STORAGE_OK && info.type == 'd');
    assert(vfs_stat("/.ortos-fs-test", &info) == STORAGE_ERR_NOENT);

    assert(vfs_mkdir("/docs") == STORAGE_OK);
    assert(vfs_create("/docs/guide", "hello", 5) == STORAGE_OK);
    assert(storage_read_file("/docs/guide", contents, sizeof(contents), &length) == 5);
    assert(length == 5 && strcmp(contents, "hello") == 0);
    assert(vfs_write_file("/docs/guide", "world!", 6, 0) == 6);
    memset(contents, 0, sizeof(contents));
    assert(storage_read_file("/docs/guide", contents, sizeof(contents), &length) == 6);
    assert(length == 6 && strcmp(contents, "world!") == 0);

    int index = storage_find_entry("/docs/guide");
    assert(index >= 0);
    assert(storage_get_entry_type(index) == 'f');
    assert(storage_get_entry_name(index)[0] == 'g');
    assert(storage_get_entry_name(index)[1] == 'u');
    assert(storage_get_entry_name(index)[2] == 'i');
    assert(storage_get_entry_name(index)[3] == 'd');
    assert(storage_get_entry_name(index)[4] == 'e');
    assert(storage_get_entry_name(index)[5] == '\0');

    const char* path = storage_get_entry_path(index);
    assert(path[0] == '/');
    assert(path[1] == 'd');
    assert(path[2] == 'o');
    assert(path[3] == 'c');
    assert(path[4] == 's');
    assert(path[5] == '/');
    assert(path[6] == 'g');
    assert(path[7] == 'u');
    assert(path[8] == 'i');
    assert(path[9] == 'd');
    assert(path[10] == 'e');
    assert(path[11] == '\0');

    assert(vfs_unmount() == STORAGE_OK);
    assert(vfs_stat("/", &info) == STORAGE_ERR_NOTMOUNTED);
    assert(vfs_mount() == STORAGE_OK);
    memset(contents, 0, sizeof(contents));
    assert(storage_read_file("/docs/guide", contents, sizeof(contents), &length) == 6);
    assert(length == 6 && strcmp(contents, "world!") == 0);

    memset(disk, 0, BLOCK_SECTOR_SIZE);
    assert(vfs_attach_block_device(&device) == STORAGE_OK);
    vfs_init();
    assert(!storage_is_mounted());
    assert(disk[BLOCK_SECTOR_SIZE] != 0);

    puts("storage path test passed");
    return 0;
}
