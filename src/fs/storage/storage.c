#include "storage.h"

#define STORAGE_MAGIC 0x4f525346u
#define STORAGE_VERSION 2u
#define STORAGE_INODE_SECTORS STORAGE_MAX_ENTRIES
#define STORAGE_BITMAP_START (1u + STORAGE_INODE_SECTORS)
#define STORAGE_BITMAP_BITS_PER_SECTOR (BLOCK_SECTOR_SIZE * 8u)
#define STORAGE_CACHE_SLOTS 4
#define STORAGE_CONTENT_CAPACITY 256u
#define STORAGE_SELFTEST_SECTORS 72u

struct storage_entry {
    char path[STORAGE_MAX_PATH];
    char name[16];
    char type;
    size_t size;
    uint32_t blocks[STORAGE_MAX_FILE_BLOCKS];
    char content[STORAGE_CONTENT_CAPACITY];
};
struct storage_fd { int entry; size_t offset; int used; };
struct cache_sector { uint64_t sector; unsigned char data[BLOCK_SECTOR_SIZE]; int valid; int dirty; };

static struct storage_entry entries[STORAGE_MAX_ENTRIES];
static struct storage_fd fds[16];
static struct cache_sector cache[STORAGE_CACHE_SLOTS];
static unsigned int cache_next_slot;
static const struct block_device *storage_device;
static uint64_t bitmap_sectors;
static uint64_t data_start;
static int entry_count;
static int mounted;
static int create(const char *path, char type, const char *content);

struct storage_saved_state {
    struct storage_entry entries[STORAGE_MAX_ENTRIES];
    struct storage_fd fds[16];
    struct cache_sector cache[STORAGE_CACHE_SLOTS];
    const struct block_device *device;
    uint64_t bitmap_sectors;
    uint64_t data_start;
    unsigned int cache_next_slot;
    int entry_count;
    int mounted;
};

static struct storage_saved_state selftest_saved_state;
static unsigned char selftest_disk[STORAGE_SELFTEST_SECTORS * BLOCK_SECTOR_SIZE];
static struct block_device selftest_device;

static void zero(void *buffer, size_t length) { unsigned char *p = buffer; while (length--) *p++ = 0; }
static void copy(void *out, const void *in, size_t length) { unsigned char *d = out; const unsigned char *s = in; while (length--) *d++ = *s++; }
static int equal(const char *a, const char *b) { while (*a && *b && *a == *b) { a++; b++; } return *a == *b; }
static int path_prefix(const char *path, const char *prefix) { while (*prefix && *path == *prefix) { path++; prefix++; } return *prefix == 0 && *path == '/'; }
static void text_copy(char *out, const char *in, size_t limit) { size_t i = 0; while (in && in[i] && i + 1 < limit) { out[i] = in[i]; i++; } out[i] = 0; }
static uint32_t get32(const unsigned char *p) { return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); }
static uint64_t get64(const unsigned char *p) { uint64_t value = 0; for (int i = 7; i >= 0; i--) value = (value << 8) | p[i]; return value; }
static void put32(unsigned char *p, uint32_t value) { for (int i = 0; i < 4; i++) { p[i] = (unsigned char)value; value >>= 8; } }
static void put64(unsigned char *p, uint64_t value) { for (int i = 0; i < 8; i++) { p[i] = (unsigned char)value; value >>= 8; } }
static int selftest_read_sector(void *context, uint64_t sector, void *buffer)
{
    unsigned char *disk = context;
    if (!disk || !buffer || sector >= STORAGE_SELFTEST_SECTORS) return -1;
    copy(buffer, disk + sector * BLOCK_SECTOR_SIZE, BLOCK_SECTOR_SIZE);
    return 0;
}
static int selftest_write_sector(void *context, uint64_t sector, const void *buffer)
{
    unsigned char *disk = context;
    if (!disk || !buffer || sector >= STORAGE_SELFTEST_SECTORS) return -1;
    copy(disk + sector * BLOCK_SECTOR_SIZE, buffer, BLOCK_SECTOR_SIZE);
    return 0;
}
static int selftest_fail_read(void *context, uint64_t sector, void *buffer)
{
    (void)context; (void)sector; (void)buffer;
    return -1;
}
static int selftest_fail_write(void *context, uint64_t sector, const void *buffer)
{
    (void)context; (void)sector; (void)buffer;
    return -1;
}

