#pragma once

#include <stddef.h>
#include <stdint.h>

enum {
    ORTOS_SECURITY_OK = 0,
    ORTOS_SECURITY_ERR_INVAL = -1,
    ORTOS_SECURITY_ERR_NOT_SUPPORTED = -2,
    ORTOS_SECURITY_ERR_DENIED = -3
};

/* These APIs fail closed until identity, isolation and crypto providers exist. */
int ortos_security_authenticate(const char *user, const void *credential,
                                size_t credential_length);
int ortos_security_check_file(uint32_t user_id, uint32_t mode, int requested_access);
int ortos_security_launch_sandboxed(const void *image, size_t image_length);
int ortos_security_disk_encrypt(const void *device, size_t device_length);