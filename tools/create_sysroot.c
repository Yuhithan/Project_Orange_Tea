#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define SECTOR_SIZE 512u
#define IMAGE_SECTORS 262144u
#define RESERVED_SECTORS 32u
#define FAT_COUNT 2u
#define ROOT_CLUSTER 2u
#define EOC 0x0fffffffu
#define MAX_NODES 512
#define MAX_NAME 256
#define MAX_PATH 1024

struct node {
    char name[MAX_NAME];
    int parent;
    int directory;
    uint32_t cluster;
    uint32_t size;
    uint8_t *data;
};

struct image {
    int fd;
    char *path;
    uint32_t fat_sectors;
    uint32_t cluster_count;
    uint32_t data_start;
    uint32_t next_cluster;
    uint32_t *fat;
    struct node nodes[MAX_NODES];
    int node_count;
};

static void put16(uint8_t *bytes, uint16_t value)
{
    bytes[0] = (uint8_t)value;
    bytes[1] = (uint8_t)(value >> 8);
}

static void put32(uint8_t *bytes, uint32_t value)
{
    bytes[0] = (uint8_t)value;
    bytes[1] = (uint8_t)(value >> 8);
    bytes[2] = (uint8_t)(value >> 16);
    bytes[3] = (uint8_t)(value >> 24);
}

static int write_at(struct image *image, uint64_t offset, const void *buffer, size_t length)
{
    const uint8_t *bytes = buffer;
    size_t written = 0;
    while (written < length) {
        ssize_t result = pwrite(image->fd, bytes + written, length - written,
            (off_t)(offset + written));
        if (result <= 0) return -1;
        written += (size_t)result;
    }
    return 0;
}

static int write_sector(struct image *image, uint32_t sector, const uint8_t data[SECTOR_SIZE])
{
    return write_at(image, (uint64_t)sector * SECTOR_SIZE, data, SECTOR_SIZE);
}

static uint32_t cluster_sector(const struct image *image, uint32_t cluster)
{
    return image->data_start + (cluster - 2);
}

static uint32_t allocate_chain(struct image *image, uint32_t count)
{
    if (!count) return 0;
    if (image->next_cluster + count > image->cluster_count + 2) return 0;
    uint32_t first = image->next_cluster;
    for (uint32_t index = 0; index < count; index++) {
        uint32_t cluster = first + index;
        image->fat[cluster] = index + 1 == count ? EOC : cluster + 1;
    }
    image->next_cluster += count;
    return first;
}

static int find_child(const struct image *image, int parent, const char *name)
{
    for (int index = 1; index < image->node_count; index++)
        if (image->nodes[index].parent == parent &&
            strcmp(image->nodes[index].name, name) == 0) return index;
    return -1;
}

static int add_node(struct image *image, int parent, const char *name, int directory)
{
    if (parent < 0 || parent >= image->node_count || !image->nodes[parent].directory ||
        !name[0] || strlen(name) >= MAX_NAME) return -1;
    int existing = find_child(image, parent, name);
    if (existing >= 0) return image->nodes[existing].directory == directory ? existing : -1;
    if (image->node_count == MAX_NODES) return -1;
    struct node *node = &image->nodes[image->node_count];
    memset(node, 0, sizeof(*node));
    strcpy(node->name, name);
    node->parent = parent;
    node->directory = directory;
    if (directory) {
        node->cluster = allocate_chain(image, 1);
        if (!node->cluster) return -1;
    }
    return image->node_count++;
}

static int ensure_directory(struct image *image, const char *path)
{
    char copy[MAX_PATH];
    if (strlen(path) >= sizeof(copy)) return -1;
    strcpy(copy, path);
    int parent = 0;
    char *part = copy;
    while (*part) {
        char *slash = strchr(part, '/');
        if (slash) *slash = 0;
        if (*part) {
            int next = find_child(image, parent, part);
            if (next < 0) next = add_node(image, parent, part, 1);
            if (next < 0 || !image->nodes[next].directory) return -1;
            parent = next;
        }
        if (!slash) break;
        part = slash + 1;
    }
    return parent;
}

