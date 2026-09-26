#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "storage.h"
#include "vfs.h"

int main(void)
{
    char resolved[STORAGE_MAX_PATH];
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

    storage_init();

    int created = storage_create_entry("/docs/guide", 'f', "hello");
    assert(created == 1);

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

    puts("storage path test passed");
    return 0;
}
