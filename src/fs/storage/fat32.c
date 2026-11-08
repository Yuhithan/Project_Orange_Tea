#include "fat32.h"

#define FAT32_SECTOR_SIZE 512u
#define FAT32_EOC 0x0fffffffu
#define FAT32_BAD 0x0ffffff7u
#define FAT32_MIN_CLUSTERS 65525u
#define FAT32_MAX_COMPONENTS 32
#define FAT32_INDEX_LIMIT 512
#define FAT32_MAX_CLUSTER_SECTORS 128
#define FAT32_MAX_LFN_ENTRIES 20

struct fat32_volume {
    const struct block_device *device;
    uint32_t partition_start;
    uint32_t volume_sectors;
    uint32_t fat_start;
    uint32_t fat_sectors;
    uint32_t data_start;
    uint32_t cluster_count;
    uint32_t root_cluster;
    uint32_t sectors_per_cluster;
    uint32_t bytes_per_cluster;
    uint16_t reserved_sectors;
    uint8_t fat_count;
    int mounted;
};

struct fat32_location {
    uint32_t sector;
    uint16_t offset;
};

struct fat32_found {
    char name[FAT32_MAX_NAME];
    char short_name[13];
    uint32_t cluster;
    uint32_t size;
    uint8_t attributes;
    uint16_t created_date;
    uint16_t modified_date;
    uint16_t modified_time;
    uint32_t parent_cluster;
    struct fat32_location location;
    int found;
};

struct fat32_index_entry {
    char path[FAT32_MAX_PATH];
    char name[FAT32_MAX_NAME];
    char type;
    uint32_t cluster;
    uint32_t size;
    uint8_t attributes;
    uint16_t created_date;
    uint16_t modified_date;
    uint16_t modified_time;
};

struct fat32_fd {
    char path[FAT32_MAX_PATH];
    uint32_t offset;
    int used;
};

struct fat32_dir_slot {
    uint32_t sector;
    uint16_t offset;
};

static struct fat32_volume volume;
static struct fat32_fd file_descriptors[16];
static struct fat32_index_entry index_entries[FAT32_INDEX_LIMIT];
static char remove_paths[FAT32_INDEX_LIMIT][FAT32_MAX_PATH];
static int index_count;
static int index_valid;
static char content_preview[256];
static uint32_t allocation_hint = 2;

static uint16_t read16(const uint8_t *p)
{
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}



static uint32_t read32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
        ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void write16(uint8_t *p, uint16_t value)
{
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8);
}

static void write32(uint8_t *p, uint32_t value)
{
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8);
    p[2] = (uint8_t)(value >> 16);
    p[3] = (uint8_t)(value >> 24);
}

static void bytes_zero(void *buffer, size_t length)
{
    uint8_t *p = buffer;
    while (length--) *p++ = 0;
}

static void bytes_copy(void *destination, const void *source, size_t length)
{
    uint8_t *out = destination;
    const uint8_t *in = source;
    while (length--) *out++ = *in++;
}

static size_t string_length(const char *text)
{
    size_t length = 0;
    while (text && text[length]) length++;
    return length;
}

static void string_copy(char *destination, const char *source, size_t capacity)
{
    size_t index = 0;
    if (!capacity) return;
    while (source && source[index] && index + 1 < capacity) {
        destination[index] = source[index];
        index++;
    }
    destination[index] = 0;
}

static uint8_t ascii_lower(uint8_t value)
{
    return value >= 'A' && value <= 'Z' ? (uint8_t)(value + ('a' - 'A')) : value;
}

static int path_has_prefix(const char *path, const char *prefix)
{
    while (*prefix && ascii_lower((uint8_t)*path) == ascii_lower((uint8_t)*prefix)) {
        path++;
        prefix++;
    }
    return !*prefix && *path == '/';
}

static int name_equal(const char *left, const char *right)
{
    while (*left && *right) {
        if (ascii_lower((uint8_t)*left) != ascii_lower((uint8_t)*right)) return 0;
        left++;
        right++;
    }
    return *left == *right;
}

static int device_read(uint32_t sector, void *buffer)
{
    if (!volume.device || sector >= volume.device->sector_count || !buffer) return STORAGE_ERR_IO;
    return volume.device->read_sector(volume.device->context, sector, buffer) == 0
        ? STORAGE_OK : STORAGE_ERR_IO;
}

static int device_write(uint32_t sector, const void *buffer)
{
    if (!volume.device || sector >= volume.device->sector_count || !buffer) return STORAGE_ERR_IO;
    return volume.device->write_sector(volume.device->context, sector, buffer) == 0
        ? STORAGE_OK : STORAGE_ERR_IO;
}

static int partition_from_mbr(const struct block_device *device, uint32_t *start, uint32_t *sectors)
{
    uint8_t mbr[FAT32_SECTOR_SIZE];
    if (!device || !start || !sectors || !device->read_sector ||
        device->read_sector(device->context, 0, mbr) != 0 ||
        mbr[510] != 0x55 || mbr[511] != 0xaa) return 0;

    for (unsigned int index = 0; index < 4; index++) {
        const uint8_t *partition = mbr + 446 + index * 16;
        uint8_t type = partition[4];
        uint32_t first = read32(partition + 8);
        uint32_t count = read32(partition + 12);
        if ((type == 0x0b || type == 0x0c) && first && count &&
            first < device->sector_count && count <= device->sector_count - first) {
            *start = first;
            *sectors = count;
            return 1;
        }
    }
    return 0;
}

static int volume_parameters(const struct block_device *device, uint32_t *start_out,
    uint32_t *sectors_out, uint8_t boot[FAT32_SECTOR_SIZE])
{
    uint32_t start = 0, partition_sectors = 0;
    if (!device || !device->read_sector || device->sector_count < FAT32_MIN_CLUSTERS) return STORAGE_ERR_NOENT;
    if (!partition_from_mbr(device, &start, &partition_sectors)) {
        if (device->read_sector(device->context, 0, boot) != 0) return STORAGE_ERR_IO;
        partition_sectors = (uint32_t)(device->sector_count > 0xffffffffu ? 0xffffffffu : device->sector_count);
    } else if (device->read_sector(device->context, start, boot) != 0) {
        return STORAGE_ERR_IO;
    }

    if (boot[510] != 0x55 || boot[511] != 0xaa ||
        read16(boot + 11) != FAT32_SECTOR_SIZE) return STORAGE_ERR_NOENT;

    uint32_t sectors_per_cluster = boot[13];
    uint32_t reserved = read16(boot + 14);
    uint32_t fats = boot[16];
    uint32_t root_entries = read16(boot + 17);
    uint32_t total16 = read16(boot + 19);
    uint32_t fat16 = read16(boot + 22);
    uint32_t total32 = read32(boot + 32);
    uint32_t fat32 = read32(boot + 36);
    uint32_t total = total16 ? total16 : total32;
    if (!sectors_per_cluster || (sectors_per_cluster & (sectors_per_cluster - 1)) ||
        sectors_per_cluster > FAT32_MAX_CLUSTER_SECTORS || !reserved ||
        (fats != 1 && fats != 2) || root_entries || fat16 || !fat32 ||
        !total || total > partition_sectors || total > device->sector_count - start ||
        (uint64_t)reserved + (uint64_t)fats * fat32 >= total) return STORAGE_ERR_CORRUPT;

    uint32_t data_sectors = total - reserved - fats * fat32;
    uint32_t clusters = data_sectors / sectors_per_cluster;
    if (clusters < FAT32_MIN_CLUSTERS || read32(boot + 44) < 2 ||
        read32(boot + 44) >= clusters + 2 ||
        (uint64_t)fat32 * FAT32_SECTOR_SIZE < ((uint64_t)clusters + 2) * 4) return STORAGE_ERR_CORRUPT;

    *start_out = start;
    *sectors_out = total;
    return STORAGE_OK;
}

static uint32_t cluster_sector(uint32_t cluster)
{
    return volume.partition_start + volume.data_start +
        (cluster - 2) * volume.sectors_per_cluster;
}

static int valid_cluster(uint32_t cluster)
{
    return cluster >= 2 && cluster < volume.cluster_count + 2;
}

static int fat_get(uint32_t cluster, uint32_t *value)
{
    uint8_t sector[FAT32_SECTOR_SIZE];
    uint32_t fat_offset = cluster * 4;
    if (!valid_cluster(cluster) || fat_offset / FAT32_SECTOR_SIZE >= volume.fat_sectors ||
        device_read(volume.partition_start + volume.fat_start +
            fat_offset / FAT32_SECTOR_SIZE, sector) != STORAGE_OK) return STORAGE_ERR_CORRUPT;
    *value = read32(sector + fat_offset % FAT32_SECTOR_SIZE) & 0x0fffffffu;
    return STORAGE_OK;
}

static int fat_set(uint32_t cluster, uint32_t value)
{
    uint8_t sector[FAT32_SECTOR_SIZE];
    uint32_t fat_offset = cluster * 4;
    if (!valid_cluster(cluster) || fat_offset / FAT32_SECTOR_SIZE >= volume.fat_sectors)
        return STORAGE_ERR_CORRUPT;
    for (uint32_t copy = 0; copy < volume.fat_count; copy++) {
        uint32_t sector_number = volume.partition_start + volume.fat_start +
            copy * volume.fat_sectors + fat_offset / FAT32_SECTOR_SIZE;
        if (device_read(sector_number, sector) != STORAGE_OK) return STORAGE_ERR_IO;
        write32(sector + fat_offset % FAT32_SECTOR_SIZE, value & 0x0fffffffu);
        if (device_write(sector_number, sector) != STORAGE_OK) return STORAGE_ERR_IO;
    }
    return STORAGE_OK;
}

