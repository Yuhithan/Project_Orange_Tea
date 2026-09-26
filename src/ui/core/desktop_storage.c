#include "desktop_storage.h"

#include "vfs.h"

static const char *const desktop_directories[] = {
    "/C:",
    "/C:/System",
    "/C:/System/Config",
    "/C:/System/Logs",
    "/C:/System/Drivers",
    "/C:/Users",
    "/C:/Users/Guest",
    "/C:/Users/Guest/Desktop",
    "/C:/Users/Guest/Documents",
    "/C:/Users/Guest/Downloads",
    "/C:/Users/Guest/Pictures",
    "/C:/Users/Guest/AppData",
    "/C:/Users/admin",
    "/C:/Users/admin/Desktop",
    "/C:/Users/admin/Documents",
    "/C:/Users/admin/Downloads",
    "/C:/Program Files",
    "/C:/Program Files/ORTos",
    "/C:/Program Files/ORTos/Apps",
    "/C:/ProgramData",
    "/C:/ProgramData/ORTos",
    "/C:/ProgramData/ORTos/Menu",
    "/C:/ProgramData/ORTos/Taskbar",
    "/C:/ProgramData/ORTos/Settings",
    "/C:/Temp",
    "/C:/Recovery",
    "/C:/Logs",
    "/C:/Backups",
};

struct desktop_task_seed {
    const char *path;
    const char *content;
};

static const struct desktop_task_seed desktop_tasks[] = {
    { "/C:/ProgramData/ORTos/Menu/OExplorer.task", "name=OExplorer\ncommand=OExplorer\ncategory=System" },
    { "/C:/ProgramData/ORTos/Menu/setting.task", "name=setting\ncommand=setting\ncategory=System" },
    { "/C:/ProgramData/ORTos/Menu/browser.task", "name=browser\ncommand=browser\ncategory=Internet" },
    { "/C:/ProgramData/ORTos/Menu/terminal.task", "name=Terminal\ncommand=terminal\ncategory=Utilities" },
    { "/C:/ProgramData/ORTos/Taskbar/OExplorer.task", "name=OExplorer\ncommand=OExplorer" },
    { "/C:/ProgramData/ORTos/Taskbar/setting.task", "name=setting\ncommand=setting" },
    { "/C:/ProgramData/ORTos/Taskbar/browser.task", "name=browser\ncommand=browser" },
    { "/C:/Users/Guest/Desktop/OExplorer.task", "name=OExplorer\ncommand=OExplorer" },
    { "/C:/Users/Guest/Desktop/setting.task", "name=setting\ncommand=setting" },
    { "/C:/Users/Guest/Desktop/browser.task", "name=browser\ncommand=browser" }
};

static int ensure_directory(const char *path)
{
    struct storage_entry_info info;
    int result = vfs_stat(path, &info);
    if (result == STORAGE_OK) return info.type == 'd' ? STORAGE_OK : STORAGE_ERR_NOTDIR;
    if (result != STORAGE_ERR_NOENT) return result;
    return vfs_mkdir(path);
}

static int migrate_files(const char *source, const char *destination)
{
    size_t source_length = 0;
    int initial_entry_count = vfs_entry_count();
    while (source[source_length]) source_length++;

    for (int index = 0; index < initial_entry_count; index++) {
        struct storage_entry_info info;
        if (vfs_readdir(index, &info) != STORAGE_OK || info.type != 'f') continue;
        const char *name = info.path;
        size_t position = 0;
        while (source[position] && name[position] == source[position]) position++;
        if (position != source_length || !name[position]) continue;
        name += source_length;
        int direct_child = 1;
        for (const char *cursor = name; *cursor; cursor++) {
            if (*cursor == '/') {
                direct_child = 0;
                break;
            }
        }
        if (!direct_child) continue;

        char target[STORAGE_MAX_PATH];
        size_t destination_length = 0;
        while (destination[destination_length]) destination_length++;
        size_t name_length = 0;
        while (name[name_length]) name_length++;
        if (destination_length + name_length >= sizeof(target)) return STORAGE_ERR_INVAL;
        for (size_t character = 0; character < destination_length; character++)
            target[character] = destination[character];
        for (size_t character = 0; character <= name_length; character++)
            target[destination_length + character] = name[character];

        struct storage_entry_info existing;
        int result = vfs_stat(target, &existing);
        if (result == STORAGE_ERR_NOENT) {
            result = vfs_copy(info.path, target);
            if (result != STORAGE_OK && result != STORAGE_ERR_EXIST) return result;
        } else if (result != STORAGE_OK) {
            return result;
        }
    }
    return STORAGE_OK;
}

static int ensure_task(const struct desktop_task_seed *task)
{
    struct storage_entry_info info;
    int result = vfs_stat(task->path, &info);
    if (result == STORAGE_OK) return info.type == 'f' ? STORAGE_OK : STORAGE_ERR_ISDIR;
    if (result != STORAGE_ERR_NOENT) return result;

    size_t length = 0;
    while (task->content[length]) length++;
    return vfs_create(task->path, task->content, length);
}

int desktop_storage_init(void)
{
    static const struct {
        const char *source;
        const char *destination;
    } migrations[] = {
        { "/C:/menu/", "/C:/ProgramData/ORTos/Menu/" },
        { "/C:/menu/taskbar/", "/C:/ProgramData/ORTos/Taskbar/" },
        { "/C:/user/admin/desktop/", "/C:/Users/admin/Desktop/" },
        { "/C:/Users/admin/Desktop/", "/C:/Users/Guest/Desktop/" }
    };

    for (size_t index = 0; index < sizeof(desktop_directories) / sizeof(desktop_directories[0]); index++) {
        int result = ensure_directory(desktop_directories[index]);
        if (result != STORAGE_OK) return result;
    }

    for (size_t index = 0; index < sizeof(migrations) / sizeof(migrations[0]); index++) {
        int result = migrate_files(migrations[index].source, migrations[index].destination);
        if (result != STORAGE_OK) return result;
    }

    for (size_t index = 0; index < sizeof(desktop_tasks) / sizeof(desktop_tasks[0]); index++) {
        int result = ensure_task(&desktop_tasks[index]);
        if (result != STORAGE_OK) return result;
    }
    return STORAGE_OK;
}