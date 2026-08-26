#include "storage.h"

#define STORAGE_MAX_ENTRIES 32
#define STORAGE_NAME_LEN 16
#define STORAGE_CONTENT_LEN 256
#define STORAGE_MAX_FDS 16

struct storage_entry { char path[STORAGE_MAX_PATH]; char name[STORAGE_NAME_LEN]; char type; char content[STORAGE_CONTENT_LEN]; size_t size; };
struct storage_fd { int entry; size_t offset; int used; };
static struct storage_entry entries[STORAGE_MAX_ENTRIES];
static struct storage_fd fds[STORAGE_MAX_FDS];
static int entry_count;
static int create(const char *path, char type, const char *content);

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
    (void)create("/bin", 'd', 0);
    (void)create("/README", 'f', "ORT kernel shell\n");
}

int storage_create_entry(const char *path, char type, const char *content) { return create(path, type, content) == STORAGE_OK; }
int storage_mkdir(const char *path) { return create(path, 'd', 0); }

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
    return (int)length;
}
int storage_seek(int fd, size_t offset) { if (fd < 0 || fd >= STORAGE_MAX_FDS || !fds[fd].used) return STORAGE_ERR_BADFD; if (offset >= STORAGE_CONTENT_LEN) return STORAGE_ERR_INVAL; fds[fd].offset = offset; return STORAGE_OK; }