static int cluster_next(uint32_t cluster, uint32_t *next)
{
    int result = fat_get(cluster, next);
    if (result != STORAGE_OK) return result;
    if (*next >= 0x0ffffff8u) return 1;
    if (*next == FAT32_BAD || !valid_cluster(*next)) return STORAGE_ERR_CORRUPT;
    return STORAGE_OK;
}

static int allocate_cluster(uint32_t *result)
{
    uint8_t sector[FAT32_SECTOR_SIZE];
    if (!result) return STORAGE_ERR_INVAL;
    uint32_t first = allocation_hint;
    if (!valid_cluster(first)) first = 2;
    for (uint32_t pass = 0; pass < 2; pass++) {
        uint32_t begin = pass == 0 ? first : 2;
        uint32_t end = pass == 0 ? volume.cluster_count + 2 : first;
        for (uint32_t cluster = begin; cluster < end; cluster++) {
            uint32_t current;
            if (fat_get(cluster, &current) != STORAGE_OK) return STORAGE_ERR_IO;
            if (current != 0) continue;
            if (fat_set(cluster, FAT32_EOC) != STORAGE_OK) return STORAGE_ERR_IO;
            bytes_zero(sector, sizeof(sector));
            uint32_t first_sector = cluster_sector(cluster);
            for (uint32_t index = 0; index < volume.sectors_per_cluster; index++)
                if (device_write(first_sector + index, sector) != STORAGE_OK) return STORAGE_ERR_IO;
            *result = cluster;
            allocation_hint = cluster + 1;
            if (!valid_cluster(allocation_hint)) allocation_hint = 2;
            return STORAGE_OK;
        }
    }
    return STORAGE_ERR_NOSPC;
}

static int free_chain(uint32_t cluster)
{
    for (uint32_t count = 0; valid_cluster(cluster) && count <= volume.cluster_count; count++) {
        uint32_t next;
        int result = cluster_next(cluster, &next);
        if (result < 0) return result;
        if (fat_set(cluster, 0) != STORAGE_OK) return STORAGE_ERR_IO;
        if (result == 1) return STORAGE_OK;
        cluster = next;
    }
    return STORAGE_ERR_CORRUPT;
}

static int get_nth_cluster(uint32_t first, uint32_t index, int allocate, uint32_t *result)
{
    if (!first && allocate) {
        if (allocate_cluster(&first) != STORAGE_OK) return STORAGE_ERR_NOSPC;
    }
    if (!valid_cluster(first)) return STORAGE_ERR_NOENT;
    uint32_t cluster = first;
    for (uint32_t count = 0; count < index; count++) {
        uint32_t next;
        int status = cluster_next(cluster, &next);
        if (status == 1 && allocate) {
            if (allocate_cluster(&next) != STORAGE_OK || fat_set(cluster, next) != STORAGE_OK)
                return STORAGE_ERR_NOSPC;
        } else if (status != STORAGE_OK) {
            return status == 1 ? STORAGE_ERR_NOENT : status;
        }
        cluster = next;
    }
    *result = cluster;
    return STORAGE_OK;
}

static int utf8_next(const uint8_t *text, size_t length, size_t *position, uint32_t *codepoint)
{
    if (*position >= length) return 0;
    uint8_t first = text[(*position)++];
    if (first < 0x80) {
        *codepoint = first;
        return 1;
    }
    unsigned int extra;
    uint32_t value;
    if ((first & 0xe0) == 0xc0) { extra = 1; value = first & 0x1f; }
    else if ((first & 0xf0) == 0xe0) { extra = 2; value = first & 0x0f; }
    else if ((first & 0xf8) == 0xf0) { extra = 3; value = first & 0x07; }
    else { *codepoint = '?'; return 1; }
    if (*position + extra > length) { *codepoint = '?'; *position = length; return 1; }
    for (unsigned int index = 0; index < extra; index++) {
        uint8_t next = text[(*position)++];
        if ((next & 0xc0) != 0x80) { *codepoint = '?'; return 1; }
        value = (value << 6) | (next & 0x3f);
    }
    *codepoint = value;
    return 1;
}

static int utf8_to_utf16(const char *text, uint16_t output[255], size_t *units)
{
    size_t length = string_length(text), position = 0, count = 0;
    while (position < length) {
        uint32_t codepoint;
        utf8_next((const uint8_t *)text, length, &position, &codepoint);
        if (codepoint == 0 || codepoint > 0x10ffff ||
            (codepoint >= 0xd800 && codepoint <= 0xdfff)) codepoint = '?';
        if (codepoint < 0x10000) {
            if (count == 255) return STORAGE_ERR_INVAL;
            output[count++] = (uint16_t)codepoint;
        } else {
            if (count > 253) return STORAGE_ERR_INVAL;
            codepoint -= 0x10000;
            output[count++] = (uint16_t)(0xd800 | (codepoint >> 10));
            output[count++] = (uint16_t)(0xdc00 | (codepoint & 0x3ff));
        }
    }
    if (!count) return STORAGE_ERR_INVAL;
    *units = count;
    return STORAGE_OK;
}

static size_t utf16_to_utf8(const uint16_t *input, size_t units, char *output, size_t capacity)
{
    size_t out = 0;
    for (size_t index = 0; index < units && out + 4 < capacity; index++) {
        uint32_t codepoint = input[index];
        if (codepoint == 0 || codepoint == 0xffff) break;
        if (codepoint >= 0xd800 && codepoint <= 0xdbff &&
            index + 1 < units && input[index + 1] >= 0xdc00 && input[index + 1] <= 0xdfff) {
            codepoint = 0x10000 + ((codepoint - 0xd800) << 10) +
                (input[++index] - 0xdc00);
        }
        if (codepoint < 0x80) output[out++] = (char)codepoint;
        else if (codepoint < 0x800) {
            output[out++] = (char)(0xc0 | (codepoint >> 6));
            output[out++] = (char)(0x80 | (codepoint & 0x3f));
        } else if (codepoint < 0x10000) {
            output[out++] = (char)(0xe0 | (codepoint >> 12));
            output[out++] = (char)(0x80 | ((codepoint >> 6) & 0x3f));
            output[out++] = (char)(0x80 | (codepoint & 0x3f));
        } else {
            output[out++] = (char)(0xf0 | (codepoint >> 18));
            output[out++] = (char)(0x80 | ((codepoint >> 12) & 0x3f));
            output[out++] = (char)(0x80 | ((codepoint >> 6) & 0x3f));
            output[out++] = (char)(0x80 | (codepoint & 0x3f));
        }
    }
    output[out] = 0;
    return out;
}

static void short_name_to_text(const uint8_t *entry, char output[13])
{
    size_t out = 0;
    for (unsigned int index = 0; index < 8 && entry[index] != ' '; index++)
        output[out++] = (char)entry[index];
    if (entry[8] != ' ') {
        output[out++] = '.';
        for (unsigned int index = 8; index < 11 && entry[index] != ' '; index++)
            output[out++] = (char)entry[index];
    }
    output[out] = 0;
}

static void lfn_copy_piece(uint16_t output[260], const uint8_t *entry)
{
    static const uint8_t offsets[13] = {
        1, 3, 5, 7, 9, 14, 16, 18, 20, 22, 24, 28, 30
    };
    unsigned int ordinal = entry[0] & 0x1f;
    if (!ordinal || ordinal > FAT32_MAX_LFN_ENTRIES) return;
    for (unsigned int index = 0; index < 13; index++)
        output[(ordinal - 1) * 13 + index] = read16(entry + offsets[index]);
}

static int fat32_read_cluster_sector(uint32_t cluster, uint32_t offset, uint8_t sector[FAT32_SECTOR_SIZE])
{
    if (!valid_cluster(cluster) || offset >= volume.sectors_per_cluster)
        return STORAGE_ERR_CORRUPT;
    return device_read(cluster_sector(cluster) + offset, sector);
}