static int disk_read(uint64_t sector, void *buffer)
{
    if (!storage_device || sector >= storage_device->sector_count || !buffer) return STORAGE_ERR_IO;
    for (int i = 0; i < STORAGE_CACHE_SLOTS; i++) if (cache[i].valid && cache[i].sector == sector) { copy(buffer, cache[i].data, BLOCK_SECTOR_SIZE); return STORAGE_OK; }
    if (storage_device->read_sector(storage_device->context, sector, buffer) != 0) return STORAGE_ERR_IO;
    int slot = -1;
    for (int i = 0; i < STORAGE_CACHE_SLOTS; i++) if (!cache[i].valid) { slot = i; break; }
    if (slot < 0) slot = (int)(cache_next_slot % STORAGE_CACHE_SLOTS);
    cache_next_slot = ((unsigned int)slot + 1u) % STORAGE_CACHE_SLOTS;
    cache[slot].valid = 1; cache[slot].sector = sector; cache[slot].dirty = 0;
    copy(cache[slot].data, buffer, BLOCK_SECTOR_SIZE);
    return STORAGE_OK;
}
static int disk_write(uint64_t sector, const void *buffer)
{
    if (!storage_device || sector >= storage_device->sector_count || !buffer) return STORAGE_ERR_IO;
    if (storage_device->write_sector(storage_device->context, sector, buffer) != 0) return STORAGE_ERR_IO;
    for (int i = 0; i < STORAGE_CACHE_SLOTS; i++) if (cache[i].valid && cache[i].sector == sector) { copy(cache[i].data, buffer, BLOCK_SECTOR_SIZE); cache[i].dirty = 0; }
    return STORAGE_OK;
}

static int normalize(const char *path, char out[STORAGE_MAX_PATH])
{
    int length = 1, starts[32], components = 0;
    if (!path || !*path) return STORAGE_ERR_INVAL;
    out[0] = '/'; out[1] = 0;
    for (int i = 0; path[i];) {
        while (path[i] == '/') i++;
        if (!path[i]) break;
        int first = i; while (path[i] && path[i] != '/') i++;
        int count = i - first;
        if (count == 1 && path[first] == '.') continue;
        if (count == 2 && path[first] == '.' && path[first + 1] == '.') { if (components) { length = starts[--components]; out[length] = 0; } continue; }
        if (count >= 16 || components == 32 || length + count + 1 >= STORAGE_MAX_PATH) return STORAGE_ERR_INVAL;
        starts[components++] = length;
        if (length > 1) out[length++] = '/';
        for (int j = 0; j < count; j++) out[length++] = path[first + j];
        out[length] = 0;
    }
    return STORAGE_OK;
}
static int find(const char *path)
{
    char normalized[STORAGE_MAX_PATH];
    if (normalize(path, normalized) != STORAGE_OK) return -1;
    for (int i = 0; i < entry_count; i++) if (equal(entries[i].path, normalized)) return i;
    return -1;
}
static int parent_directory(const char *path)
{
    char parent[STORAGE_MAX_PATH]; text_copy(parent, path, sizeof(parent));
    int length = 0; while (parent[length]) length++;
    while (length > 1 && parent[length - 1] != '/') length--;
    if (length <= 1) return 1;
    parent[length - 1] = 0;
    int index = find(parent);
    if (index < 0) return create(parent, 'd', 0) == STORAGE_OK;
    return entries[index].type == 'd';
}
static int parent_exists(const char *path)
{
    char parent[STORAGE_MAX_PATH]; text_copy(parent, path, sizeof(parent));
    int length = 0; while (parent[length]) length++;
    while (length > 1 && parent[length - 1] != '/') length--;
    if (length <= 1) return 1;
    parent[length - 1] = 0;
    int index = find(parent);
    return index >= 0 && entries[index].type == 'd';
}

static int bitmap_bit(uint64_t sector, int set)
{
    unsigned char block[BLOCK_SECTOR_SIZE];
    uint64_t bitmap_sector = STORAGE_BITMAP_START + sector / STORAGE_BITMAP_BITS_PER_SECTOR;
    unsigned int bit = (unsigned int)(sector % STORAGE_BITMAP_BITS_PER_SECTOR);
    if (disk_read(bitmap_sector, block) != STORAGE_OK) return STORAGE_ERR_IO;
    if (set) block[bit / 8] |= (unsigned char)(1u << (bit % 8));
    else block[bit / 8] &= (unsigned char)~(1u << (bit % 8));
    return disk_write(bitmap_sector, block);
}
static int bitmap_value(uint64_t sector, int *value)
{
    unsigned char block[BLOCK_SECTOR_SIZE];
    uint64_t bitmap_sector = STORAGE_BITMAP_START + sector / STORAGE_BITMAP_BITS_PER_SECTOR;
    unsigned int bit = (unsigned int)(sector % STORAGE_BITMAP_BITS_PER_SECTOR);
    if (!value || bitmap_sector >= STORAGE_BITMAP_START + bitmap_sectors || disk_read(bitmap_sector, block) != STORAGE_OK) return STORAGE_ERR_IO;
    *value = (block[bit / 8] >> (bit % 8)) & 1;
    return STORAGE_OK;
}
static int sector_used(uint64_t sector)
{
    int value = 1;
    if (bitmap_value(sector, &value) != STORAGE_OK) return 1;
    return value;
}
static int allocate_sector(uint32_t *result)
{
    if (!storage_device) return STORAGE_ERR_NOSPC;
    for (uint64_t sector = data_start; sector < storage_device->sector_count; sector++) if (!sector_used(sector)) {
        if (bitmap_bit(sector, 1) != STORAGE_OK) return STORAGE_ERR_IO;
        *result = (uint32_t)sector;
        return STORAGE_OK;
    }
    return STORAGE_ERR_NOSPC;
}

