#include <assert.h>
#include <string.h>

#include "desktop_storage.h"
#include "vfs.h"

static void read_text(const char *path, char *text, size_t capacity)
{
    size_t length = 0;
    assert(storage_read_file(path, text, capacity, &length) >= 0);
    assert(length < capacity);
    text[length] = '\0';
}

int main(void)
{
    char legacy_text[64];
    char migrated_text[64];
    struct storage_entry_info info;

    vfs_init();
    assert(vfs_mkdir("/C:") == STORAGE_OK);
    assert(vfs_mkdir("/C:/menu") == STORAGE_OK);
    assert(vfs_mkdir("/C:/menu/taskbar") == STORAGE_OK);
    assert(vfs_mkdir("/C:/user") == STORAGE_OK);
    assert(vfs_mkdir("/C:/user/admin") == STORAGE_OK);
    assert(vfs_mkdir("/C:/user/admin/desktop") == STORAGE_OK);
    assert(vfs_create("/C:/menu/custom.task", "name=Custom\ncommand=custom", 26) == STORAGE_OK);
    assert(vfs_write_file("/C:/user/admin/desktop/notes.txt", "keep me", 7, 0) == 7);

    assert(desktop_storage_init() == STORAGE_OK);
    assert(vfs_stat("/C:/System/Config", &info) == STORAGE_OK && info.type == 'd');
    assert(vfs_stat("/C:/Users/admin/Documents", &info) == STORAGE_OK && info.type == 'd');
    assert(vfs_stat("/C:/Program Files/ORTos/Apps", &info) == STORAGE_OK && info.type == 'd');
    assert(vfs_stat("/C:/ProgramData/ORTos/Taskbar", &info) == STORAGE_OK && info.type == 'd');
    assert(vfs_stat("/C:/Temp", &info) == STORAGE_OK && info.type == 'd');
    assert(vfs_stat("/C:/Logs", &info) == STORAGE_OK && info.type == 'd');
    assert(vfs_stat("/C:/Backups", &info) == STORAGE_OK && info.type == 'd');

    read_text("/C:/menu/custom.task", legacy_text, sizeof(legacy_text));
    read_text("/C:/ProgramData/ORTos/Menu/custom.task", migrated_text, sizeof(migrated_text));
    assert(strcmp(legacy_text, migrated_text) == 0);
    read_text("/C:/user/admin/desktop/notes.txt", legacy_text, sizeof(legacy_text));
    read_text("/C:/Users/admin/Desktop/notes.txt", migrated_text, sizeof(migrated_text));
    assert(strcmp(legacy_text, migrated_text) == 0);
    assert(vfs_stat("/C:/ProgramData/ORTos/Menu/OExplorer.task", &info) == STORAGE_OK);

    assert(vfs_write_file("/C:/ProgramData/ORTos/Menu/custom.task", "new version", 11, 0) == 11);
    int entry_count = vfs_entry_count();
    assert(desktop_storage_init() == STORAGE_OK);
    assert(vfs_entry_count() == entry_count);
    read_text("/C:/menu/custom.task", legacy_text, sizeof(legacy_text));
    assert(strcmp(legacy_text, "name=Custom\ncommand=custom") == 0);
    read_text("/C:/ProgramData/ORTos/Menu/custom.task", migrated_text, sizeof(migrated_text));
    assert(strcmp(migrated_text, "new version") == 0);
    return 0;
}