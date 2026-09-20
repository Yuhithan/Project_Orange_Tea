#include "storage.h"

#define STORAGE_MAX_ENTRIES 32
#define STORAGE_NAME_LEN 16
#define STORAGE_CONTENT_LEN 256
#define STORAGE_MAX_FDS 16
#define STORAGE_DISK_MAGIC 0x5346524fu
#define STORAGE_DISK_VERSION 1u
#define STORAGE_DISK_RECORD_SIZE 512u

struct storage_entry { char path[STORAGE_MAX_PATH]; char name[STORAGE_NAME_LEN]; char type; char content[STORAGE_CONTENT_LEN]; size_t size; };
struct storage_fd { int entry; size_t offset; int used; };
static struct storage_entry entries[STORAGE_MAX_ENTRIES];
static struct storage_fd fds[STORAGE_MAX_FDS];
static int entry_count;
static const struct block_device *storage_device;
static int create(const char *path, char type, const char *content);

static void disk_copy(unsigned char *out, const void *in, size_t length)
{
    const unsigned char *bytes = in;
    for (size_t i = 0; i < length; i++) out[i] = bytes[i];
}

static int disk_load(void)
{
    unsigned char sector[BLOCK_SECTOR_SIZE];
    uint32_t magic;
    uint16_t version;
    uint16_t count;
    if (!storage_device || storage_device->read_sector(storage_device->context, 0, sector) != 0) return STORAGE_ERR_IO;
    disk_copy((unsigned char *)&magic, sector, sizeof(magic));
    disk_copy((unsigned char *)&version, sector + 4, sizeof(version));
    disk_copy((unsigned char *)&count, sector + 6, sizeof(count));
    if (magic != STORAGE_DISK_MAGIC || version != STORAGE_DISK_VERSION || count > STORAGE_MAX_ENTRIES ||
        storage_device->sector_count < (uint64_t)count + 1) return STORAGE_ERR_NOENT;
    entry_count = 0;
    for (int i = 0; i < count; i++) {
        struct storage_entry *entry = &entries[i];
        uint64_t size;
        if (storage_device->read_sector(storage_device->context, (uint64_t)i + 1, sector) != 0) return STORAGE_ERR_IO;
        disk_copy((unsigned char *)entry->path, sector, sizeof(entry->path));
        disk_copy((unsigned char *)entry->name, sector + 64, sizeof(entry->name));
        entry->type = (char)sector[80];
        disk_copy((unsigned char *)&size, sector + 88, sizeof(size));
        if ((entry->type != 'f' && entry->type != 'd') || size >= STORAGE_CONTENT_LEN) return STORAGE_ERR_INVAL;
        entry->size = (size_t)size;
        disk_copy((unsigned char *)entry->content, sector + 96, sizeof(entry->content));
        entry->content[entry->size] = 0;
        entry_count++;
    }
    return STORAGE_OK;
}

static int equal(const char *a, const char *b) { while (*a && *b) if (*a++ != *b++) return 0; return *a == *b; }
static void copy(char *out, const char *in, int max) { int i = 0; while (in && in[i] && i + 1 < max) { out[i] = in[i]; i++; } out[i] = 0; }

/* Canonicalizes /, repeated slashes, . and .. without permitting overflow. */
static int normalize(const char *path, char out[STORAGE_MAX_PATH])
{
    int length = 1, starts[32], components = 0;
    if (!path || !*path) return STORAGE_ERR_INVAL;
    out[0] = '/'; out[1] = 0;
    for (int i = 0; path[i]; ) {
        while (path[i] == '/') i++;
        if (!path[i]) break;
        int first = i; while (path[i] && path[i] != '/') i++;
        int count = i - first;
        if (count == 1 && path[first] == '.') continue;
        if (count == 2 && path[first] == '.' && path[first + 1] == '.') {
            if (components) { length = starts[--components]; out[length] = 0; }
            continue;
        }
        if (count >= STORAGE_NAME_LEN || components == 32 || length + count + 1 >= STORAGE_MAX_PATH) return STORAGE_ERR_INVAL;
        starts[components++] = length;
        if (length > 1) out[length++] = '/';
        for (int j = 0; j < count; j++) out[length++] = path[first + j];
        out[length] = 0;
    }
    return STORAGE_OK;
}

