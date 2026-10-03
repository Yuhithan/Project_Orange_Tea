#pragma once

#include <stddef.h>

#define ORTOS_UPDATE_TEXT_SIZE 64u
#define ORTOS_UPDATE_PATH_SIZE 256u
#define ORTOS_UPDATE_HASH_SIZE 65u

typedef struct {
    char version[ORTOS_UPDATE_TEXT_SIZE];
    char architecture[ORTOS_UPDATE_TEXT_SIZE];
    char minimum_version[ORTOS_UPDATE_TEXT_SIZE];
    char package[ORTOS_UPDATE_PATH_SIZE];
    char sha256[ORTOS_UPDATE_HASH_SIZE];
    unsigned long long size;
} ortos_update_manifest_t;

int ortos_update_load_manifest(const char *path,
                               ortos_update_manifest_t *manifest,
                               char *error, size_t error_size);
int ortos_update_check(const char *manifest_path, const char *install_root,
                       char *error, size_t error_size);
int ortos_update_apply(const char *manifest_path, const char *install_root,
                       char *error, size_t error_size);