static int read_file(const char *path, uint8_t **data, uint32_t *size)
{
    struct stat status;
    if (stat(path, &status) != 0 || status.st_size < 0 ||
        (uint64_t)status.st_size > 0xffffffffu) return -1;
    FILE *file = fopen(path, "rb");
    if (!file) return -1;
    size_t length = (size_t)status.st_size;
    uint8_t *buffer = length ? malloc(length) : 0;
    if (length && (!buffer || fread(buffer, 1, length, file) != length)) {
        free(buffer);
        fclose(file);
        return -1;
    }
    if (fclose(file) != 0) {
        free(buffer);
        return -1;
    }
    *data = buffer;
    *size = (uint32_t)length;
    return 0;
}

static int add_file(struct image *image, const char *path, const char *source)
{
    char copy[MAX_PATH];
    if (strlen(path) >= sizeof(copy)) return -1;
    strcpy(copy, path);
    char *slash = strrchr(copy, '/');
    char *name = slash ? slash + 1 : copy;
    if (slash) *slash = 0;
    int parent = slash ? ensure_directory(image, copy) : 0;
    if (parent < 0 || find_child(image, parent, name) >= 0) return -1;
    int index = add_node(image, parent, name, 0);
    if (index < 0 || read_file(source, &image->nodes[index].data,
        &image->nodes[index].size) != 0) return -1;

    uint32_t cluster_count = (image->nodes[index].size + SECTOR_SIZE - 1) / SECTOR_SIZE;
    if (cluster_count) {
        image->nodes[index].cluster = allocate_chain(image, cluster_count);
        if (!image->nodes[index].cluster) return -1;
        for (uint32_t offset = 0; offset < image->nodes[index].size; offset += SECTOR_SIZE) {
            uint8_t sector[SECTOR_SIZE] = {0};
            uint32_t remaining = image->nodes[index].size - offset;
            uint32_t amount = remaining < SECTOR_SIZE ? remaining : SECTOR_SIZE;
            memcpy(sector, image->nodes[index].data + offset, amount);
            uint32_t cluster = image->nodes[index].cluster + offset / SECTOR_SIZE;
            if (write_sector(image, cluster_sector(image, cluster), sector) != 0) return -1;
        }
    }
    return 0;
}

static uint8_t short_checksum(const uint8_t alias[11])
{
    uint8_t checksum = 0;
    for (unsigned int index = 0; index < 11; index++)
        checksum = (uint8_t)(((checksum & 1) ? 0x80 : 0) + (checksum >> 1) + alias[index]);
    return checksum;
}

static int make_alias(const char *name, unsigned int serial, uint8_t alias[11])
{
    memset(alias, ' ', 11);
    const char *dot = strrchr(name, '.');
    size_t base_length = dot ? (size_t)(dot - name) : strlen(name);
    char base[9] = {0}, extension[4] = {0};
    size_t base_used = 0, extension_used = 0;
    for (size_t index = 0; index < base_length && base_used < 8; index++) {
        unsigned char value = (unsigned char)name[index];
        if (value == ' ' || value == '.') continue;
        if (value >= 'a' && value <= 'z') value = (unsigned char)(value - ('a' - 'A'));
        if ((value >= 'A' && value <= 'Z') || (value >= '0' && value <= '9') ||
            value == '-' || value == '_' || value == '$' || value == '~') base[base_used++] = (char)value;
    }
    if (!base_used) base[base_used++] = 'F';
    if (dot) {
        for (const char *at = dot + 1; *at && extension_used < 3; at++) {
            unsigned char value = (unsigned char)*at;
            if (value >= 'a' && value <= 'z') value = (unsigned char)(value - ('a' - 'A'));
            if ((value >= 'A' && value <= 'Z') || (value >= '0' && value <= '9'))
                extension[extension_used++] = (char)value;
        }
    }

    char digits[8];
    unsigned int digit_count = 0;
    do {
        digits[digit_count++] = (char)('0' + serial % 10);
        serial /= 10;
    } while (serial && digit_count < sizeof(digits));
    size_t prefix = 8 - digit_count - 1;
    if (prefix > base_used) prefix = base_used;
    memcpy(alias, base, prefix);
    alias[prefix] = '~';
    for (unsigned int index = 0; index < digit_count; index++)
        alias[prefix + 1 + index] = (uint8_t)digits[digit_count - index - 1];
    memcpy(alias + 8, extension, extension_used);
    return 0;
}