static int write_inode(int index)
{
    unsigned char block[BLOCK_SECTOR_SIZE]; struct storage_entry *entry = &entries[index];
    if (entry->type == 'f' && entry->size && entry->blocks[0] == 0) {
        uint32_t sector;
        if (allocate_sector(&sector) != STORAGE_OK) return STORAGE_ERR_NOSPC;
        entry->blocks[0] = sector;
    }
    if (entry->type == 'f' && entry->blocks[0]) {
        unsigned char data[BLOCK_SECTOR_SIZE]; zero(data, sizeof(data)); copy(data, entry->content, entry->size);
        if (disk_write(entry->blocks[0], data) != STORAGE_OK) return STORAGE_ERR_IO;
    }
    zero(block, sizeof(block)); block[0] = (unsigned char)entry->type; put64(block + 8, entry->size);
    for (int i = 0; i < STORAGE_MAX_FILE_BLOCKS; i++) put32(block + 16 + i * 4, entry->blocks[i]);
    copy(block + 64, entry->path, sizeof(entry->path)); copy(block + 128, entry->name, sizeof(entry->name));
    if (entry->type == 'f') copy(block + 144, entry->content, sizeof(entry->content));
    return disk_write((uint64_t)index + 1, block);
}
static int read_inode(int index)
{
    unsigned char block[BLOCK_SECTOR_SIZE]; struct storage_entry *entry = &entries[index];
    if (disk_read((uint64_t)index + 1, block) != STORAGE_OK) return STORAGE_ERR_IO;
    entry->type = (char)block[0]; entry->size = (size_t)get64(block + 8);
    if ((entry->type != 'f' && entry->type != 'd') || entry->size > sizeof(entry->content)) return STORAGE_ERR_CORRUPT;
    for (int i = 0; i < STORAGE_MAX_FILE_BLOCKS; i++) entry->blocks[i] = get32(block + 16 + i * 4);
    copy(entry->path, block + 64, sizeof(entry->path)); copy(entry->name, block + 128, sizeof(entry->name));
    if (!entry->path[0] || entry->path[sizeof(entry->path) - 1]) return STORAGE_ERR_CORRUPT;
    if (entry->type == 'f') {
        copy(entry->content, block + 144, sizeof(entry->content));
        if (entry->blocks[0]) { unsigned char data[BLOCK_SECTOR_SIZE]; if (disk_read(entry->blocks[0], data) != STORAGE_OK) return STORAGE_ERR_IO; copy(entry->content, data, sizeof(entry->content)); }
    } else entry->content[0] = 0;
    entry->content[sizeof(entry->content) - 1] = 0;
    return STORAGE_OK;
}
static int write_super(void)
{
    unsigned char block[BLOCK_SECTOR_SIZE]; zero(block, sizeof(block));
    put32(block, STORAGE_MAGIC); put32(block + 4, STORAGE_VERSION); put32(block + 8, (uint32_t)entry_count);
    put64(block + 16, storage_device->sector_count); put64(block + 24, bitmap_sectors); put64(block + 32, data_start);
    return disk_write(0, block);
}