static int find_in_directory(uint32_t directory, const char *name, struct fat32_found *found)
{
    uint32_t cluster = directory;
    uint16_t long_name[260];
    uint32_t traversed = 0;
    int long_name_active = 0;
    if (!valid_cluster(directory)) return STORAGE_ERR_CORRUPT;
    while (valid_cluster(cluster) && traversed++ <= volume.cluster_count) {
        for (uint32_t sector_index = 0; sector_index < volume.sectors_per_cluster; sector_index++) {
            uint8_t sector[FAT32_SECTOR_SIZE];
            if (fat32_read_cluster_sector(cluster, sector_index, sector) != STORAGE_OK)
                return STORAGE_ERR_IO;
            uint32_t absolute_sector = cluster_sector(cluster) + sector_index;
            for (uint16_t offset = 0; offset < FAT32_SECTOR_SIZE; offset += 32) {
                uint8_t *entry = sector + offset;
                if (entry[0] == 0) return STORAGE_ERR_NOENT;
                if (entry[0] == 0xe5) { long_name_active = 0; continue; }
                if (entry[11] == 0x0f) {
                    if (entry[0] & 0x40) {
                        for (unsigned int index = 0; index < 260; index++) long_name[index] = 0xffff;
                        long_name_active = 1;
                    }
                    if (long_name_active) lfn_copy_piece(long_name, entry);
                    continue;
                }
                if (entry[11] & 0x08) { long_name_active = 0; continue; }

                char candidate[FAT32_MAX_NAME];
                char alias[13];
                short_name_to_text(entry, alias);
                if (long_name_active) utf16_to_utf8(long_name, 260, candidate, sizeof(candidate));
                else string_copy(candidate, alias, sizeof(candidate));
                long_name_active = 0;

                if (name_equal(candidate, name) || name_equal(alias, name)) {
                    if (found) {
                        bytes_zero(found, sizeof(*found));
                        string_copy(found->name, candidate, sizeof(found->name));
                        string_copy(found->short_name, alias, sizeof(found->short_name));
                        found->cluster = ((uint32_t)read16(entry + 20) << 16) |
                            read16(entry + 26);
                        found->size = read32(entry + 28);
                        found->attributes = entry[11];
                        found->created_date = read16(entry + 16);
                        found->modified_date = read16(entry + 24);
                        found->modified_time = read16(entry + 22);
                        found->parent_cluster = directory;
                        found->location.sector = absolute_sector;
                        found->location.offset = offset;
                        found->found = 1;
                    }
                    return STORAGE_OK;
                }
            }
        }
        uint32_t next;
        int status = cluster_next(cluster, &next);
        if (status == 1) break;
        if (status != STORAGE_OK) return status;
        cluster = next;
    }
    return STORAGE_ERR_NOENT;
}

static int normalize_path(const char *path, char output[FAT32_MAX_PATH])
{
    size_t length = 1, starts[FAT32_MAX_COMPONENTS], components = 0;
    if (!path || !*path) return STORAGE_ERR_INVAL;
    output[0] = '/';
    output[1] = 0;
    if (path[0] == '/' || path[0] == '\\') path++;
    if ((path[0] == 'C' || path[0] == 'c') && path[1] == ':') {
        path += 2;
        while (*path == '/' || *path == '\\') path++;
    }
    while (*path) {
        while (*path == '/' || *path == '\\') path++;
        if (!*path) break;
        const char *start = path;
        while (*path && *path != '/' && *path != '\\') path++;
        size_t count = (size_t)(path - start);
        if (count == 1 && start[0] == '.') continue;
        if (count == 2 && start[0] == '.' && start[1] == '.') {
            if (components) {
                length = starts[--components];
                output[length] = 0;
            }
            continue;
        }
        if (!count || count > 255 || components == FAT32_MAX_COMPONENTS ||
            length + count + 1 >= FAT32_MAX_PATH) return STORAGE_ERR_INVAL;
        starts[components++] = length;
        if (length > 1) output[length++] = '/';
        for (size_t index = 0; index < count; index++) output[length++] = start[index];
        output[length] = 0;
    }
    return STORAGE_OK;
}

static int lookup_path(const char *path, struct fat32_found *found, char normalized[FAT32_MAX_PATH])
{
    if (normalize_path(path, normalized) != STORAGE_OK) return STORAGE_ERR_INVAL;
    if (normalized[1] == 0) {
        if (found) {
            bytes_zero(found, sizeof(*found));
            found->name[0] = '/';
            found->attributes = 0x10;
            found->cluster = volume.root_cluster;
            found->found = 1;
        }
        return STORAGE_OK;
    }
    uint32_t directory = volume.root_cluster;
    const char *part = normalized + 1;
    while (*part) {
        char component[FAT32_MAX_NAME];
        size_t length = 0;
        while (part[length] && part[length] != '/') length++;
        if (!length || length >= sizeof(component)) return STORAGE_ERR_INVAL;
        for (size_t index = 0; index < length; index++) component[index] = part[index];
        component[length] = 0;
        struct fat32_found current;
        int result = find_in_directory(directory, component, &current);
        if (result != STORAGE_OK) return result;
        part += length;
        if (*part == '/') {
            if (!(current.attributes & 0x10)) return STORAGE_ERR_NOTDIR;
            directory = current.cluster;
            part++;
            if (!*part) return STORAGE_ERR_INVAL;
        } else if (found) {
            *found = current;
        }
    }
    return STORAGE_OK;
}

static int path_parent(const char *path, char parent[FAT32_MAX_PATH], char name[FAT32_MAX_NAME])
{
    char normalized[FAT32_MAX_PATH];
    if (normalize_path(path, normalized) != STORAGE_OK || normalized[1] == 0)
        return STORAGE_ERR_INVAL;
    size_t length = string_length(normalized);
    size_t separator = length;
    while (separator > 1 && normalized[separator - 1] != '/') separator--;
    string_copy(name, normalized + separator, FAT32_MAX_NAME);
    if (separator <= 1) {
        parent[0] = '/';
        parent[1] = 0;
    } else {
        size_t parent_length = separator - 1;
        for (size_t index = 0; index < parent_length; index++) parent[index] = normalized[index];
        parent[parent_length] = 0;
    }
    return STORAGE_OK;
}

static uint8_t short_checksum(const uint8_t name[11])
{
    uint8_t checksum = 0;
    for (unsigned int index = 0; index < 11; index++)
        checksum = (uint8_t)(((checksum & 1) ? 0x80 : 0) + (checksum >> 1) + name[index]);
    return checksum;
}

static void make_short_alias(const char *name, unsigned int serial, uint8_t output[11])
{
    size_t length = string_length(name), dot = length;
    for (size_t index = 0; index < length; index++) if (name[index] == '.') dot = index;
    for (unsigned int index = 0; index < 11; index++) output[index] = ' ';

    char base[9], extension[4];
    size_t base_length = 0, extension_length = 0;
    for (size_t index = 0; index < dot && base_length < sizeof(base) - 1; index++) {
        uint8_t value = (uint8_t)name[index];
        if (value == ' ' || value == '.') continue;
        if (value >= 'a' && value <= 'z') value = (uint8_t)(value - ('a' - 'A'));
        if ((value >= 'A' && value <= 'Z') || (value >= '0' && value <= '9') ||
        value == '$' || value == '%' || value == '\'' || value == '-' ||
        value == '_' || value == '@' || value == '~' || value == '`' ||
        value == '!' || value == '(' || value == ')' || value == '{' ||
        value == '}' || value == '^' || value == '#' || value == '&')
        base[base_length++] = (char)value;
    }
    if (!base_length) base[base_length++] = 'F';
    base[base_length] = 0;
    if (dot < length) {
        for (size_t index = dot + 1; index < length && extension_length < 3; index++) {
            uint8_t value = (uint8_t)name[index];
            if (value >= 'a' && value <= 'z') value = (uint8_t)(value - ('a' - 'A'));
            if ((value >= 'A' && value <= 'Z') || (value >= '0' && value <= '9'))
                extension[extension_length++] = (char)value;
        }
    }
    extension[extension_length] = 0;

    if (serial) {
        char digits[8];
        unsigned int count = 0;
        do {
            digits[count++] = (char)('0' + serial % 10);
            serial /= 10;
        } while (serial && count < sizeof(digits));
        size_t keep = count < 6 ? 7 - count : 1;
        if (keep > base_length) keep = base_length;
        for (size_t index = 0; index < keep; index++) output[index] = (uint8_t)base[index];
        output[keep++] = '~';
        while (count) output[keep++] = (uint8_t)digits[--count];
    } else {
        for (size_t index = 0; index < base_length && index < 8; index++)
            output[index] = (uint8_t)base[index];
    }
    for (size_t index = 0; index < extension_length; index++) output[8 + index] = (uint8_t)extension[index];
}

static void lfn_write_piece(uint8_t *entry, const uint16_t *units, size_t unit_count,
    unsigned int ordinal, unsigned int total, uint8_t checksum)
{
    static const uint8_t offsets[13] = {
        1, 3, 5, 7, 9, 14, 16, 18, 20, 22, 24, 28, 30
    };
    entry[0] = (uint8_t)ordinal;
    if (ordinal == total) entry[0] |= 0x40;
    entry[11] = 0x0f;
    entry[12] = 0;
    entry[13] = checksum;
    write16(entry + 26, 0);
    for (unsigned int index = 0; index < 13; index++) {
        size_t position = (ordinal - 1) * 13 + index;
        uint16_t value = position < unit_count ? units[position] :
            (position == unit_count ? 0 : 0xffff);
        write16(entry + offsets[index], value);
    }
}

