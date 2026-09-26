#include "update.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

typedef struct {
    unsigned int state[8];
    unsigned long long length;
    unsigned char block[64];
    size_t used;
} sha256_t;

static const unsigned int k[64] = {
    0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u,
    0x923f82a4u, 0xab1c5ed5u, 0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u,
    0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u, 0xe49b69c1u, 0xefbe4786u,
    0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
    0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u,
    0x06ca6351u, 0x14292967u, 0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u,
    0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u, 0xa2bfe8a1u, 0xa81a664bu,
    0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
    0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au,
    0x5b9cca4fu, 0x682e6ff3u, 0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
    0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u
};

static unsigned int rotate_right(unsigned int value, unsigned int count)
{ return (value >> count) | (value << (32u - count)); }
static unsigned int word(const unsigned char *p)
{ return ((unsigned int)p[0] << 24) | ((unsigned int)p[1] << 16) | ((unsigned int)p[2] << 8) | p[3]; }

static void sha256_block(sha256_t *hash)
{
    unsigned int w[64], a, b, c, d, e, f, g, h;
    for (unsigned int i = 0; i < 16u; ++i) w[i] = word(hash->block + i * 4u);
    for (unsigned int i = 16u; i < 64u; ++i) {
        unsigned int s0 = rotate_right(w[i - 15u], 7u) ^ rotate_right(w[i - 15u], 18u) ^ (w[i - 15u] >> 3u);
        unsigned int s1 = rotate_right(w[i - 2u], 17u) ^ rotate_right(w[i - 2u], 19u) ^ (w[i - 2u] >> 10u);
        w[i] = w[i - 16u] + s0 + w[i - 7u] + s1;
    }
    a = hash->state[0]; b = hash->state[1]; c = hash->state[2]; d = hash->state[3];
    e = hash->state[4]; f = hash->state[5]; g = hash->state[6]; h = hash->state[7];
    for (unsigned int i = 0; i < 64u; ++i) {
        unsigned int s1 = rotate_right(e, 6u) ^ rotate_right(e, 11u) ^ rotate_right(e, 25u);
        unsigned int choose = (e & f) ^ ((~e) & g);
        unsigned int temporary1 = h + s1 + choose + k[i] + w[i];
        unsigned int s0 = rotate_right(a, 2u) ^ rotate_right(a, 13u) ^ rotate_right(a, 22u);
        unsigned int majority = (a & b) ^ (a & c) ^ (b & c);
        unsigned int temporary2 = s0 + majority;
        h = g; g = f; f = e; e = d + temporary1; d = c; c = b; b = a; a = temporary1 + temporary2;
    }
    hash->state[0] += a; hash->state[1] += b; hash->state[2] += c; hash->state[3] += d;
    hash->state[4] += e; hash->state[5] += f; hash->state[6] += g; hash->state[7] += h;
}

static void sha256_init(sha256_t *hash)
{
    static const unsigned int initial[8] = { 0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
        0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u };
    memcpy(hash->state, initial, sizeof(initial)); hash->length = 0; hash->used = 0;
}
static void sha256_update(sha256_t *hash, const void *data, size_t length)
{
    const unsigned char *bytes = data;
    hash->length += length;
    while (length) { size_t count = 64u - hash->used; if (count > length) count = length; memcpy(hash->block + hash->used, bytes, count); hash->used += count; bytes += count; length -= count; if (hash->used == 64u) { sha256_block(hash); hash->used = 0; } }
}
static void sha256_final(sha256_t *hash, unsigned char output[32])
{
    unsigned long long bits = hash->length * 8u; size_t index = hash->used;
    hash->block[index++] = 0x80u; while (index != 56u) { if (index == 64u) { sha256_block(hash); index = 0; } hash->block[index++] = 0; }
    for (int i = 7; i >= 0; --i) hash->block[index++] = (unsigned char)(bits >> (i * 8));
    sha256_block(hash);
    for (unsigned int i = 0; i < 8u; ++i) for (unsigned int j = 0; j < 4u; ++j) output[i * 4u + j] = (unsigned char)(hash->state[i] >> (24u - j * 8u));
}

static void set_error(char *error, size_t size, const char *message)
{ if (error && size) { snprintf(error, size, "%s", message); } }
static int copy_field(char *destination, size_t capacity, const char *value)
{ if (strlen(value) >= capacity) return -1; snprintf(destination, capacity, "%s", value); return 0; }
static int parse_size(const char *value, unsigned long long *result)
{ char *end; unsigned long long parsed = strtoull(value, &end, 10); if (*value == 0 || *end != 0) return -1; *result = parsed; return 0; }

int ortos_update_load_manifest(const char *path, ortos_update_manifest_t *manifest, char *error, size_t error_size)
{
    FILE *file; char line[512];
    if (!path || !manifest) { set_error(error, error_size, "manifest path is required"); return -1; }
    memset(manifest, 0, sizeof(*manifest)); file = fopen(path, "r");
    if (!file) { set_error(error, error_size, "cannot open update manifest"); return -1; }
    while (fgets(line, sizeof(line), file)) {
        char *separator = strchr(line, '='); char *value;
        if (!separator) continue; *separator = 0; value = separator + 1; value[strcspn(value, "\r\n")] = 0;
        if (strcmp(line, "version") == 0) { if (copy_field(manifest->version, sizeof(manifest->version), value)) goto invalid; }
        else if (strcmp(line, "architecture") == 0) { if (copy_field(manifest->architecture, sizeof(manifest->architecture), value)) goto invalid; }
        else if (strcmp(line, "min_version") == 0) { if (copy_field(manifest->minimum_version, sizeof(manifest->minimum_version), value)) goto invalid; }
        else if (strcmp(line, "package") == 0) { if (copy_field(manifest->package, sizeof(manifest->package), value)) goto invalid; }
        else if (strcmp(line, "sha256") == 0) { if (copy_field(manifest->sha256, sizeof(manifest->sha256), value)) goto invalid; }
        else if (strcmp(line, "size") == 0 && parse_size(value, &manifest->size)) goto invalid;
    }
    fclose(file);
    if (!manifest->version[0] || !manifest->architecture[0] || !manifest->package[0] ||
        strlen(manifest->sha256) != 64u || !manifest->size) goto invalid;
    return 0;
invalid:
    fclose(file); set_error(error, error_size, "invalid update manifest"); return -1;
}