static int alias_used(const uint8_t (*aliases)[11], int count, const uint8_t alias[11])
{
    for (int index = 0; index < count; index++)
        if (memcmp(aliases[index], alias, 11) == 0) return 1;
    return 0;
}

static int child_count(const struct image *image, int parent)
{
    int count = 0;
    for (int index = 1; index < image->node_count; index++)
        if (image->nodes[index].parent == parent) count++;
    return count;
}

static void write_lfn(uint8_t entry[32], const uint16_t *units, size_t unit_count,
    unsigned int ordinal, unsigned int total, uint8_t checksum)
{
    static const uint8_t offsets[13] = {
        1, 3, 5, 7, 9, 14, 16, 18, 20, 22, 24, 28, 30
    };
    memset(entry, 0, 32);
    entry[0] = (uint8_t)ordinal;
    if (ordinal == total) entry[0] |= 0x40;
    entry[11] = 0x0f;
    entry[13] = checksum;
    for (unsigned int index = 0; index < 13; index++) {
        size_t position = (ordinal - 1) * 13 + index;
        uint16_t value = position < unit_count ? units[position] :
            (position == unit_count ? 0 : 0xffff);
        put16(entry + offsets[index], value);
    }
}

static int append_child_entries(const struct image *image, int child,
    uint8_t *entries, size_t capacity, size_t *used,
    uint8_t (*aliases)[11], int *alias_count)
{
    const struct node *node = &image->nodes[child];
    size_t name_length = strlen(node->name);
    if (name_length > 255) return -1;
    uint16_t units[255];
    for (size_t index = 0; index < name_length; index++) {
        unsigned char value = (unsigned char)node->name[index];
        if (value >= 0x80) return -1;
        units[index] = value;
    }
    size_t unit_count = name_length;
    size_t padded = unit_count + 1 + ((13 - ((unit_count + 1) % 13)) % 13);
    unsigned int long_count = (unsigned int)(padded / 13);
    uint8_t alias[11];
    unsigned int serial = 1;
    do {
        make_alias(node->name, serial++, alias);
    } while (alias_used(aliases, *alias_count, alias));
    if (*alias_count == MAX_NODES) return -1;
    memcpy(aliases[(*alias_count)++], alias, 11);
    uint8_t checksum = short_checksum(alias);

    size_t needed = ((size_t)long_count + 1) * 32;
    if (*used + needed > capacity) return -1;
    for (unsigned int index = 0; index < long_count; index++) {
        uint8_t entry[32];
        unsigned int ordinal = long_count - index;
        write_lfn(entry, units, unit_count, ordinal, long_count, checksum);
        memcpy(entries + *used, entry, 32);
        *used += 32;
    }

    uint8_t *entry = entries + *used;
    memset(entry, 0, 32);
    memcpy(entry, alias, 11);
    entry[11] = node->directory ? 0x10 : 0x20;
    put16(entry + 16, 0x5d68);
    put16(entry + 18, 0x5d68);
    put16(entry + 20, (uint16_t)(node->cluster >> 16));
    put16(entry + 24, 0x5d68);
    put16(entry + 26, (uint16_t)node->cluster);
    put32(entry + 28, node->size);
    *used += 32;
    return 0;
}

