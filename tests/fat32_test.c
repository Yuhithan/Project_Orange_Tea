#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "storage.h"
#include "vfs.h"

#define TEST_SECTORS 70000u

static uint8_t *disk;

struct file_disk {
    FILE *stream;
    uint64_t sectors;
};

static int read_file_sector(void *context, uint64_t sector, void *buffer)
{
    struct file_disk *file_disk = context;
    if (!file_disk || sector >= file_disk->sectors ||
        fseek(file_disk->stream, (long)(sector * BLOCK_SECTOR_SIZE), SEEK_SET) != 0)
        return -1;
    return fread(buffer, 1, BLOCK_SECTOR_SIZE, file_disk->stream) == BLOCK_SECTOR_SIZE ? 0 : -1;
}

static int write_file_sector(void *context, uint64_t sector, const void *buffer)
{
    struct file_disk *file_disk = context;
    if (!file_disk || sector >= file_disk->sectors ||
        fseek(file_disk->stream, (long)(sector * BLOCK_SECTOR_SIZE), SEEK_SET) != 0)
        return -1;
    return fwrite(buffer, 1, BLOCK_SECTOR_SIZE, file_disk->stream) == BLOCK_SECTOR_SIZE ? 0 : -1;
}

static int read_sector(void *context, uint64_t sector, void *buffer)
{
    uint8_t *bytes = context;
    if (sector >= TEST_SECTORS || !buffer) return -1;
    memcpy(buffer, bytes + sector * BLOCK_SECTOR_SIZE, BLOCK_SECTOR_SIZE);
    return 0;
}

static int write_sector(void *context, uint64_t sector, const void *buffer)
{
    uint8_t *bytes = context;
    if (sector >= TEST_SECTORS || !buffer) return -1;
    memcpy(bytes + sector * BLOCK_SECTOR_SIZE, buffer, BLOCK_SECTOR_SIZE);
    return 0;
}