static int version_at_least(const char *current, const char *minimum)
{
    unsigned int a, b, c, x, y, z;
    if (!minimum[0]) return 1;
    if (sscanf(current, "%u.%u.%u", &a, &b, &c) != 3 || sscanf(minimum, "%u.%u.%u", &x, &y, &z) != 3) return 0;
    return a > x || (a == x && (b > y || (b == y && c >= z)));
}

static int package_path(const char *manifest_path, const char *package, char output[ORTOS_UPDATE_PATH_SIZE])
{
    const char *slash = strrchr(manifest_path, '/'); size_t length = slash ? (size_t)(slash - manifest_path + 1u) : 0u;
    if (package[0] == '/' || length + strlen(package) + 1u > ORTOS_UPDATE_PATH_SIZE) return -1;
    if (length) memcpy(output, manifest_path, length); memcpy(output + length, package, strlen(package) + 1u); return 0;
}

static int verify_package(const char *path, const ortos_update_manifest_t *manifest)
{
    FILE *file = fopen(path, "rb"); unsigned char buffer[4096], digest[32]; char actual[65]; size_t count; sha256_t hash; unsigned long long total = 0;
    if (!file) return -1; sha256_init(&hash);
    while ((count = fread(buffer, 1, sizeof(buffer), file)) != 0) { sha256_update(&hash, buffer, count); total += count; }
    fclose(file); sha256_final(&hash, digest);
    for (unsigned int i = 0; i < 32u; ++i) snprintf(actual + i * 2u, 3, "%02x", digest[i]);
    return total == manifest->size && strcmp(actual, manifest->sha256) == 0 ? 0 : -1;
}

static int compatibility(const ortos_update_manifest_t *manifest, const char *root, char *error, size_t error_size)
{
    char path[ORTOS_UPDATE_PATH_SIZE], current[ORTOS_UPDATE_TEXT_SIZE] = "0.0.0"; FILE *file;
    if (strcmp(manifest->architecture, "x86_64") != 0) { set_error(error, error_size, "update architecture is not x86_64"); return -1; }
    snprintf(path, sizeof(path), "%s/ortos.version", root); file = fopen(path, "r");
    if (file) { if (!fgets(current, sizeof(current), file)) current[0] = 0; fclose(file); current[strcspn(current, "\r\n")] = 0; }
    if (!version_at_least(current, manifest->minimum_version)) { set_error(error, error_size, "current OS version is incompatible"); return -1; }
    return 0;
}

int ortos_update_check(const char *manifest_path, const char *install_root, char *error, size_t error_size)
{
    ortos_update_manifest_t manifest; char package[ORTOS_UPDATE_PATH_SIZE];
    if (ortos_update_load_manifest(manifest_path, &manifest, error, error_size) || compatibility(&manifest, install_root, error, error_size)) return -1;
    if (package_path(manifest_path, manifest.package, package) || verify_package(package, &manifest)) { set_error(error, error_size, "update package hash or size mismatch"); return -1; }
    return 0;
}

int ortos_update_apply(const char *manifest_path, const char *install_root, char *error, size_t error_size)
{
    ortos_update_manifest_t manifest; char package[ORTOS_UPDATE_PATH_SIZE], current[ORTOS_UPDATE_PATH_SIZE], backup[ORTOS_UPDATE_PATH_SIZE], temporary[ORTOS_UPDATE_PATH_SIZE];
    FILE *input, *output; unsigned char buffer[4096]; size_t count; int had_current;
    if (ortos_update_check(manifest_path, install_root, error, error_size)) return -1;
    ortos_update_load_manifest(manifest_path, &manifest, error, error_size); package_path(manifest_path, manifest.package, package);
    snprintf(current, sizeof(current), "%s/kernel.bin", install_root); snprintf(backup, sizeof(backup), "%s/kernel.bin.previous", install_root); snprintf(temporary, sizeof(temporary), "%s/kernel.bin.new", install_root);
    input = fopen(package, "rb"); output = fopen(temporary, "wb"); if (!input || !output) { if (input) fclose(input); if (output) fclose(output); remove(temporary); set_error(error, error_size, "cannot stage update package"); return -1; }
    while ((count = fread(buffer, 1, sizeof(buffer), input)) != 0) if (fwrite(buffer, 1, count, output) != count) { fclose(input); fclose(output); remove(temporary); set_error(error, error_size, "cannot write staged update"); return -1; }
    fclose(input); if (fclose(output) != 0) { remove(temporary); set_error(error, error_size, "cannot flush staged update"); return -1; }
    had_current = access(current, F_OK) == 0; if (had_current) { remove(backup); if (rename(current, backup) != 0) { remove(temporary); set_error(error, error_size, "cannot preserve previous kernel"); return -1; } }
    if (rename(temporary, current) != 0) { if (had_current) rename(backup, current); remove(temporary); set_error(error, error_size, "cannot activate update"); return -1; }
    return 0;
}