static int render_directory(struct image *image, int index)
{
    struct node *directory = &image->nodes[index];
    size_t child_capacity = (size_t)child_count(image, index);
    uint8_t (*aliases)[11] = calloc(child_capacity ? child_capacity : 1, 11);
    size_t entry_capacity = (child_capacity * 21 + 3) * 32;
    uint8_t *entries = calloc(entry_capacity, 1);
    if (!aliases || !entries) {
        free(aliases);
        free(entries);
        return -1;
    }
    size_t used = 0;
    if (directory->parent >= 0) {
        int parent_cluster = directory->parent;
        uint8_t *entry = entries + used;
        memcpy(entry, ".          ", 11);
        entry[11] = 0x10;
        put16(entry + 20, (uint16_t)(directory->cluster >> 16));
        put16(entry + 26, (uint16_t)directory->cluster);
        used += 32;
        entry = entries + used;
        memcpy(entry, "..         ", 11);
        entry[11] = 0x10;
        put16(entry + 20, (uint16_t)(image->nodes[parent_cluster].cluster >> 16));
        put16(entry + 26, (uint16_t)image->nodes[parent_cluster].cluster);
        used += 32;
    }

    int alias_count = 0;
    for (int child = 1; child < image->node_count; child++) {
        if (image->nodes[child].parent != index) continue;
        if (append_child_entries(image, child, entries, entry_capacity, &used,
            aliases, &alias_count) != 0) {
            free(aliases);
            free(entries);
            return -1;
        }
    }
    used += 32;
    uint32_t clusters = (uint32_t)((used + SECTOR_SIZE - 1) / SECTOR_SIZE);
    if (clusters == 0) clusters = 1;
    if (clusters > 1) {
        uint32_t extra = allocate_chain(image, clusters - 1);
        if (!extra) {
            free(aliases);
            free(entries);
            return -1;
        }
        image->fat[directory->cluster] = extra;
    }
    uint32_t cluster = directory->cluster;
    size_t offset = 0;
    for (uint32_t index_in_chain = 0; index_in_chain < clusters; index_in_chain++) {
        for (unsigned int sector_index = 0; sector_index < 1; sector_index++) {
            uint8_t sector[SECTOR_SIZE] = {0};
            size_t remaining = used - offset;
            size_t amount = remaining < SECTOR_SIZE ? remaining : SECTOR_SIZE;
            if (amount) memcpy(sector, entries + offset, amount);
            if (write_sector(image, cluster_sector(image, cluster) + sector_index, sector) != 0) {
                free(aliases);
                free(entries);
                return -1;
            }
            offset += amount;
        }
        if (index_in_chain + 1 < clusters) cluster = image->fat[cluster];
    }
    free(aliases);
    free(entries);

    for (int child = 1; child < image->node_count; child++)
        if (image->nodes[child].parent == index && image->nodes[child].directory &&
            render_directory(image, child) != 0) return -1;
    return 0;
}

static int add_sysroot_assets(struct image *image,
    const char *kernel, const char *grub, const char *example)
{
    if (kernel && access(kernel, R_OK) == 0) {
        if (add_file(image, "Boot/Kernel/kernel.bin", kernel) != 0 ||
            add_file(image, "Windows/System32/Core/kernel.bin", kernel) != 0) return -1;
    }
    if (grub && access(grub, R_OK) == 0 &&
        add_file(image, "Boot/Configuration/grub.cfg", grub) != 0) return -1;
    if (example && access(example, R_OK) == 0) {
        if (ensure_directory(image, "Programs/Portable/Examples") < 0 ||
            add_file(image, "Programs/Portable/Examples/hello.ora", example) != 0) return -1;
    }
    return 0;
}

static int format_image(struct image *image)
{
    uint8_t zero[SECTOR_SIZE] = {0};
    uint8_t boot[SECTOR_SIZE] = {0};
    uint32_t fat_size = image->fat_sectors;

    if (render_directory(image, 0) != 0) return -1;
    for (uint32_t copy = 0; copy < FAT_COUNT; copy++) {
        uint32_t start = RESERVED_SECTORS + copy * fat_size;
        if (write_at(image, (uint64_t)start * SECTOR_SIZE, image->fat,
            (size_t)fat_size * SECTOR_SIZE) != 0) return -1;
    }

    boot[0] = 0xeb;
    boot[1] = 0x58;
    boot[2] = 0x90;
    memcpy(boot + 3, "ORTOSFAT", 8);
    put16(boot + 11, SECTOR_SIZE);
    boot[13] = 1;
    put16(boot + 14, RESERVED_SECTORS);
    boot[16] = FAT_COUNT;
    put32(boot + 32, IMAGE_SECTORS);
    put32(boot + 36, fat_size);
    put32(boot + 44, ROOT_CLUSTER);
    put16(boot + 48, 1);
    put16(boot + 50, 6);
    boot[64] = 0x80;
    boot[66] = 0x29;
    put32(boot + 67, 0x4f52544f);
    memcpy(boot + 71, "ORANGE TEA ", 11);
    memcpy(boot + 82, "FAT32   ", 8);
    boot[510] = 0x55;
    boot[511] = 0xaa;
    if (write_sector(image, 0, boot) != 0 || write_sector(image, 6, boot) != 0)
        return -1;

    uint8_t fsinfo[SECTOR_SIZE] = {0};
    put32(fsinfo, 0x41615252);
    put32(fsinfo + 484, 0x61417272);
    put32(fsinfo + 488, image->cluster_count - (image->next_cluster - 2));
    put32(fsinfo + 492, image->next_cluster);
    put32(fsinfo + 508, 0xaa550000);
    if (write_sector(image, 1, fsinfo) != 0 || write_sector(image, 7, fsinfo) != 0)
        return -1;
    for (uint32_t sector = 2; sector < RESERVED_SECTORS; sector++) {
        if (sector == 6 || sector == 7) continue;
        if (write_sector(image, sector, zero) != 0) return -1;
    }
    return fsync(image->fd);
}