int storage_find_entry(const char *path)
{
    char normalized[STORAGE_MAX_PATH];
    if (normalize(path, normalized) != STORAGE_OK) return -1;
    for (int i = 0; i < entry_count; i++) if (equal(entries[i].path, normalized)) return i;
    return -1;
}

static int parent_is_directory(const char *path)
{
    char parent[STORAGE_MAX_PATH]; copy(parent, path, sizeof(parent));
    int i = 0; while (parent[i]) i++;
    while (i > 1 && parent[i - 1] != '/') i--;
    if (i <= 1) return 1;
    parent[i - 1] = 0;
    int index = storage_find_entry(parent);
    if (index < 0) return create(parent, 'd', 0) == STORAGE_OK;
    return entries[index].type == 'd';
}

static int create(const char *path, char type, const char *content)
{
    char normalized[STORAGE_MAX_PATH];
    if (normalize(path, normalized) != STORAGE_OK || (type != 'f' && type != 'd') || equal(normalized, "/")) return STORAGE_ERR_INVAL;
    if (storage_find_entry(normalized) >= 0) return STORAGE_ERR_EXIST;
    if (entry_count == STORAGE_MAX_ENTRIES) return STORAGE_ERR_NOSPC;
    if (!parent_is_directory(normalized)) return STORAGE_ERR_NOTDIR;
    struct storage_entry *entry = &entries[entry_count++];
    copy(entry->path, normalized, sizeof(entry->path));
    const char *name = normalized; for (const char *p = normalized; *p; p++) if (*p == '/') name = p + 1;
    copy(entry->name, name, sizeof(entry->name)); entry->type = type; entry->size = 0; entry->content[0] = 0;
    if (type == 'f' && content) while (content[entry->size] && entry->size + 1 < sizeof(entry->content)) { entry->content[entry->size] = content[entry->size]; entry->size++; }
    entry->content[entry->size] = 0;
    return STORAGE_OK;
}

void storage_init(void)
{
    if (entry_count) return;
    for (int i = 0; i < STORAGE_MAX_FDS; i++) fds[i].used = 0;
    if (storage_device && disk_load() == STORAGE_OK) return;
    (void)create("/bin", 'd', 0);
    (void)create("/README", 'f', "ORT kernel shell\n");
    (void)storage_sync();
}

int storage_attach_block_device(const struct block_device *device)
{
    if (!device || !device->read_sector || !device->write_sector || device->sector_count == 0) return STORAGE_ERR_INVAL;
    storage_device = device;
    return STORAGE_OK;
}

int storage_sync(void)
{
    unsigned char sector[BLOCK_SECTOR_SIZE];
    if (!storage_device) return STORAGE_ERR_NOENT;
    for (size_t i = 0; i < BLOCK_SECTOR_SIZE; i++) sector[i] = 0;
    disk_copy(sector, &(uint32_t){ STORAGE_DISK_MAGIC }, sizeof(uint32_t));
    disk_copy(sector + 4, &(uint16_t){ STORAGE_DISK_VERSION }, sizeof(uint16_t));
    disk_copy(sector + 6, &(uint16_t){ (uint16_t)entry_count }, sizeof(uint16_t));
    if (storage_device->write_sector(storage_device->context, 0, sector) != 0) return STORAGE_ERR_IO;
    for (int i = 0; i < entry_count; i++) {
        const struct storage_entry *entry = &entries[i];
        uint64_t size = entry->size;
        for (size_t j = 0; j < BLOCK_SECTOR_SIZE; j++) sector[j] = 0;
        disk_copy(sector, entry->path, sizeof(entry->path));
        disk_copy(sector + 64, entry->name, sizeof(entry->name));
        sector[80] = (unsigned char)entry->type;
        disk_copy(sector + 88, &size, sizeof(size));
        disk_copy(sector + 96, entry->content, sizeof(entry->content));
        if (storage_device->write_sector(storage_device->context, (uint64_t)i + 1, sector) != 0) return STORAGE_ERR_IO;
    }
    return STORAGE_OK;
}

int storage_create_entry(const char *path, char type, const char *content)
{
    int result = create(path, type, content);
    if (result == STORAGE_OK && storage_device && storage_sync() != STORAGE_OK) return 0;
    return result == STORAGE_OK;
}

int storage_mkdir(const char *path)
{
    int result = create(path, 'd', 0);
    if (result == STORAGE_OK && storage_device && storage_sync() != STORAGE_OK) return STORAGE_ERR_IO;
    return result;
}