static int find_free_slots(uint32_t directory, unsigned int count, struct fat32_dir_slot *slots)
{
    uint32_t cluster = directory, traversed = 0;
    uint32_t tail = directory;
    unsigned int free_count = 0;
    while (valid_cluster(cluster) && traversed++ <= volume.cluster_count) {
        for (uint32_t sector_index = 0; sector_index < volume.sectors_per_cluster; sector_index++) {
            uint8_t sector[FAT32_SECTOR_SIZE];
            if (fat32_read_cluster_sector(cluster, sector_index, sector) != STORAGE_OK)
                return STORAGE_ERR_IO;
            uint32_t absolute = cluster_sector(cluster) + sector_index;
            for (uint16_t offset = 0; offset < FAT32_SECTOR_SIZE; offset += 32) {
                uint8_t first = sector[offset];
                if (first == 0 || first == 0xe5) {
                    if (free_count < count) {
                        slots[free_count].sector = absolute;
                        slots[free_count].offset = offset;
                    }
                    if (++free_count >= count) return STORAGE_OK;
                } else free_count = 0;
            }
        }
        uint32_t next;
        int status = cluster_next(cluster, &next);
        if (status == 1) break;
        if (status != STORAGE_OK) return status;
        tail = cluster;
        cluster = next;
    }

    while (free_count < count) {
        uint32_t added;
        if (allocate_cluster(&added) != STORAGE_OK || fat_set(tail, added) != STORAGE_OK)
            return STORAGE_ERR_NOSPC;
        uint32_t entries_per_cluster = volume.bytes_per_cluster / 32;
        for (uint32_t index = 0; index < entries_per_cluster && free_count < count; index++) {
            slots[free_count].sector = cluster_sector(added) +
                index / (FAT32_SECTOR_SIZE / 32);
            slots[free_count].offset =
                (uint16_t)((index % (FAT32_SECTOR_SIZE / 32)) * 32);
            free_count++;
        }
        tail = added;
    }
    return STORAGE_OK;
}

static int write_slot(const struct fat32_dir_slot *slot, const uint8_t entry[32])
{
    uint8_t sector[FAT32_SECTOR_SIZE];
    if (device_read(slot->sector, sector) != STORAGE_OK) return STORAGE_ERR_IO;
    bytes_copy(sector + slot->offset, entry, 32);
    return device_write(slot->sector, sector);
}

static int create_dir_entry(uint32_t parent, const char *name, uint8_t attributes,
    uint32_t first_cluster, uint32_t size, struct fat32_found *created)
{
    uint16_t units[255];
    size_t unit_count = 0;
    if (utf8_to_utf16(name, units, &unit_count) != STORAGE_OK ||
        unit_count > 255 || name_equal(name, ".") || name_equal(name, ".."))
        return STORAGE_ERR_INVAL;
    struct fat32_found existing;
    if (find_in_directory(parent, name, &existing) == STORAGE_OK) return STORAGE_ERR_EXIST;

    uint8_t alias[11];
    char alias_text[13];
    unsigned int serial = 0;
    for (;;) {
        make_short_alias(name, serial, alias);
        short_name_to_text(alias, alias_text);
        if (find_in_directory(parent, alias_text, &existing) != STORAGE_OK) break;
        if (++serial > 999999) return STORAGE_ERR_NOSPC;
    }

    unsigned int long_entries = (unsigned int)((unit_count + 1 + 12) / 13);
    int needs_long_name = long_entries > 1 || !name_equal(name, alias_text);
    if (!needs_long_name) long_entries = 0;
    unsigned int slots_needed = long_entries + 1;
    struct fat32_dir_slot slots[FAT32_MAX_LFN_ENTRIES + 1];
    int status = find_free_slots(parent, slots_needed, slots);
    if (status != STORAGE_OK) return status;

    uint8_t checksum = short_checksum(alias);
    for (unsigned int index = 0; index < long_entries; index++) {
        uint8_t entry[32];
        unsigned int ordinal = long_entries - index;
        bytes_zero(entry, sizeof(entry));
        lfn_write_piece(entry, units, unit_count, ordinal, long_entries, checksum);
        if (write_slot(&slots[index], entry) != STORAGE_OK) return STORAGE_ERR_IO;
    }

    uint8_t entry[32];
    bytes_zero(entry, sizeof(entry));
    bytes_copy(entry, alias, sizeof(alias));
    entry[11] = attributes;
    write16(entry + 14, 0);
    write16(entry + 16, 0x5d68);
    write16(entry + 18, 0x5d68);
    write16(entry + 20, (uint16_t)(first_cluster >> 16));
    write16(entry + 22, 0);
    write16(entry + 24, 0x5d68);
    write16(entry + 26, (uint16_t)first_cluster);
    write32(entry + 28, size);
    if (write_slot(&slots[long_entries], entry) != STORAGE_OK) return STORAGE_ERR_IO;

    if (created) {
        bytes_zero(created, sizeof(*created));
        string_copy(created->name, name, sizeof(created->name));
        string_copy(created->short_name, alias_text, sizeof(created->short_name));
        created->cluster = first_cluster;
        created->size = size;
        created->attributes = attributes;
        created->created_date = 0x5d68;
        created->modified_date = 0x5d68;
        created->parent_cluster = parent;
        created->location.sector = slots[long_entries].sector;
        created->location.offset = slots[long_entries].offset;
        created->found = 1;
    }
    index_valid = 0;
    return STORAGE_OK;
}

static int write_directory_dots(uint32_t cluster, uint32_t parent)
{
    uint8_t sector[FAT32_SECTOR_SIZE];
    bytes_zero(sector, sizeof(sector));
    bytes_copy(sector, ".          ", 11);
    sector[11] = 0x10;
    write16(sector + 20, (uint16_t)(cluster >> 16));
    write16(sector + 26, (uint16_t)cluster);
    bytes_copy(sector + 32, "..         ", 11);
    sector[32 + 11] = 0x10;
    write16(sector + 32 + 20, (uint16_t)(parent >> 16));
    write16(sector + 32 + 26, (uint16_t)parent);
    uint32_t start = cluster_sector(cluster);
    for (uint32_t index = 0; index < volume.sectors_per_cluster; index++)
        if (device_write(start + index, sector) != STORAGE_OK) return STORAGE_ERR_IO;
    return STORAGE_OK;
}

static int split_for_create(const char *path, char parent_path[FAT32_MAX_PATH],
    char name[FAT32_MAX_NAME], uint32_t *parent_cluster)
{
    struct fat32_found parent;
    int result = path_parent(path, parent_path, name);
    if (result != STORAGE_OK) return result;
    char normalized_parent[FAT32_MAX_PATH];
    result = lookup_path(parent_path, &parent, normalized_parent);
    if (result != STORAGE_OK) return result;
    if (!(parent.attributes & 0x10)) return STORAGE_ERR_NOTDIR;
    *parent_cluster = parent.cluster;
    return STORAGE_OK;
}

static int create_entry_internal(const char *path, char type, const void *content, size_t length)
{
    char parent_path[FAT32_MAX_PATH], name[FAT32_MAX_NAME];
    uint32_t parent_cluster, new_cluster = 0;
    if (type != 'f' && type != 'd') return STORAGE_ERR_INVAL;
    int result = split_for_create(path, parent_path, name, &parent_cluster);
    if (result != STORAGE_OK) return result;
    if (type == 'd') {
        result = allocate_cluster(&new_cluster);
        if (result != STORAGE_OK) return result;
        if (write_directory_dots(new_cluster, parent_cluster) != STORAGE_OK) {
            (void)free_chain(new_cluster);
            return STORAGE_ERR_IO;
        }
    }
    result = create_dir_entry(parent_cluster, name, type == 'd' ? 0x10 : 0x20,
        new_cluster, 0, 0);
    if (result != STORAGE_OK) {
        if (new_cluster) (void)free_chain(new_cluster);
        return result;
    }
    if (type == 'f' && length) {
        result = fat32_write_file(path, content, length, 0);
        if (result < 0) {
            (void)fat32_unlink(path);
            return result;
        }
    }
    return STORAGE_OK;
}

static int update_found_entry(const struct fat32_found *found, uint32_t cluster, uint32_t size)
{
    uint8_t sector[FAT32_SECTOR_SIZE];
    if (!found || device_read(found->location.sector, sector) != STORAGE_OK)
        return STORAGE_ERR_IO;
    uint8_t *entry = sector + found->location.offset;
    if (entry[0] == 0xe5 || entry[11] == 0x0f) return STORAGE_ERR_CORRUPT;
    write16(entry + 20, (uint16_t)(cluster >> 16));
    write16(entry + 26, (uint16_t)cluster);
    write32(entry + 28, size);
    write16(entry + 22, 0);
    write16(entry + 24, 0x5d68);
    if (device_write(found->location.sector, sector) != STORAGE_OK) return STORAGE_ERR_IO;
    index_valid = 0;
    return STORAGE_OK;
}

static int file_read_at(uint32_t first_cluster, uint32_t size, uint32_t offset,
    void *buffer, size_t length)
{
    if (!buffer && length) return STORAGE_ERR_INVAL;
    if (offset >= size) return 0;
    size_t count = size - offset < length ? size - offset : length;
    size_t copied = 0;
    uint8_t sector[FAT32_SECTOR_SIZE];
    while (copied < count) {
        uint32_t absolute_offset = offset + (uint32_t)copied;
        uint32_t cluster_index = absolute_offset / volume.bytes_per_cluster;
        uint32_t within_cluster = absolute_offset % volume.bytes_per_cluster;
        uint32_t cluster;
        int result = get_nth_cluster(first_cluster, cluster_index, 0, &cluster);
        if (result != STORAGE_OK) return result;
        uint32_t sector_index = within_cluster / FAT32_SECTOR_SIZE;
        uint32_t within_sector = within_cluster % FAT32_SECTOR_SIZE;
        if (fat32_read_cluster_sector(cluster, sector_index, sector) != STORAGE_OK)
            return STORAGE_ERR_IO;
        size_t part = FAT32_SECTOR_SIZE - within_sector;
        if (part > count - copied) part = count - copied;
        bytes_copy((uint8_t *)buffer + copied, sector + within_sector, part);
        copied += part;
    }
    return (int)copied;
}