int main(int argc, char **argv)
{
    static const char *const directories[] = {
        "Windows/System32/Drivers", "Windows/System32/Libraries",
        "Windows/System32/Core", "Windows/System32/Services",
        "Windows/Resources", "Windows/Configuration", "Windows/Fonts",
        "Windows/Themes", "Windows/Logs", "Programs/Installed",
        "Programs/Portable", "Programs/Shared", "Users/Default/Desktop",
        "Users/Default/Documents", "Users/Default/Downloads",
        "Users/Default/Pictures", "Users/Default/Music", "Users/Default/Videos",
        "Users/Default/AppData", "Users/Public", "Desktop", "Documents",
        "Downloads", "Pictures", "Music", "Videos", "ProgramData", "Temp",
        "Recovery", "Boot/Kernel", "Boot/Bootloader", "Boot/Configuration"
    };
    if (argc != 6) {
        fprintf(stderr, "usage: %s IMAGE SYSROOT KERNEL GRUB_CONFIG EXAMPLE_APP\n", argv[0]);
        return 2;
    }

    struct image image;
    memset(&image, 0, sizeof(image));
    image.path = argv[1];
    image.fd = open(image.path, O_CREAT | O_EXCL | O_RDWR, 0644);
    if (image.fd < 0) {
        if (errno == EEXIST) {
            printf("Preserving existing disk image: %s\n", image.path);
            return 0;
        }
        perror("create FAT32 image");
        return 1;
    }
    if (ftruncate(image.fd, (off_t)IMAGE_SECTORS * SECTOR_SIZE) != 0) {
        perror("size FAT32 image");
        close(image.fd);
        return 1;
    }

    uint32_t fat_sectors = 1;
    for (;;) {
        uint32_t clusters = IMAGE_SECTORS - RESERVED_SECTORS - FAT_COUNT * fat_sectors;
        uint32_t needed = (uint32_t)(((uint64_t)(clusters + 2) * 4 + SECTOR_SIZE - 1) /
            SECTOR_SIZE);
        if (needed == fat_sectors) break;
        fat_sectors = needed;
    }
    image.fat_sectors = fat_sectors;
    image.cluster_count = IMAGE_SECTORS - RESERVED_SECTORS - FAT_COUNT * fat_sectors;
    image.data_start = RESERVED_SECTORS + FAT_COUNT * fat_sectors;
    image.next_cluster = ROOT_CLUSTER + 1;
    image.fat = calloc((size_t)fat_sectors * SECTOR_SIZE / sizeof(uint32_t), sizeof(uint32_t));
    image.node_count = 1;
    image.nodes[0].directory = 1;
    image.nodes[0].cluster = ROOT_CLUSTER;
    image.nodes[0].parent = -1;
    if (!image.fat) {
        fprintf(stderr, "FAT table allocation failed\n");
        close(image.fd);
        return 1;
    }
    image.fat[0] = 0x0ffffff8;
    image.fat[1] = 0xffffffff;
    image.fat[ROOT_CLUSTER] = EOC;

    int result = 0;
    for (size_t index = 0; index < sizeof(directories) / sizeof(directories[0]); index++)
        if (ensure_directory(&image, directories[index]) < 0) result = -1;
    if (!result && add_sysroot_assets(&image, argv[3], argv[4], argv[5]) != 0)
        result = -1;
    if (!result && format_image(&image) != 0) result = -1;
    if (result) fprintf(stderr, "FAT32 sysroot image creation failed: %s\n", strerror(errno));
    else printf("Created FAT32 runtime image: %s\n", image.path);

    for (int index = 0; index < image.node_count; index++) free(image.nodes[index].data);
    free(image.fat);
    if (close(image.fd) != 0 && !result) result = -1;
    return result ? 1 : 0;
}