int storage_attach_block_device(const struct block_device *device)
{
    if (!device || !device->read_sector || !device->write_sector || device->sector_count < 70) return STORAGE_ERR_INVAL;
    storage_device = device; mounted = 0; entry_count = 0; cache_next_slot = 0; zero(cache, sizeof(cache)); return STORAGE_OK;
}
int storage_read_blocks(const struct block_device *device, uint64_t block, void *buffer, size_t count)
{
    if (!device || !device->read_sector || (!buffer && count) || block > device->sector_count || count > device->sector_count - block) return STORAGE_ERR_INVAL;
    for (size_t i = 0; i < count; i++) if (device->read_sector(device->context, block + i, (unsigned char *)buffer + i * BLOCK_SECTOR_SIZE) != 0) return STORAGE_ERR_IO;
    return STORAGE_OK;
}
int storage_write_blocks(const struct block_device *device, uint64_t block, const void *buffer, size_t count)
{
    if (!device || !device->write_sector || (!buffer && count) || block > device->sector_count || count > device->sector_count - block) return STORAGE_ERR_INVAL;
    for (size_t i = 0; i < count; i++) if (device->write_sector(device->context, block + i, (const unsigned char *)buffer + i * BLOCK_SECTOR_SIZE) != 0) return STORAGE_ERR_IO;
    return STORAGE_OK;
}
int storage_format(void)
{
    unsigned char block[BLOCK_SECTOR_SIZE];
    if (!storage_device) return STORAGE_ERR_NOENT;
    bitmap_sectors = (storage_device->sector_count + STORAGE_BITMAP_BITS_PER_SECTOR - 1) / STORAGE_BITMAP_BITS_PER_SECTOR;
    data_start = STORAGE_BITMAP_START + bitmap_sectors;
    if (data_start >= storage_device->sector_count) return STORAGE_ERR_NOSPC;
    zero(cache, sizeof(cache)); cache_next_slot = 0; entry_count = 0; mounted = 1;
    if (write_super() != STORAGE_OK) return STORAGE_ERR_IO;
    zero(block, sizeof(block));
    for (uint64_t i = 0; i < bitmap_sectors; i++) if (disk_write(STORAGE_BITMAP_START + i, block) != STORAGE_OK) return STORAGE_ERR_IO;
    for (uint64_t i = 0; i < data_start; i++) if (bitmap_bit(i, 1) != STORAGE_OK) return STORAGE_ERR_IO;
    for (int i = 0; i < STORAGE_MAX_ENTRIES; i++) if (disk_write((uint64_t)i + 1, block) != STORAGE_OK) return STORAGE_ERR_IO;
    return STORAGE_OK;
}
int storage_mount(void)
{
    unsigned char block[BLOCK_SECTOR_SIZE];
    if (!storage_device) return STORAGE_ERR_NOENT;
    if (disk_read(0, block) != STORAGE_OK) return STORAGE_ERR_IO;
    if (get32(block) != STORAGE_MAGIC || get32(block + 4) != STORAGE_VERSION || get64(block + 16) != storage_device->sector_count) return STORAGE_ERR_NOENT;
    entry_count = (int)get32(block + 8); bitmap_sectors = get64(block + 24); data_start = get64(block + 32);
    if (entry_count < 0 || entry_count > STORAGE_MAX_ENTRIES || data_start >= storage_device->sector_count || bitmap_sectors == 0) return STORAGE_ERR_CORRUPT;
    for (int i = 0; i < entry_count; i++) if (read_inode(i) != STORAGE_OK) { mounted = 0; return STORAGE_ERR_CORRUPT; }
    mounted = 1; return STORAGE_OK;
}
int storage_unmount(void) { int result = storage_sync(); if (result == STORAGE_OK) mounted = 0; return result; }
int storage_is_mounted(void) { return mounted; }
void storage_init(void) { for (int i = 0; i < 16; i++) fds[i].used = 0; if (storage_device) (void)storage_mount();
#if __STDC_HOSTED__
    else mounted = 1;
#endif
}
int storage_sync(void) { if (!storage_device) return mounted ? STORAGE_OK : STORAGE_ERR_NOENT; if (mounted && write_super() != STORAGE_OK) return STORAGE_ERR_IO; for (int i = 0; i < entry_count; i++) if (write_inode(i) != STORAGE_OK) return STORAGE_ERR_IO; return STORAGE_OK; }

static int create(const char *path, char type, const char *content)
{
    char normalized[STORAGE_MAX_PATH];
    if (!mounted || normalize(path, normalized) != STORAGE_OK || (type != 'f' && type != 'd') || equal(normalized, "/")) return STORAGE_ERR_INVAL;
    if (find(normalized) >= 0) return STORAGE_ERR_EXIST;
    if (entry_count == STORAGE_MAX_ENTRIES || !parent_directory(normalized)) return STORAGE_ERR_NOTDIR;
    struct storage_entry *entry = &entries[entry_count]; zero(entry, sizeof(*entry)); text_copy(entry->path, normalized, sizeof(entry->path));
    const char *name = normalized; for (const char *p = normalized; *p; p++) if (*p == '/') name = p + 1;
    text_copy(entry->name, name, sizeof(entry->name)); entry->type = type; entry_count++;
    if (type == 'f' && content) { text_copy(entry->content, content, sizeof(entry->content)); while (entry->content[entry->size]) entry->size++; }
    return storage_sync();
}
int storage_create_entry(const char *path, char type, const char *content) { return create(path, type, content) == STORAGE_OK; }
int storage_mkdir(const char *path) { return create(path, 'd', 0); }
int storage_find_entry(const char *path) { return find(path); }
int storage_get_entry_count(void) { return entry_count; }
const char *storage_get_entry_name(int index) { return index >= 0 && index < entry_count ? entries[index].name : ""; }
const char *storage_get_entry_path(int index) { return index >= 0 && index < entry_count ? entries[index].path : ""; }
char storage_get_entry_type(int index) { return index >= 0 && index < entry_count ? entries[index].type : 0; }
const char *storage_get_entry_content(int index) { return index >= 0 && index < entry_count ? entries[index].content : ""; }