int main(int argc, char **argv)
{
    static const char *const directories[] = {
        "C:/Windows", "C:/Windows/System32",
        "C:/Windows/System32/Drivers",
        "C:/Windows/System32/Libraries",
        "C:/Windows/System32/Core",
        "C:/Windows/System32/Services",
        "C:/Users", "C:/Users/Default", "C:/Users/Default/Documents"
    };
    const char *long_path =
        "C:/Users/Default/Documents/Quarterly Systems Architecture Review.txt";
    struct block_device device;
    struct storage_entry_info info;
    struct storage_stats stats;
    char content[2048], readback[2048];
    size_t read_length = 0;

    disk = calloc(TEST_SECTORS, BLOCK_SECTOR_SIZE);
    assert(disk);
    device.context = disk;
    device.sector_count = TEST_SECTORS;
    device.name = "fat32-test";
    device.type = "RAM test disk";
    device.read_sector = read_sector;
    device.write_sector = write_sector;
    device.flush = 0;

    assert(vfs_attach_block_device(&device) == STORAGE_OK);
    assert(vfs_format() == STORAGE_OK);
    assert(storage_is_mounted());
    assert(disk[510] == 0x55 && disk[511] == 0xaa);
    assert(disk[82] == 'F' && disk[83] == 'A' && disk[84] == 'T');
    for (size_t index = 0; index < sizeof(directories) / sizeof(directories[0]); index++)
        assert(vfs_mkdir(directories[index]) == STORAGE_OK);

    memset(content, 'A', sizeof(content));
    memcpy(content, "persistent FAT32 payload", 24);
    assert(vfs_create(long_path, content, sizeof(content)) == STORAGE_OK);
    assert(vfs_stat(long_path, &info) == STORAGE_OK);
    assert(info.type == 'f' && info.size == sizeof(content));
    assert(strcmp(info.name, "Quarterly Systems Architecture Review.txt") == 0);
    assert(storage_read_file(long_path, readback, sizeof(readback), &read_length) ==
        (int)sizeof(content));
    assert(memcmp(content, readback, sizeof(content)) == 0);

    int fd = vfs_open(long_path, 0);
    assert(fd >= 0);
    assert(vfs_seek(fd, 10) == STORAGE_OK);
    memset(readback, 0, sizeof(readback));
    assert(vfs_read(fd, readback, 12) == 12);
    assert(memcmp(readback, content + 10, 12) == 0);
    assert(vfs_seek(fd, 0) == STORAGE_OK);
    assert(vfs_write(fd, "FAT32", 5) == 5);
    assert(vfs_close(fd) == STORAGE_OK);
    memcpy(content, "FAT32", 5);

    assert(vfs_rename(long_path,
        "C:/Users/Default/Documents/Quarterly Systems Architecture Notes.txt") == STORAGE_OK);
    assert(vfs_copy(
        "C:/Users/Default/Documents/Quarterly Systems Architecture Notes.txt",
        "C:/Windows/System32/Core/system notes.dat") == STORAGE_OK);
    assert(vfs_get_stats(&stats) == STORAGE_OK);
    assert(stats.file_count == 2 && stats.directory_count >= 8 && stats.free_sectors > 0);

    assert(vfs_unmount() == STORAGE_OK);
    assert(!storage_is_mounted());
    assert(vfs_attach_block_device(&device) == STORAGE_OK);
    assert(vfs_mount() == STORAGE_OK);
    assert(vfs_stat(
        "C:/Users/Default/Documents/Quarterly Systems Architecture Notes.txt", &info) == STORAGE_OK);
    assert(storage_read_file(
        "C:/Users/Default/Documents/Quarterly Systems Architecture Notes.txt",
        readback, sizeof(readback), &read_length) == (int)sizeof(content));
    assert(read_length == sizeof(content));
    assert(memcmp(content, readback, sizeof(content)) == 0);

    assert(vfs_rmdir("C:/Windows") == STORAGE_ERR_NOTEMPTY);
    assert(vfs_remove_tree("C:/Windows/System32") == STORAGE_OK);
    assert(vfs_stat("C:/Windows/System32/Core/system notes.dat", &info) == STORAGE_ERR_NOENT);
    assert(vfs_unlink(
        "C:/Users/Default/Documents/Quarterly Systems Architecture Notes.txt") == STORAGE_OK);
    assert(vfs_rmdir("C:/Users/Default/Documents") == STORAGE_OK);
    assert(vfs_unmount() == STORAGE_OK);
    free(disk);

    if (argc > 1) {
        struct file_disk file_disk;
        struct stat image_stat;
        struct block_device image_device;
        file_disk.stream = fopen(argv[1], "r+b");
        assert(file_disk.stream);
        assert(fstat(fileno(file_disk.stream), &image_stat) == 0);
        assert(image_stat.st_size > 0 && image_stat.st_size % BLOCK_SECTOR_SIZE == 0);
        file_disk.sectors = (uint64_t)image_stat.st_size / BLOCK_SECTOR_SIZE;
        image_device.context = &file_disk;
        image_device.sector_count = file_disk.sectors;
        image_device.name = "sysroot-image";
        image_device.type = "FAT32 image";
        image_device.read_sector = read_file_sector;
        image_device.write_sector = write_file_sector;
        image_device.flush = 0;
        assert(vfs_attach_block_device(&image_device) == STORAGE_OK);
        assert(vfs_mount() == STORAGE_OK);
        assert(storage_is_fat32());
        assert(vfs_stat("C:/Windows/System32/Core/kernel.bin", &info) == STORAGE_OK);
        assert(vfs_stat("C:/Users/Default/Documents", &info) == STORAGE_OK);
        assert(vfs_stat("C:/Programs/Portable", &info) == STORAGE_OK);
        assert(vfs_unmount() == STORAGE_OK);
        assert(fclose(file_disk.stream) == 0);
    }
    return 0;
}