static int file_write_at(struct fat32_found *found, uint32_t offset, const void *buffer,
    size_t length)
{
    if (length > 0xffffffffu || offset > 0xffffffffu - length) return STORAGE_ERR_NOSPC;
    if (!buffer && length) return STORAGE_ERR_INVAL;
    uint32_t required_size = offset + (uint32_t)length;
    uint32_t needed_clusters = (required_size + volume.bytes_per_cluster - 1) /
        volume.bytes_per_cluster;
    uint32_t first = found->cluster;
    if (needed_clusters && !first) {
        if (allocate_cluster(&first) != STORAGE_OK) return STORAGE_ERR_NOSPC;
    }
    for (uint32_t index = 0; index < needed_clusters; index++) {
        uint32_t cluster;
        int result = get_nth_cluster(first, index, 1, &cluster);
        if (result != STORAGE_OK) return result;
    }
    if (offset > found->size) {
        uint8_t zeros[64];
        bytes_zero(zeros, sizeof(zeros));
        uint32_t gap = offset - found->size;
        uint32_t cursor = found->size;
        while (gap) {
            size_t part = gap > sizeof(zeros) ? sizeof(zeros) : gap;
            int result = file_write_at(found, cursor, zeros, part);
            if (result < 0) return result;
            cursor += (uint32_t)part;
            gap -= (uint32_t)part;
            found->size = cursor;
            found->cluster = first;
        }
    }
    size_t written = 0;
    uint8_t sector[FAT32_SECTOR_SIZE];
    while (written < length) {
        uint32_t absolute_offset = offset + (uint32_t)written;
        uint32_t cluster_index = absolute_offset / volume.bytes_per_cluster;
        uint32_t within_cluster = absolute_offset % volume.bytes_per_cluster;
        uint32_t cluster;
        int result = get_nth_cluster(first, cluster_index, 1, &cluster);
        if (result != STORAGE_OK) return result;
        uint32_t sector_index = within_cluster / FAT32_SECTOR_SIZE;
        uint32_t within_sector = within_cluster % FAT32_SECTOR_SIZE;
        uint32_t sector_number = cluster_sector(cluster) + sector_index;
        size_t part = FAT32_SECTOR_SIZE - within_sector;
        if (part > length - written) part = length - written;
        if ((within_sector || part < FAT32_SECTOR_SIZE) &&
            device_read(sector_number, sector) != STORAGE_OK) return STORAGE_ERR_IO;
        if (!within_sector && part == FAT32_SECTOR_SIZE) bytes_zero(sector, sizeof(sector));
        bytes_copy(sector + within_sector, (const uint8_t *)buffer + written, part);
        if (device_write(sector_number, sector) != STORAGE_OK) return STORAGE_ERR_IO;
        written += part;
    }
    if (required_size > found->size) found->size = required_size;
    found->cluster = first;
    return update_found_entry(found, first, found->size) == STORAGE_OK
        ? (int)length : STORAGE_ERR_IO;
}

static int directory_is_empty(uint32_t cluster)
{
    uint32_t current = cluster, traversed = 0;
    while (valid_cluster(current) && traversed++ <= volume.cluster_count) {
        for (uint32_t sector_index = 0; sector_index < volume.sectors_per_cluster; sector_index++) {
            uint8_t sector[FAT32_SECTOR_SIZE];
            if (fat32_read_cluster_sector(current, sector_index, sector) != STORAGE_OK)
                return STORAGE_ERR_IO;
            for (unsigned int offset = 0; offset < FAT32_SECTOR_SIZE; offset += 32) {
                uint8_t *entry = sector + offset;
                if (entry[0] == 0) return 1;
                if (entry[0] == 0xe5 || entry[11] == 0x0f) continue;
                if (entry[11] & 0x08) continue;
                if (entry[0] == '.' && (entry[1] == ' ' || entry[1] == '.')) continue;
                return 0;
            }
        }
        uint32_t next;
        int status = cluster_next(current, &next);
        if (status == 1) return 1;
        if (status != STORAGE_OK) return status;
        current = next;
    }
    return STORAGE_ERR_CORRUPT;
}

static int delete_found_entry(const struct fat32_found *found)
{
    uint32_t cluster = found->parent_cluster, traversed = 0;
    struct fat32_location long_slots[FAT32_MAX_LFN_ENTRIES];
    unsigned int long_count = 0;
    while (valid_cluster(cluster) && traversed++ <= volume.cluster_count) {
        for (uint32_t sector_index = 0; sector_index < volume.sectors_per_cluster; sector_index++) {
            uint8_t sector[FAT32_SECTOR_SIZE];
            if (fat32_read_cluster_sector(cluster, sector_index, sector) != STORAGE_OK)
                return STORAGE_ERR_IO;
            uint32_t absolute = cluster_sector(cluster) + sector_index;
            for (uint16_t offset = 0; offset < FAT32_SECTOR_SIZE; offset += 32) {
                uint8_t *entry = sector + offset;
                if (entry[0] == 0) return STORAGE_ERR_NOENT;
                if (entry[11] == 0x0f) {
                    if (long_count < FAT32_MAX_LFN_ENTRIES) {
                        long_slots[long_count].sector = absolute;
                        long_slots[long_count++].offset = offset;
                    }
                    continue;
                }
                if (entry[0] == 0xe5) { long_count = 0; continue; }
                if (absolute == found->location.sector && offset == found->location.offset) {
                    uint8_t tombstone[32];
                    if (long_count > FAT32_MAX_LFN_ENTRIES) long_count = FAT32_MAX_LFN_ENTRIES;
                    for (unsigned int index = 0; index < long_count; index++) {
                        if (device_read(long_slots[index].sector, sector) != STORAGE_OK) return STORAGE_ERR_IO;
                        bytes_copy(tombstone, sector + long_slots[index].offset, sizeof(tombstone));
                        tombstone[0] = 0xe5;
                        bytes_copy(sector + long_slots[index].offset, tombstone, sizeof(tombstone));
                        if (device_write(long_slots[index].sector, sector) != STORAGE_OK) return STORAGE_ERR_IO;
                    }
                    if (device_read(absolute, sector) != STORAGE_OK) return STORAGE_ERR_IO;
                    sector[offset] = 0xe5;
                    if (device_write(absolute, sector) != STORAGE_OK) return STORAGE_ERR_IO;
                    index_valid = 0;
                    return STORAGE_OK;
                }
                long_count = 0;
            }
        }
        uint32_t next;
        int status = cluster_next(cluster, &next);
        if (status == 1) break;
        if (status != STORAGE_OK) return status;
        cluster = next;
    }
    return STORAGE_ERR_NOENT;
}

static int remove_one(const char *path, int directory)
{
    struct fat32_found found;
    char normalized[FAT32_MAX_PATH];
    int result = lookup_path(path, &found, normalized);
    if (result != STORAGE_OK) return result;
    if (normalized[1] == 0) return STORAGE_ERR_INVAL;
    if (!!(found.attributes & 0x10) != !!directory)
        return directory ? STORAGE_ERR_NOTDIR : STORAGE_ERR_ISDIR;
    if (directory) {
        int empty = directory_is_empty(found.cluster);
        if (empty < 0) return empty;
        if (!empty) return STORAGE_ERR_NOTEMPTY;
    }
    if (delete_found_entry(&found) != STORAGE_OK) return STORAGE_ERR_IO;
    if (found.cluster && free_chain(found.cluster) != STORAGE_OK) return STORAGE_ERR_IO;
    return STORAGE_OK;
}

static int index_add(const char *path, const struct fat32_found *found)
{
    if (index_count >= FAT32_INDEX_LIMIT) return STORAGE_ERR_NOSPC;
    struct fat32_index_entry *entry = &index_entries[index_count++];
    bytes_zero(entry, sizeof(*entry));
    string_copy(entry->path, path, sizeof(entry->path));
    string_copy(entry->name, found->name, sizeof(entry->name));
    entry->type = found->attributes & 0x10 ? 'd' : 'f';
    entry->cluster = found->cluster;
    entry->size = found->size;
    entry->attributes = found->attributes;
    entry->created_date = found->created_date;
    entry->modified_date = found->modified_date;
    entry->modified_time = found->modified_time;
    return STORAGE_OK;
}