int storage_remove_entry(const char *path)
{
    int index = find(path); if (index < 0) return 0;
    int length = 0; while (entries[index].path[length]) length++;
    if (entries[index].type == 'd') for (int i = 0; i < entry_count; i++) if (i != index && path_prefix(entries[i].path, entries[index].path)) return 0;
    for (int i = 0; i < STORAGE_MAX_FILE_BLOCKS; i++) if (entries[index].blocks[i]) (void)bitmap_bit(entries[index].blocks[i], 0);
    for (int i = index; i + 1 < entry_count; i++) entries[i] = entries[i + 1];
    entry_count--;
    for (int i = 0; i < 16; i++) if (fds[i].used && fds[i].entry >= index) fds[i].used = 0;
    return storage_sync() == STORAGE_OK;
}
int storage_write_file(const char *path, const void *buffer, size_t length, int append)
{
    int index = find(path); if (index < 0) { if (create(path, 'f', 0) != STORAGE_OK) return STORAGE_ERR_NOENT; index = find(path); }
    if (index < 0 || entries[index].type != 'f' || (!buffer && length)) return STORAGE_ERR_INVAL;
    if (!append) entries[index].size = 0;
    if (entries[index].size + length >= sizeof(entries[index].content)) return STORAGE_ERR_NOSPC;
    copy(entries[index].content + entries[index].size, buffer, length); entries[index].size += length; entries[index].content[entries[index].size] = 0;
    return storage_sync() == STORAGE_OK ? (int)length : STORAGE_ERR_IO;
}
int storage_read_file(const char *path, void *buffer, size_t capacity, size_t *length)
{
    int index = find(path); if (index < 0) return STORAGE_ERR_NOENT;
    if (entries[index].type != 'f' || (!buffer && capacity)) return STORAGE_ERR_ISDIR;
    size_t count = entries[index].size < capacity ? entries[index].size : capacity; copy(buffer, entries[index].content, count); if (length) *length = entries[index].size; return (int)count;
}
int storage_rename(const char *source, const char *destination)
{
    int index = find(source); char normalized[STORAGE_MAX_PATH];
    if (index < 0 || normalize(destination, normalized) != STORAGE_OK || find(normalized) >= 0 || !parent_directory(normalized)) return STORAGE_ERR_INVAL;
    text_copy(entries[index].path, normalized, sizeof(entries[index].path)); const char *name = normalized; for (const char *p = normalized; *p; p++) if (*p == '/') name = p + 1; text_copy(entries[index].name, name, sizeof(entries[index].name)); return storage_sync();
}
int storage_copy(const char *source, const char *destination)
{
    int index = find(source); if (index < 0) return STORAGE_ERR_NOENT;
    if (entries[index].type == 'd') return storage_mkdir(destination) ? STORAGE_OK : STORAGE_ERR_IO;
    return storage_write_file(destination, entries[index].content, entries[index].size, 0) < 0 ? STORAGE_ERR_IO : STORAGE_OK;
}
int storage_open(const char *path, int create_if_missing)
{
    int entry = find(path); if (entry < 0 && create_if_missing) { if (create(path, 'f', 0) != STORAGE_OK) return STORAGE_ERR_IO; entry = find(path); }
    if (entry < 0) return STORAGE_ERR_NOENT;
    if (entries[entry].type != 'f') return STORAGE_ERR_ISDIR;
    for (int fd = 0; fd < 16; fd++) if (!fds[fd].used) { fds[fd].used = 1; fds[fd].entry = entry; fds[fd].offset = 0; return fd; }
    return STORAGE_ERR_NOSPC;
}
int storage_close(int fd) { if (fd < 0 || fd >= 16 || !fds[fd].used) return STORAGE_ERR_BADFD; fds[fd].used = 0; return STORAGE_OK; }
int storage_read(int fd, void *buffer, size_t length) { if (fd < 0 || fd >= 16 || !fds[fd].used || (!buffer && length)) return STORAGE_ERR_BADFD; size_t available = fds[fd].offset < entries[fds[fd].entry].size ? entries[fds[fd].entry].size - fds[fd].offset : 0; if (length > available) length = available; copy(buffer, entries[fds[fd].entry].content + fds[fd].offset, length); fds[fd].offset += length; return (int)length; }
int storage_write(int fd, const void *buffer, size_t length) { if (fd < 0 || fd >= 16 || !fds[fd].used || (!buffer && length)) return STORAGE_ERR_BADFD; if (fds[fd].offset + length >= sizeof(entries[fds[fd].entry].content)) return STORAGE_ERR_NOSPC; copy(entries[fds[fd].entry].content + fds[fd].offset, buffer, length); fds[fd].offset += length; if (fds[fd].offset > entries[fds[fd].entry].size) entries[fds[fd].entry].size = fds[fd].offset; entries[fds[fd].entry].content[entries[fds[fd].entry].size] = 0; return storage_sync() == STORAGE_OK ? (int)length : STORAGE_ERR_IO; }
int storage_seek(int fd, size_t offset) { if (fd < 0 || fd >= 16 || !fds[fd].used || offset >= sizeof(entries[0].content)) return STORAGE_ERR_BADFD; fds[fd].offset = offset; return STORAGE_OK; }

