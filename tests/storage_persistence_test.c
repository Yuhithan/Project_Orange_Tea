#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "storage.h"

static unsigned char disk[128 * BLOCK_SECTOR_SIZE];
static int flush_count;
static int flush_failure;
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
    if (sector >= 128) return -1;
    memcpy(disk + sector * BLOCK_SECTOR_SIZE, buffer, BLOCK_SECTOR_SIZE);
    return 0;
}
static int flush_disk(void *context)
{
    (void)context;
    flush_count++;
    return flush_failure ? -1 : 0;
}

int main(void)
{
    struct block_device device = { 0, 128, "disk0", "test", read_sector, write_sector, flush_disk };
    char text[16] = {0};
    size_t length = 0;

    assert(storage_attach_block_device(&device) == STORAGE_OK);
    assert(storage_format() == STORAGE_OK);
    assert(storage_mkdir("/home") == STORAGE_OK);
    assert(storage_write_file("/home/note", "persistent", 10, 0) == 10);
    assert(storage_sync() == STORAGE_OK);
    int previous_flush_count = flush_count;
    assert(storage_sync() == STORAGE_OK && flush_count > previous_flush_count);
    flush_failure = 1;
    assert(storage_sync() == STORAGE_ERR_IO);
    flush_failure = 0;

    assert(storage_attach_block_device(&device) == STORAGE_OK);
    assert(storage_mount() == STORAGE_OK);
    assert(storage_read_file("/home/note", text, sizeof(text), &length) == 10);
    assert(length == 10 && strcmp(text, "persistent") == 0);
    int errors = 0;
    assert(storage_fsck(0, &errors) == STORAGE_OK && errors == 0);

    uint32_t data_sector = (uint32_t)disk[2 * BLOCK_SECTOR_SIZE + 16]
        | ((uint32_t)disk[2 * BLOCK_SECTOR_SIZE + 17] << 8)
        | ((uint32_t)disk[2 * BLOCK_SECTOR_SIZE + 18] << 16)
        | ((uint32_t)disk[2 * BLOCK_SECTOR_SIZE + 19] << 24);
    assert(data_sector >= 66 && data_sector < 128);
    disk[65 * BLOCK_SECTOR_SIZE + data_sector / 8] &= (unsigned char)~(1u << (data_sector % 8));
    assert(storage_attach_block_device(&device) == STORAGE_OK);
    assert(storage_mount() == STORAGE_OK);
    assert(storage_fsck(0, &errors) == STORAGE_ERR_CORRUPT && errors > 0);
    assert(storage_fsck(1, &errors) == STORAGE_OK && errors == 0);
    assert(storage_fsck(0, &errors) == STORAGE_OK && errors == 0);

    const uint32_t orphan_sector = 127;
    disk[65 * BLOCK_SECTOR_SIZE + orphan_sector / 8] |= (unsigned char)(1u << (orphan_sector % 8));
    assert(storage_attach_block_device(&device) == STORAGE_OK);
    assert(storage_mount() == STORAGE_OK);
    assert(storage_fsck(0, &errors) == STORAGE_ERR_CORRUPT && errors > 0);
    assert(storage_fsck(1, &errors) == STORAGE_ERR_CORRUPT && errors > 0);
    assert(disk[65 * BLOCK_SECTOR_SIZE + orphan_sector / 8] & (1u << (orphan_sector % 8)));
    disk[65 * BLOCK_SECTOR_SIZE + orphan_sector / 8] &= (unsigned char)~(1u << (orphan_sector % 8));
    assert(storage_attach_block_device(&device) == STORAGE_OK);
    assert(storage_mount() == STORAGE_OK);
    assert(storage_fsck(0, &errors) == STORAGE_OK && errors == 0);

    assert(storage_remove_entry("/home") == 0);
    assert(storage_remove_entry("/home/note") == 1);
    assert(storage_remove_entry("/home") == 1);

    assert(storage_mkdir("/ops") == STORAGE_OK);
    assert(storage_mkdir("/ops/sub") == STORAGE_OK);
    assert(storage_mkdir("/missing/child") == STORAGE_ERR_NOTDIR);
    char oversized[256] = {0};
    assert(storage_write_file("/ops/sub/too-big", oversized, sizeof(oversized), 0) == STORAGE_ERR_NOSPC);
    assert(storage_find_entry("/ops/sub/too-big") < 0);
    assert(storage_create_file("/ops/sub/a", "abc", 3) == STORAGE_OK);
    assert(storage_create_file("/ops/sub/a", "x", 1) == STORAGE_ERR_EXIST);
    assert(storage_get_entry_info("/ops/sub/a", 0) == STORAGE_ERR_INVAL);
    struct storage_entry_info info;
    assert(storage_get_entry_info("/ops/sub/a", &info) == STORAGE_OK);
    assert(info.type == 'f' && info.size == 3 && info.blocks_used == 1);
    assert(storage_unlink("/ops") == STORAGE_ERR_ISDIR);
    assert(storage_rmdir("/ops/sub") == STORAGE_ERR_NOTEMPTY);
    assert(storage_rename("/ops/sub", "/ops/moved") == STORAGE_OK);
    memset(text, 0, sizeof(text));
    assert(storage_read_file("/ops/moved/a", text, sizeof(text), &length) == 3);
    assert(length == 3 && strcmp(text, "abc") == 0);
    assert(storage_copy("/ops/moved/a", "/ops/moved/b") == STORAGE_OK);
    assert(storage_copy("/ops/moved/a", "/ops/moved/b") == STORAGE_ERR_EXIST);
    assert(storage_rmdir("/ops") == STORAGE_ERR_NOTEMPTY);
    assert(storage_attach_block_device(&device) == STORAGE_OK);
    assert(storage_mount() == STORAGE_OK);
    memset(text, 0, sizeof(text));
    assert(storage_read_file("/ops/moved/b", text, sizeof(text), &length) == 3);
    assert(length == 3 && strcmp(text, "abc") == 0);
    assert(storage_remove_tree("/ops") == STORAGE_OK);
    assert(storage_find_entry("/ops") < 0 && storage_find_entry("/ops/moved/a") < 0);
    assert(storage_rmdir("/") == STORAGE_ERR_INVAL);
    assert(storage_unmount() == STORAGE_OK);
    assert(storage_read_file("/missing", text, sizeof(text), &length) == STORAGE_ERR_NOTMOUNTED);
    assert(storage_mount() == STORAGE_OK);

    assert(storage_self_test() == STORAGE_SELFTEST_ALL);
    assert(storage_is_mounted());
    assert(storage_get_entry_count() == 0);
    return 0;
}