static int index_directory(uint32_t directory, const char *path, unsigned int depth)
{
    uint32_t cluster = directory, traversed = 0;
    uint16_t long_name[260];
    int long_name_active = 0;
    if (depth > FAT32_MAX_COMPONENTS) return STORAGE_ERR_CORRUPT;
    while (valid_cluster(cluster) && traversed++ <= volume.cluster_count) {
        for (uint32_t sector_index = 0; sector_index < volume.sectors_per_cluster; sector_index++) {
            uint8_t sector[FAT32_SECTOR_SIZE];
            if (fat32_read_cluster_sector(cluster, sector_index, sector) != STORAGE_OK)
                return STORAGE_ERR_IO;
            uint32_t absolute = cluster_sector(cluster) + sector_index;
            for (uint16_t offset = 0; offset < FAT32_SECTOR_SIZE; offset += 32) {
                uint8_t *entry = sector + offset;
                if (entry[0] == 0) return STORAGE_OK;
                if (entry[0] == 0xe5) { long_name_active = 0; continue; }
                if (entry[11] == 0x0f) {
                    if (entry[0] & 0x40) {
                        for (unsigned int index = 0; index < 260; index++) long_name[index] = 0xffff;
                        long_name_active = 1;
                    }
                    if (long_name_active) lfn_copy_piece(long_name, entry);
                    continue;
                }
                if (entry[11] & 0x08) { long_name_active = 0; continue; }
                if (entry[0] == '.') { long_name_active = 0; continue; }

                struct fat32_found found;
                bytes_zero(&found, sizeof(found));
                char alias[13];
                short_name_to_text(entry, alias);
                if (long_name_active) utf16_to_utf8(long_name, 260, found.name, sizeof(found.name));
                else string_copy(found.name, alias, sizeof(found.name));
                long_name_active = 0;
                string_copy(found.short_name, alias, sizeof(found.short_name));
                found.cluster = ((uint32_t)read16(entry + 20) << 16) | read16(entry + 26);
                found.size = read32(entry + 28);
                found.attributes = entry[11];
                found.created_date = read16(entry + 16);
                found.modified_date = read16(entry + 24);
                found.modified_time = read16(entry + 22);
                found.location.sector = absolute;
                found.location.offset = offset;
                found.parent_cluster = directory;
                found.found = 1;

                char child[FAT32_MAX_PATH];
                size_t path_length = string_length(path), name_length = string_length(found.name);
                if (path_length + name_length + 2 >= sizeof(child)) return STORAGE_ERR_INVAL;
                string_copy(child, path, sizeof(child));
                if (path_length > 1) child[path_length++] = '/';
                for (size_t index = 0; index < name_length; index++)
                    child[path_length + index] = found.name[index];
                child[path_length + name_length] = 0;
                int result = index_add(child, &found);
                if (result != STORAGE_OK) return result;
                if (found.attributes & 0x10) {
                    result = index_directory(found.cluster, child, depth + 1);
                    if (result != STORAGE_OK) return result;
                }
            }
        }
        uint32_t next;
        int status = cluster_next(cluster, &next);
        if (status == 1) break;
        if (status != STORAGE_OK) return status;
        cluster = next;
    }
    return STORAGE_OK;
}

static int rebuild_index(void)
{
    index_count = 0;
    if (!volume.mounted) return STORAGE_ERR_NOTMOUNTED;
    int result = index_directory(volume.root_cluster, "/C:", 0);
    if (result == STORAGE_OK) index_valid = 1;
    return result;
}

static int ensure_index(void)
{
    return index_valid ? STORAGE_OK : rebuild_index();
}

static int format_volume(const struct block_device *device)
{
    uint32_t start = 0;
    uint32_t total = 0;
    uint32_t partition_start = 2048;
    if (!device || !device->read_sector || !device->write_sector ||
        device->sector_count > 0xffffffffu) return STORAGE_ERR_INVAL;
    if (device->sector_count >= 131072) start = partition_start;
    total = (uint32_t)device->sector_count - start;
    uint32_t reserved = 32, fats = 2, fat_sectors = 1, clusters = 0;
    for (unsigned int iteration = 0; iteration < 16; iteration++) {
        if (total <= reserved + fats * fat_sectors) return STORAGE_ERR_NOSPC;
        clusters = (total - reserved - fats * fat_sectors);
        uint32_t needed = (uint32_t)(((uint64_t)(clusters + 2) * 4 +
            FAT32_SECTOR_SIZE - 1) / FAT32_SECTOR_SIZE);
        if (needed == fat_sectors) break;
        fat_sectors = needed;
    }
    clusters = total - reserved - fats * fat_sectors;
    if (clusters < FAT32_MIN_CLUSTERS) return STORAGE_ERR_NOSPC;

    uint8_t sector[FAT32_SECTOR_SIZE];
    bytes_zero(sector, sizeof(sector));
    if (start) {
        sector[0] = 0xeb;
        sector[1] = 0x58;
        sector[2] = 0x90;
        bytes_copy(sector + 3, "ORTOSFAT", 8);
        sector[446 + 4] = 0x0c;
        write32(sector + 446 + 8, start);
        write32(sector + 446 + 12, total);
        sector[510] = 0x55;
        sector[511] = 0xaa;
        if (device->write_sector(device->context, 0, sector) != 0) return STORAGE_ERR_IO;
    }

    bytes_zero(sector, sizeof(sector));
    sector[0] = 0xeb;
    sector[1] = 0x58;
    sector[2] = 0x90;
    bytes_copy(sector + 3, "ORTOSFAT", 8);
    write16(sector + 11, FAT32_SECTOR_SIZE);
    sector[13] = 1;
    write16(sector + 14, (uint16_t)reserved);
    sector[16] = (uint8_t)fats;
    write32(sector + 32, total);
    write32(sector + 36, fat_sectors);
    write16(sector + 40, 0);
    write16(sector + 42, 0);
    write32(sector + 44, 2);
    write16(sector + 48, 1);
    write16(sector + 50, 6);
    sector[64] = 0x80;
    sector[66] = 0x29;
    write32(sector + 67, 0x4f52544f);
    bytes_copy(sector + 71, "ORANGE TEA ", 11);
    bytes_copy(sector + 82, "FAT32   ", 8);
    sector[510] = 0x55;
    sector[511] = 0xaa;
    if (device->write_sector(device->context, start, sector) != 0) return STORAGE_ERR_IO;
    if (device->write_sector(device->context, start + 6, sector) != 0) return STORAGE_ERR_IO;

    bytes_zero(sector, sizeof(sector));
    for (uint32_t index = 0; index < reserved; index++) {
        if (index == 0 || index == 1 || index == 6) continue;
        if (device->write_sector(device->context, start + index, sector) != 0) return STORAGE_ERR_IO;
    }
    bytes_zero(sector, sizeof(sector));
    write32(sector, 0x0ffffff8);
    write32(sector + 4, 0xffffffff);
    write32(sector + 8, 0x0fffffff);
    uint32_t fats_base = start + reserved;
    for (uint32_t copy = 0; copy < fats; copy++) {
        for (uint32_t index = 0; index < fat_sectors; index++) {
            bytes_zero(sector, sizeof(sector));
            if (!index) {
                write32(sector, 0x0ffffff8);
                write32(sector + 4, 0xffffffff);
                write32(sector + 8, 0x0fffffff);
            }
            if (device->write_sector(device->context,
                fats_base + copy * fat_sectors + index, sector) != 0) return STORAGE_ERR_IO;
        }
    }
    bytes_zero(sector, sizeof(sector));
    uint32_t root_sector = start + reserved + fats * fat_sectors;
    for (uint32_t index = 0; index < FAT32_SECTOR_SIZE / FAT32_SECTOR_SIZE *
        (uint32_t)1; index++)
        if (device->write_sector(device->context, root_sector + index, sector) != 0)
            return STORAGE_ERR_IO;
    bytes_zero(sector, sizeof(sector));
    write32(sector, 0x41615252);
    write32(sector + 484, 0x61417272);
    write32(sector + 488, clusters - 1);
    write32(sector + 492, 3);
    write32(sector + 508, 0xaa550000);
    if (device->write_sector(device->context, start + 1, sector) != 0 ||
        device->write_sector(device->context, start + 7, sector) != 0) return STORAGE_ERR_IO;
    if (device->flush && device->flush(device->context) != 0) return STORAGE_ERR_IO;
    return STORAGE_OK;
}

int fat32_format(const struct block_device *device)
{
    if (!device) return STORAGE_ERR_INVAL;
    volume.device = (struct block_device *)device;
    volume.mounted = 0;
    int result = format_volume(device);
    if (result != STORAGE_OK) return result;
    return fat32_mount(device);
}

int fat32_mount(const struct block_device *device)
{
    uint8_t boot[FAT32_SECTOR_SIZE];
    uint32_t start, sectors;
    bytes_zero(&volume, sizeof(volume));
    if (!device || !device->read_sector) return STORAGE_ERR_INVAL;
    int result = volume_parameters(device, &start, &sectors, boot);
    if (result != STORAGE_OK) return result;

    volume.device = (struct block_device *)device;
    volume.partition_start = start;
    volume.volume_sectors = sectors;
    volume.sectors_per_cluster = boot[13];
    volume.bytes_per_cluster = volume.sectors_per_cluster * FAT32_SECTOR_SIZE;
    volume.reserved_sectors = read16(boot + 14);
    volume.fat_count = boot[16];
    volume.fat_sectors = read32(boot + 36);
    volume.fat_start = volume.reserved_sectors;
    volume.data_start = volume.reserved_sectors + volume.fat_count * volume.fat_sectors;
    volume.cluster_count = (sectors - volume.data_start) / volume.sectors_per_cluster;
    volume.root_cluster = read32(boot + 44) & 0x0fffffff;
    if (!valid_cluster(volume.root_cluster)) {
        bytes_zero(&volume, sizeof(volume));
        return STORAGE_ERR_CORRUPT;
    }
    volume.mounted = 1;
    uint32_t root_value;
    if (fat_get(volume.root_cluster, &root_value) != STORAGE_OK ||
        (root_value < 0x0ffffff8u && !valid_cluster(root_value))) {
        bytes_zero(&volume, sizeof(volume));
        return STORAGE_ERR_CORRUPT;
    }
    index_valid = 0;
    allocation_hint = 2;
    for (unsigned int index = 0; index < 16; index++) file_descriptors[index].used = 0;
    return STORAGE_OK;
}