int storage_get_device_info(struct storage_device_info *info) { if (!info || !storage_device) return STORAGE_ERR_NOENT; info->name = storage_device->name ? storage_device->name : "disk0"; info->type = storage_device->type ? storage_device->type : "ATA"; info->sectors = storage_device->sector_count; info->sector_size = BLOCK_SECTOR_SIZE; info->present = 1; info->mounted = mounted; return STORAGE_OK; }
int storage_get_stats(struct storage_stats *stats) { if (!stats || !mounted) return STORAGE_ERR_NOTMOUNTED; stats->total_sectors = storage_device ? storage_device->sector_count : 0; stats->used_sectors = storage_device ? data_start : 0; stats->sector_size = BLOCK_SECTOR_SIZE; stats->file_count = 0; stats->directory_count = 0; for (int i = 0; i < entry_count; i++) { if (entries[i].type == 'f') { stats->file_count++; if (entries[i].blocks[0]) stats->used_sectors++; } else stats->directory_count++; } stats->free_sectors = stats->total_sectors > stats->used_sectors ? stats->total_sectors - stats->used_sectors : 0; return STORAGE_OK; }
static int count_allocated_data_sectors(uint64_t *count)
{
    unsigned char block[BLOCK_SECTOR_SIZE];
    uint64_t allocated = 0;
    for (uint64_t bitmap_index = 0; bitmap_index < bitmap_sectors; bitmap_index++) {
        uint64_t bitmap_sector = STORAGE_BITMAP_START + bitmap_index;
        uint64_t first_sector = bitmap_index * STORAGE_BITMAP_BITS_PER_SECTOR;
        uint64_t end_sector = storage_device->sector_count - first_sector < STORAGE_BITMAP_BITS_PER_SECTOR
            ? storage_device->sector_count : first_sector + STORAGE_BITMAP_BITS_PER_SECTOR;
        if (disk_read(bitmap_sector, block) != STORAGE_OK) return STORAGE_ERR_IO;
        if (first_sector < data_start) first_sector = data_start;
        for (uint64_t sector = first_sector; sector < end_sector; sector++) {
            unsigned int bit = (unsigned int)(sector % STORAGE_BITMAP_BITS_PER_SECTOR);
            if ((block[bit / 8] >> (bit % 8)) & 1) allocated++;
        }
    }
    *count = allocated;
    return STORAGE_OK;
}

