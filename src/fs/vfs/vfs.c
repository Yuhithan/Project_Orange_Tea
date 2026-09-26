#include "vfs.h"

static void vfs_copy_bytes(char *destination, const char *source, size_t count)
{
    for (size_t i = 0; i < count; i++) destination[i] = source[i];
}

void vfs_init(void) { storage_init(); }
int vfs_attach_block_device(const struct block_device *device) { return storage_attach_block_device(device); }

int vfs_resolve_path(const char *cwd, const char *path, char *out, size_t capacity)
{
    char combined[STORAGE_MAX_PATH * 2];
    size_t combined_length = 0, output_length = 1, starts[32], components = 0;
    if (!cwd || !path || !out || capacity < 2 || !*path) return STORAGE_ERR_INVAL;

    if (path[0] == '/') {
        while (path[combined_length] && combined_length + 1 < sizeof(combined)) {
            combined[combined_length] = path[combined_length];
            combined_length++;
        }
        if (path[combined_length]) return STORAGE_ERR_INVAL;
    } else {
        size_t cwd_length = 0, path_length = 0;
        while (cwd[cwd_length] && cwd_length + 1 < sizeof(combined)) cwd_length++;
        while (path[path_length] && path_length + 1 < sizeof(combined)) path_length++;
        if (cwd[cwd_length] || path[path_length] || cwd_length + path_length + 2 >= sizeof(combined)) return STORAGE_ERR_INVAL;
        vfs_copy_bytes(combined, cwd, cwd_length);
        combined_length = cwd_length;
        if (!cwd_length || cwd[cwd_length - 1] != '/') combined[combined_length++] = '/';
        vfs_copy_bytes(combined + combined_length, path, path_length);
        combined_length += path_length;
    }

    out[0] = '/';
    out[1] = 0;
    for (size_t position = 0; position < combined_length;) {
        while (position < combined_length && combined[position] == '/') position++;
        if (position == combined_length) break;
        size_t first = position;
        while (position < combined_length && combined[position] != '/') position++;
        size_t count = position - first;
        if (count == 1 && combined[first] == '.') continue;
        if (count == 2 && combined[first] == '.' && combined[first + 1] == '.') {
            if (components) {
                output_length = starts[--components];
                out[output_length] = 0;
            }
            continue;
        }
        size_t separator = output_length > 1;
        if (count >= 16 || components >= 32 || output_length + separator + count >= STORAGE_MAX_PATH ||
            output_length + separator + count >= capacity) return STORAGE_ERR_INVAL;
        starts[components++] = output_length;
        if (separator) out[output_length++] = '/';
        vfs_copy_bytes(out + output_length, combined + first, count);
        output_length += count;
        out[output_length] = 0;
    }
    return STORAGE_OK;
}

int vfs_open(const char *path, int create) { return storage_open(path, create); }
int vfs_close(int fd) { return storage_close(fd); }
int vfs_read(int fd, void *buffer, size_t length) { return storage_read(fd, buffer, length); }
int vfs_write(int fd, const void *buffer, size_t length) { return storage_write(fd, buffer, length); }
int vfs_seek(int fd, size_t offset) { return storage_seek(fd, offset); }
int vfs_create(const char *path, const void *buffer, size_t length) { return storage_create_file(path, buffer, length); }
int vfs_write_file(const char *path, const void *buffer, size_t length, int append) { return storage_write_file(path, buffer, length, append); }
int vfs_mkdir(const char *path) { return storage_mkdir(path); }
int vfs_unlink(const char *path) { return storage_unlink(path); }
int vfs_rmdir(const char *path) { return storage_rmdir(path); }
int vfs_remove_tree(const char *path) { return storage_remove_tree(path); }
int vfs_rename(const char *source, const char *destination) { return storage_rename(source, destination); }
int vfs_copy(const char *source, const char *destination) { return storage_copy(source, destination); }
int vfs_stat(const char *path, struct storage_entry_info *info) { return storage_get_entry_info(path, info); }
int vfs_entry_count(void) { return storage_get_entry_count(); }
int vfs_readdir(int index, struct storage_entry_info *info)
{
    if (index < 0 || index >= storage_get_entry_count()) return STORAGE_ERR_NOENT;
    return storage_get_entry_info(storage_get_entry_path(index), info);
}
int vfs_get_stats(struct storage_stats *stats) { return storage_get_stats(stats); }
int vfs_get_device_info(struct storage_device_info *info) { return storage_get_device_info(info); }
int vfs_mount(void) { return storage_mount(); }
int vfs_unmount(void) { return storage_unmount(); }
int vfs_format(void) { return storage_format(); }
int vfs_sync(void) { return storage_sync(); }
int vfs_fsck(int repair, int *errors) { return storage_fsck(repair, errors); }