int fat32_unmount(void)
{
    int result = fat32_sync();
    if (result == STORAGE_OK) {
        volume.mounted = 0;
        index_valid = 0;
        for (unsigned int index = 0; index < 16; index++) file_descriptors[index].used = 0;
    }
    return result;
}

int fat32_is_mounted(void) { return volume.mounted; }

int fat32_sync(void)
{
    if (!volume.mounted) return STORAGE_ERR_NOTMOUNTED;
    return volume.device->flush && volume.device->flush(volume.device->context) != 0
        ? STORAGE_ERR_IO : STORAGE_OK;
}

int fat32_get_stats(struct storage_stats *stats)
{
    if (!stats || !volume.mounted) return STORAGE_ERR_NOTMOUNTED;
    uint64_t allocated = 0;
    uint8_t fat_sector[FAT32_SECTOR_SIZE];
    for (uint32_t sector_index = 0; sector_index < volume.fat_sectors; sector_index++) {
        if (device_read(volume.partition_start + volume.fat_start + sector_index,
            fat_sector) != STORAGE_OK) return STORAGE_ERR_IO;
        uint32_t first_cluster = sector_index * (FAT32_SECTOR_SIZE / 4);
        for (uint32_t index = 0; index < FAT32_SECTOR_SIZE / 4; index++) {
            uint32_t cluster = first_cluster + index;
            if (cluster >= 2 && cluster < volume.cluster_count + 2 &&
                (read32(fat_sector + index * 4) & 0x0fffffffu)) allocated++;
        }
    }
    if (ensure_index() != STORAGE_OK) return STORAGE_ERR_IO;
    stats->total_sectors = volume.volume_sectors;
    stats->used_sectors = (uint64_t)volume.data_start +
        allocated * volume.sectors_per_cluster;
    stats->free_sectors = volume.volume_sectors > stats->used_sectors
        ? volume.volume_sectors - stats->used_sectors : 0;
    stats->sector_size = FAT32_SECTOR_SIZE;
    stats->file_count = 0;
    stats->directory_count = 0;
    for (int index = 0; index < index_count; index++) {
        if (index_entries[index].type == 'f') stats->file_count++;
        else stats->directory_count++;
    }
    return STORAGE_OK;
}

int fat32_fsck(int repair, int *errors)
{
    (void)repair;
    if (errors) *errors = 0;
    if (!volume.mounted) return STORAGE_ERR_NOTMOUNTED;
    uint32_t root;
    if (fat_get(volume.root_cluster, &root) != STORAGE_OK || root < 0x0ffffff8u) {
        if (errors) *errors = 1;
        return STORAGE_ERR_CORRUPT;
    }
    return STORAGE_OK;
}

int fat32_create_file(const char *path, const void *buffer, size_t length)
{
    if (!volume.mounted) return STORAGE_ERR_NOTMOUNTED;
    if (!buffer && length) return STORAGE_ERR_INVAL;
    return create_entry_internal(path, 'f', buffer, length);
}

int fat32_create_entry(const char *path, char type, const char *content)
{
    if (!volume.mounted) return 0;
    size_t length = type == 'f' ? string_length(content) : 0;
    return create_entry_internal(path, type, content, length) == STORAGE_OK;
}

int fat32_find_entry(const char *path)
{
    if (!volume.mounted || ensure_index() != STORAGE_OK) return -1;
    char normalized[FAT32_MAX_PATH];
    if (normalize_path(path, normalized) != STORAGE_OK) return -1;
    if (normalized[1] == 0) return -1;
    char output[FAT32_MAX_PATH];
    output[0] = '/';
    output[1] = 'C';
    output[2] = ':';
    string_copy(output + 3, normalized, sizeof(output) - 3);
    for (int index = 0; index < index_count; index++)
        if (name_equal(index_entries[index].path, output)) return index;
    return -1;
}

int fat32_get_entry_info(const char *path, struct storage_entry_info *info)
{
    if (!info) return STORAGE_ERR_INVAL;
    if (!volume.mounted) return STORAGE_ERR_NOTMOUNTED;
    struct fat32_found found;
    char normalized[FAT32_MAX_PATH];
    int result = lookup_path(path, &found, normalized);
    if (result != STORAGE_OK) return result;
    if (normalized[1] == 0) {
        info->name = "/";
        info->path = "/";
        info->type = 'd';
        info->size = 0;
        info->blocks_used = 0;
        info->file_id = 0;
        info->created_at = 0;
        info->modified_at = 0;
        info->attributes = STORAGE_ATTR_SYSTEM;
        return STORAGE_OK;
    }
    if (ensure_index() != STORAGE_OK) return STORAGE_ERR_IO;
    int index = fat32_find_entry(path);
    if (index < 0) return STORAGE_ERR_NOENT;
    return fat32_get_entry_info_at(index, info);
}

int fat32_get_entry_count(void)
{
    return volume.mounted && ensure_index() == STORAGE_OK ? index_count : 0;
}

int fat32_get_entry_info_at(int index, struct storage_entry_info *info)
{
    if (!info) return STORAGE_ERR_INVAL;
    if (!volume.mounted) return STORAGE_ERR_NOTMOUNTED;
    if (ensure_index() != STORAGE_OK || index < 0 || index >= index_count)
        return STORAGE_ERR_NOENT;
    struct fat32_index_entry *entry = &index_entries[index];
    info->name = entry->name;
    info->path = entry->path;
    info->type = entry->type;
    info->size = entry->size;
    info->blocks_used = entry->cluster ? (entry->size + volume.bytes_per_cluster - 1) /
        volume.bytes_per_cluster : 0;
    info->file_id = (uint32_t)index + 1;
    info->created_at = entry->created_date;
    info->modified_at = ((uint64_t)entry->modified_date << 16) | entry->modified_time;
    info->attributes = (entry->attributes & 0x01 ? STORAGE_ATTR_READONLY : 0) |
        (entry->attributes & 0x04 ? STORAGE_ATTR_HIDDEN : 0) |
        (entry->attributes & 0x04 ? STORAGE_ATTR_SYSTEM : 0);
    return STORAGE_OK;
}

const char *fat32_get_entry_name(int index)
{
    return volume.mounted && ensure_index() == STORAGE_OK && index >= 0 && index < index_count
        ? index_entries[index].name : "";
}

const char *fat32_get_entry_path(int index)
{
    return volume.mounted && ensure_index() == STORAGE_OK && index >= 0 && index < index_count
        ? index_entries[index].path : "";
}

char fat32_get_entry_type(int index)
{
    return volume.mounted && ensure_index() == STORAGE_OK && index >= 0 && index < index_count
        ? index_entries[index].type : 0;
}

const char *fat32_get_entry_content(int index)
{
    size_t length = 0;
    if (fat32_get_entry_type(index) != 'f') return "";
    if (fat32_read_file(fat32_get_entry_path(index), content_preview,
        sizeof(content_preview) - 1, &length) < 0) return "";
    content_preview[length < sizeof(content_preview) ? length : sizeof(content_preview) - 1] = 0;
    return content_preview;
}

int fat32_unlink(const char *path) { return volume.mounted ? remove_one(path, 0) : STORAGE_ERR_NOTMOUNTED; }
int fat32_rmdir(const char *path) { return volume.mounted ? remove_one(path, 1) : STORAGE_ERR_NOTMOUNTED; }

int fat32_remove_tree(const char *path)
{
    if (!volume.mounted) return STORAGE_ERR_NOTMOUNTED;
    struct fat32_found root;
    char normalized[FAT32_MAX_PATH], full_path[FAT32_MAX_PATH];
    int result = lookup_path(path, &root, normalized);
    if (result != STORAGE_OK) return result;
    if (normalized[1] == 0) return STORAGE_ERR_INVAL;
    if (!(root.attributes & 0x10)) return STORAGE_ERR_NOTDIR;
    if (ensure_index() != STORAGE_OK) return STORAGE_ERR_IO;
    full_path[0] = '/';
    full_path[1] = 'C';
    full_path[2] = ':';
    string_copy(full_path + 3, normalized, sizeof(full_path) - 3);
    size_t prefix_length = string_length(full_path);
    unsigned int count = 0;
    for (int index = 0; index < index_count; index++) {
        const char *candidate = index_entries[index].path;
        size_t candidate_length = string_length(candidate);
        if (name_equal(candidate, full_path) ||
            (candidate_length > prefix_length && path_has_prefix(candidate, full_path))) {
            string_copy(remove_paths[count++], candidate, FAT32_MAX_PATH);
        }
    }
    for (unsigned int index = 0; index < count; index++) {
        unsigned int longest = index;
        for (unsigned int candidate = index + 1; candidate < count; candidate++)
            if (string_length(remove_paths[candidate]) > string_length(remove_paths[longest]))
                longest = candidate;
        if (longest != index) {
            char temporary[FAT32_MAX_PATH];
            string_copy(temporary, remove_paths[index], sizeof(temporary));
            string_copy(remove_paths[index], remove_paths[longest], sizeof(remove_paths[index]));
            string_copy(remove_paths[longest], temporary, sizeof(remove_paths[longest]));
        }
        struct fat32_found found;
        char remove_normalized[FAT32_MAX_PATH];
        result = lookup_path(remove_paths[index], &found, remove_normalized);
        if (result != STORAGE_OK) return result;
        if (delete_found_entry(&found) != STORAGE_OK) return STORAGE_ERR_IO;
        if (found.cluster && free_chain(found.cluster) != STORAGE_OK) return STORAGE_ERR_IO;
    }
    index_valid = 0;
    return STORAGE_OK;
}