int storage_fsck(int repair, int *errors)
{
    unsigned char super[BLOCK_SECTOR_SIZE];
    uint64_t expected_bitmap, expected_data_start, allocated_sectors;
    uint64_t referenced_sectors = 0;
    int unresolved = 0, corrected = 0;

    if (errors) *errors = 0;
    if (!mounted) return STORAGE_ERR_NOTMOUNTED;
    if (!storage_device || disk_read(0, super) != STORAGE_OK) return STORAGE_ERR_IO;

    expected_bitmap = storage_device->sector_count / STORAGE_BITMAP_BITS_PER_SECTOR;
    if (storage_device->sector_count % STORAGE_BITMAP_BITS_PER_SECTOR) expected_bitmap++;
    expected_data_start = STORAGE_BITMAP_START + expected_bitmap;
    if (get32(super) != STORAGE_MAGIC || get32(super + 4) != STORAGE_VERSION ||
        get32(super + 8) != (uint32_t)entry_count || get64(super + 16) != storage_device->sector_count ||
        get64(super + 24) != expected_bitmap || get64(super + 32) != expected_data_start ||
        bitmap_sectors != expected_bitmap || data_start != expected_data_start ||
        entry_count < 0 || entry_count > STORAGE_MAX_ENTRIES || data_start >= storage_device->sector_count) {
        if (errors) *errors = 1;
        return STORAGE_ERR_CORRUPT;
    }

    for (int i = 0; i < entry_count; i++) {
        struct storage_entry *entry = &entries[i];
        char normalized[STORAGE_MAX_PATH];
        int valid_path = normalize(entry->path, normalized) == STORAGE_OK && equal(normalized, entry->path);
        const char *name = entry->path;
        for (const char *p = entry->path; *p; p++) if (*p == '/') name = p + 1;
        if (!valid_path || equal(entry->path, "/") || !parent_exists(entry->path) ||
            (entry->type != 'f' && entry->type != 'd') || !equal(name, entry->name) ||
            (entry->type == 'f' && entry->size >= sizeof(entry->content)) ||
            (entry->type == 'd' && entry->size != 0)) unresolved++;

        for (int j = i + 1; j < entry_count; j++) {
            if (equal(entry->path, entries[j].path)) unresolved++;
            if (entry->type == 'f' && entries[j].type == 'f' && entry->blocks[0] &&
                entry->blocks[0] == entries[j].blocks[0]) unresolved++;
        }

        for (int block_index = 1; block_index < STORAGE_MAX_FILE_BLOCKS; block_index++)
            if (entry->blocks[block_index]) unresolved++;
        if (entry->type == 'd' && entry->blocks[0]) unresolved++;
        if (entry->type != 'f') continue;
        if (entry->size && !entry->blocks[0]) { unresolved++; continue; }
        if (!entry->blocks[0]) continue;
        if (entry->blocks[0] < data_start || entry->blocks[0] >= storage_device->sector_count) {
            unresolved++;
            continue;
        }
        referenced_sectors++;
        int is_allocated = 0;
        if (bitmap_value(entry->blocks[0], &is_allocated) != STORAGE_OK) return STORAGE_ERR_IO;
        if (!is_allocated) {
            if (repair && bitmap_bit(entry->blocks[0], 1) == STORAGE_OK) corrected++;
            else unresolved++;
        }
    }

    for (uint64_t sector = 0; sector < data_start; sector++) {
        int is_allocated = 0;
        if (bitmap_value(sector, &is_allocated) != STORAGE_OK) return STORAGE_ERR_IO;
        if (!is_allocated) {
            if (repair && bitmap_bit(sector, 1) == STORAGE_OK) corrected++;
            else unresolved++;
        }
    }

    if (count_allocated_data_sectors(&allocated_sectors) != STORAGE_OK) return STORAGE_ERR_IO;
    if (allocated_sectors > referenced_sectors) unresolved++;
    else if (allocated_sectors < referenced_sectors) unresolved++;
    if (corrected && storage_sync() != STORAGE_OK) return STORAGE_ERR_IO;
    if (errors) *errors = unresolved;
    return unresolved ? STORAGE_ERR_CORRUPT : STORAGE_OK;
}