int storage_remove_entry(const char *path)
{
    int index = storage_find_entry(path);
    if (index < 0) return 0;
    int prefix = 0; while (entries[index].path[prefix]) prefix++;
    if (entries[index].type == 'd') for (int i = 0; i < entry_count; i++) {
        int j = 0; while (j < prefix && entries[i].path[j] == entries[index].path[j]) j++;
        if (i != index && j == prefix && entries[i].path[j] == '/') return 0;
    }
    for (int fd = 0; fd < STORAGE_MAX_FDS; fd++) if (fds[fd].used && fds[fd].entry == index) fds[fd].used = 0;
    for (int i = index; i + 1 < entry_count; i++) entries[i] = entries[i + 1];
    entry_count--;
    for (int fd = 0; fd < STORAGE_MAX_FDS; fd++) if (fds[fd].used && fds[fd].entry > index) fds[fd].entry--;
    if (storage_device && storage_sync() != STORAGE_OK) return 0;
    return 1;
}

int storage_get_entry_count(void) { return entry_count; }
const char *storage_get_entry_name(int index) { return index >= 0 && index < entry_count ? entries[index].name : ""; }
const char *storage_get_entry_path(int index) { return index >= 0 && index < entry_count ? entries[index].path : ""; }
char storage_get_entry_type(int index) { return index >= 0 && index < entry_count ? entries[index].type : 0; }
const char *storage_get_entry_content(int index) { return index >= 0 && index < entry_count ? entries[index].content : ""; }

int storage_open(const char *path, int create_if_missing)
{
    int entry = storage_find_entry(path);
    if (entry < 0 && create_if_missing) { int result = create(path, 'f', 0); if (result != STORAGE_OK) return result; entry = storage_find_entry(path); }
    if (entry < 0) return STORAGE_ERR_NOENT;
    if (entries[entry].type != 'f') return STORAGE_ERR_ISDIR;
    for (int fd = 0; fd < STORAGE_MAX_FDS; fd++) if (!fds[fd].used) { fds[fd].used = 1; fds[fd].entry = entry; fds[fd].offset = 0; return fd; }
    return STORAGE_ERR_NOSPC;
}

int storage_close(int fd) { if (fd < 0 || fd >= STORAGE_MAX_FDS || !fds[fd].used) return STORAGE_ERR_BADFD; fds[fd].used = 0; return STORAGE_OK; }
int storage_read(int fd, void *buffer, size_t length)
{
    if (fd < 0 || fd >= STORAGE_MAX_FDS || !fds[fd].used || (!buffer && length)) return STORAGE_ERR_BADFD;
    struct storage_entry *entry = &entries[fds[fd].entry];
    size_t available = fds[fd].offset < entry->size ? entry->size - fds[fd].offset : 0;
    if (length > available) length = available;
    char *out = buffer; for (size_t i = 0; i < length; i++) out[i] = entry->content[fds[fd].offset + i];
    fds[fd].offset += length; return (int)length;
}
int storage_write(int fd, const void *buffer, size_t length)
{
    if (fd < 0 || fd >= STORAGE_MAX_FDS || !fds[fd].used || (!buffer && length)) return STORAGE_ERR_BADFD;
    if (fds[fd].offset >= STORAGE_CONTENT_LEN - 1) return length ? STORAGE_ERR_NOSPC : 0;
    if (length > STORAGE_CONTENT_LEN - 1 - fds[fd].offset) length = STORAGE_CONTENT_LEN - 1 - fds[fd].offset;
    const char *in = buffer; struct storage_entry *entry = &entries[fds[fd].entry];
    for (size_t i = 0; i < length; i++) entry->content[fds[fd].offset + i] = in[i];
    fds[fd].offset += length; if (fds[fd].offset > entry->size) entry->size = fds[fd].offset; entry->content[entry->size] = 0;
    if (storage_device && storage_sync() != STORAGE_OK) return STORAGE_ERR_IO;
    return (int)length;
}
int storage_seek(int fd, size_t offset) { if (fd < 0 || fd >= STORAGE_MAX_FDS || !fds[fd].used) return STORAGE_ERR_BADFD; if (offset >= STORAGE_CONTENT_LEN) return STORAGE_ERR_INVAL; fds[fd].offset = offset; return STORAGE_OK; }