int fat32_mkdir(const char *path)
{
    if (!volume.mounted) return STORAGE_ERR_NOTMOUNTED;
    return create_entry_internal(path, 'd', 0, 0);
}

int fat32_read_file(const char *path, void *buffer, size_t capacity, size_t *length)
{
    if (!volume.mounted) return STORAGE_ERR_NOTMOUNTED;
    struct fat32_found found;
    char normalized[FAT32_MAX_PATH];
    int result = lookup_path(path, &found, normalized);
    if (result != STORAGE_OK) return result;
    if (found.attributes & 0x10) return STORAGE_ERR_ISDIR;
    if (!buffer && capacity) return STORAGE_ERR_INVAL;
    if (length) *length = found.size;
    return file_read_at(found.cluster, found.size, 0, buffer, capacity);
}

int fat32_write_file(const char *path, const void *buffer, size_t length, int append)
{
    if (!volume.mounted) return STORAGE_ERR_NOTMOUNTED;
    if (!buffer && length) return STORAGE_ERR_INVAL;
    struct fat32_found found;
    char normalized[FAT32_MAX_PATH];
    int result = lookup_path(path, &found, normalized);
    if (result == STORAGE_ERR_NOENT) {
        result = create_entry_internal(path, 'f', 0, 0);
        if (result != STORAGE_OK) return result;
        result = lookup_path(path, &found, normalized);
    }
    if (result != STORAGE_OK) return result;
    if (found.attributes & 0x10) return STORAGE_ERR_ISDIR;
    if (append) return file_write_at(&found, found.size, buffer, length);

    uint32_t old_cluster = found.cluster;
    uint32_t saved_size = found.size;
    found.cluster = 0;
    found.size = 0;
    if (!length) {
        if (update_found_entry(&found, 0, 0) != STORAGE_OK) return STORAGE_ERR_IO;
        if (old_cluster) return free_chain(old_cluster);
        return STORAGE_OK;
    }
    int written = file_write_at(&found, 0, buffer, length);
    if (written < 0) {
        if (found.cluster && found.cluster != old_cluster) (void)free_chain(found.cluster);
        found.cluster = old_cluster;
        found.size = saved_size;
        return written;
    }
    if (old_cluster && old_cluster != found.cluster) {
        int free_result = free_chain(old_cluster);
        if (free_result != STORAGE_OK) return free_result;
    }
    return written;
}

int fat32_open(const char *path, int create)
{
    if (!volume.mounted) return STORAGE_ERR_NOTMOUNTED;
    struct fat32_found found;
    char normalized[FAT32_MAX_PATH];
    int result = lookup_path(path, &found, normalized);
    if (result == STORAGE_ERR_NOENT && create) {
        result = create_entry_internal(path, 'f', 0, 0);
        if (result == STORAGE_OK) result = lookup_path(path, &found, normalized);
    }
    if (result != STORAGE_OK) return result;
    if (found.attributes & 0x10) return STORAGE_ERR_ISDIR;
    for (int index = 0; index < 16; index++) if (!file_descriptors[index].used) {
        file_descriptors[index].used = 1;
        file_descriptors[index].offset = 0;
        string_copy(file_descriptors[index].path, normalized, sizeof(file_descriptors[index].path));
        return index;
    }
    return STORAGE_ERR_NOSPC;
}

int fat32_close(int fd)
{
    if (fd < 0 || fd >= 16 || !file_descriptors[fd].used) return STORAGE_ERR_BADFD;
    file_descriptors[fd].used = 0;
    return STORAGE_OK;
}

int fat32_read(int fd, void *buffer, size_t length)
{
    if (!volume.mounted) return STORAGE_ERR_NOTMOUNTED;
    if (fd < 0 || fd >= 16 || !file_descriptors[fd].used || (!buffer && length))
        return STORAGE_ERR_BADFD;
    struct fat32_found found;
    char normalized[FAT32_MAX_PATH];
    int result = lookup_path(file_descriptors[fd].path, &found, normalized);
    if (result != STORAGE_OK) return result;
    int count = file_read_at(found.cluster, found.size, file_descriptors[fd].offset, buffer, length);
    if (count > 0) file_descriptors[fd].offset += (uint32_t)count;
    return count;
}

int fat32_write(int fd, const void *buffer, size_t length)
{
    if (!volume.mounted) return STORAGE_ERR_NOTMOUNTED;
    if (fd < 0 || fd >= 16 || !file_descriptors[fd].used || (!buffer && length))
        return STORAGE_ERR_BADFD;
    struct fat32_found found;
    char normalized[FAT32_MAX_PATH];
    int result = lookup_path(file_descriptors[fd].path, &found, normalized);
    if (result != STORAGE_OK) return result;
    result = file_write_at(&found, file_descriptors[fd].offset, buffer, length);
    if (result > 0) file_descriptors[fd].offset += (uint32_t)result;
    return result;
}

int fat32_seek(int fd, size_t offset)
{
    if (!volume.mounted) return STORAGE_ERR_NOTMOUNTED;
    if (fd < 0 || fd >= 16 || !file_descriptors[fd].used || offset > 0xffffffffu)
        return STORAGE_ERR_BADFD;
    file_descriptors[fd].offset = (uint32_t)offset;
    return STORAGE_OK;
}

int fat32_rename(const char *source, const char *destination)
{
    if (!volume.mounted) return STORAGE_ERR_NOTMOUNTED;
    struct fat32_found found, destination_entry;
    char source_path[FAT32_MAX_PATH], destination_path[FAT32_MAX_PATH];
    int result = lookup_path(source, &found, source_path);
    if (result != STORAGE_OK) return result;
    if (source_path[1] == 0) return STORAGE_ERR_INVAL;
    result = lookup_path(destination, &destination_entry, destination_path);
    if (result == STORAGE_OK) return STORAGE_ERR_EXIST;
    if (result != STORAGE_ERR_NOENT) return result;
    if ((found.attributes & 0x10) && string_length(destination_path) > string_length(source_path) &&
        name_equal(destination_path, source_path) && destination_path[string_length(source_path)] == '/')
        return STORAGE_ERR_INVAL;

    char parent_path[FAT32_MAX_PATH], name[FAT32_MAX_NAME];
    uint32_t parent;
    result = split_for_create(destination, parent_path, name, &parent);
    if (result != STORAGE_OK) return result;
    struct fat32_found moved;
    result = create_dir_entry(parent, name, found.attributes, found.cluster, found.size, &moved);
    if (result != STORAGE_OK) return result;

    if (found.attributes & 0x10) {
        uint8_t dotdot_sector[FAT32_SECTOR_SIZE];
        uint32_t first = cluster_sector(found.cluster);
        if (device_read(first, dotdot_sector) != STORAGE_OK) return STORAGE_ERR_IO;
        write16(dotdot_sector + 32 + 20, (uint16_t)(parent >> 16));
        write16(dotdot_sector + 32 + 26, (uint16_t)parent);
        if (device_write(first, dotdot_sector) != STORAGE_OK) return STORAGE_ERR_IO;
    }
    if (delete_found_entry(&found) != STORAGE_OK) return STORAGE_ERR_IO;
    for (unsigned int index = 0; index < 16; index++) {
        if (!file_descriptors[index].used) continue;
        if (name_equal(file_descriptors[index].path, source_path)) {
            string_copy(file_descriptors[index].path, destination_path,
                sizeof(file_descriptors[index].path));
        } else if ((found.attributes & 0x10) &&
            path_has_prefix(file_descriptors[index].path, source_path)) {
            char updated[FAT32_MAX_PATH];
            size_t destination_length = string_length(destination_path);
            string_copy(updated, destination_path, sizeof(updated));
            string_copy(updated + destination_length,
                file_descriptors[index].path + string_length(source_path),
                sizeof(updated) - destination_length);
            string_copy(file_descriptors[index].path, updated,
                sizeof(file_descriptors[index].path));
        }
    }
    index_valid = 0;
    return STORAGE_OK;
}

int fat32_copy(const char *source, const char *destination)
{
    if (!volume.mounted) return STORAGE_ERR_NOTMOUNTED;
    struct fat32_found found;
    char normalized[FAT32_MAX_PATH];
    int result = lookup_path(source, &found, normalized);
    if (result != STORAGE_OK) return result;
    if (found.attributes & 0x10) return STORAGE_ERR_ISDIR;
    result = create_entry_internal(destination, 'f', 0, 0);
    if (result != STORAGE_OK) return result;
    uint8_t buffer[512];
    uint32_t offset = 0;
    while (offset < found.size) {
        size_t part = found.size - offset > sizeof(buffer) ? sizeof(buffer) : found.size - offset;
        int count = file_read_at(found.cluster, found.size, offset, buffer, part);
        if (count < 0) { (void)fat32_unlink(destination); return count; }
        result = fat32_write_file(destination, buffer, (size_t)count, 1);
        if (result < 0) { (void)fat32_unlink(destination); return result; }
        offset += (uint32_t)count;
    }
    return STORAGE_OK;
}