unsigned int storage_self_test(void)
{
    unsigned int passed = 0;
    unsigned char block[BLOCK_SECTOR_SIZE], readback[BLOCK_SECTOR_SIZE];
    char file_data[32] = {0};
    size_t file_length = 0;
    int errors = 0;

    copy(selftest_saved_state.entries, entries, sizeof(entries));
    copy(selftest_saved_state.fds, fds, sizeof(fds));
    copy(selftest_saved_state.cache, cache, sizeof(cache));
    selftest_saved_state.device = storage_device;
    selftest_saved_state.bitmap_sectors = bitmap_sectors;
    selftest_saved_state.data_start = data_start;
    selftest_saved_state.cache_next_slot = cache_next_slot;
    selftest_saved_state.entry_count = entry_count;
    selftest_saved_state.mounted = mounted;

    zero(selftest_disk, sizeof(selftest_disk));
    selftest_device.context = selftest_disk;
    selftest_device.sector_count = STORAGE_SELFTEST_SECTORS;
    selftest_device.name = "ramtest0";
    selftest_device.type = "RAM test device";
    selftest_device.read_sector = selftest_read_sector;
    selftest_device.write_sector = selftest_write_sector;

    if (storage_attach_block_device(&selftest_device) == STORAGE_OK) passed |= STORAGE_SELFTEST_DEVICE;
    struct storage_device_info info;
    if (storage_get_device_info(&info) == STORAGE_OK && info.present &&
        info.sectors == STORAGE_SELFTEST_SECTORS && info.sector_size == BLOCK_SECTOR_SIZE)
        passed |= STORAGE_SELFTEST_INFO;

    int io_ok = 1;
    for (uint64_t sector = 67; sector <= 71; sector++) {
        zero(block, sizeof(block));
        block[0] = (unsigned char)sector;
        if (storage_write_blocks(&selftest_device, sector, block, 1) != STORAGE_OK ||
            disk_read(sector, readback) != STORAGE_OK || readback[0] != (unsigned char)sector)
            io_ok = 0;
    }
    selftest_disk[67 * BLOCK_SECTOR_SIZE] = 0;
    if (disk_read(67, readback) != STORAGE_OK || readback[0] != 0) io_ok = 0;
    selftest_disk[71 * BLOCK_SECTOR_SIZE] = 0;
    if (disk_read(71, readback) != STORAGE_OK || readback[0] != 71) io_ok = 0;
    zero(block, sizeof(block)); block[0] = 0xa5;
    if (disk_write(71, block) != STORAGE_OK || selftest_disk[71 * BLOCK_SECTOR_SIZE] != 0xa5 ||
        disk_read(71, readback) != STORAGE_OK || readback[0] != 0xa5) io_ok = 0;
    if (storage_read_blocks(&selftest_device, STORAGE_SELFTEST_SECTORS, block, 1) != STORAGE_ERR_INVAL ||
        storage_write_blocks(&selftest_device, STORAGE_SELFTEST_SECTORS, block, 1) != STORAGE_ERR_INVAL)
        io_ok = 0;
    if (io_ok) passed |= STORAGE_SELFTEST_BLOCK_IO | STORAGE_SELFTEST_CACHE;

    struct block_device failing_device = {
        0, STORAGE_SELFTEST_SECTORS, "failure-test", "test",
        selftest_fail_read, selftest_fail_write
    };
    if (storage_attach_block_device(0) == STORAGE_ERR_INVAL &&
        storage_read_blocks(&failing_device, 0, block, 1) == STORAGE_ERR_IO &&
        storage_write_blocks(&failing_device, 0, block, 1) == STORAGE_ERR_IO)
        passed |= STORAGE_SELFTEST_ERRORS;

    int format_ok = storage_format() == STORAGE_OK && storage_is_mounted();
    if (format_ok && storage_unmount() == STORAGE_OK && !storage_is_mounted() &&
        storage_mount() == STORAGE_OK && storage_is_mounted()) passed |= STORAGE_SELFTEST_MOUNT;

    int files_ok = storage_mkdir("/selftest") == STORAGE_OK &&
        storage_write_file("/selftest/note", "RAM-only test", 13, 0) == 13 &&
        storage_read_file("/selftest/note", file_data, sizeof(file_data), &file_length) == 13 &&
        file_length == 13 && equal(file_data, "RAM-only test");
    if (files_ok) passed |= STORAGE_SELFTEST_FILES;

    struct storage_stats stats;
    if (storage_get_stats(&stats) == STORAGE_OK && stats.total_sectors == STORAGE_SELFTEST_SECTORS &&
        stats.free_sectors > 0 && stats.used_sectors + stats.free_sectors == stats.total_sectors)
        passed |= STORAGE_SELFTEST_SPACE;

    int fsck_ok = 0;
    if (storage_write_file("/selftest/probe", "check", 5, 0) == 5) {
        int probe = find("/selftest/probe");
        if (probe >= 0 && entries[probe].blocks[0] && storage_fsck(0, &errors) == STORAGE_OK) {
            uint64_t sector = entries[probe].blocks[0];
            uint64_t bitmap_sector = STORAGE_BITMAP_START + sector / STORAGE_BITMAP_BITS_PER_SECTOR;
            unsigned int bit = (unsigned int)(sector % STORAGE_BITMAP_BITS_PER_SECTOR);
            selftest_disk[bitmap_sector * BLOCK_SECTOR_SIZE + bit / 8] &= (unsigned char)~(1u << (bit % 8));
            if (storage_attach_block_device(&selftest_device) == STORAGE_OK && storage_mount() == STORAGE_OK &&
                storage_fsck(0, &errors) == STORAGE_ERR_CORRUPT && errors > 0 &&
                storage_fsck(1, &errors) == STORAGE_OK && errors == 0 &&
                storage_fsck(0, &errors) == STORAGE_OK) fsck_ok = 1;
        }
    }
    if (fsck_ok) passed |= STORAGE_SELFTEST_FSCK;

    copy(entries, selftest_saved_state.entries, sizeof(entries));
    copy(fds, selftest_saved_state.fds, sizeof(fds));
    copy(cache, selftest_saved_state.cache, sizeof(cache));
    storage_device = selftest_saved_state.device;
    bitmap_sectors = selftest_saved_state.bitmap_sectors;
    data_start = selftest_saved_state.data_start;
    cache_next_slot = selftest_saved_state.cache_next_slot;
    entry_count = selftest_saved_state.entry_count;
    mounted = selftest_saved_state.mounted;
    return passed;
}
