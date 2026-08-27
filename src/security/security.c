#include "security/security.h"

int ortos_security_authenticate(const char *user, const void *credential,
                                size_t credential_length)
{
    (void)user;
    (void)credential;
    (void)credential_length;
    return ORTOS_SECURITY_ERR_NOT_SUPPORTED;
}

int ortos_security_check_file(uint32_t user_id, uint32_t mode, int requested_access)
{
    (void)user_id;
    (void)mode;
    (void)requested_access;
    return ORTOS_SECURITY_ERR_DENIED;
}

int ortos_security_launch_sandboxed(const void *image, size_t image_length)
{
    (void)image;
    (void)image_length;
    return ORTOS_SECURITY_ERR_NOT_SUPPORTED;
}

int ortos_security_disk_encrypt(const void *device, size_t device_length)
{
    (void)device;
    (void)device_length;
    return ORTOS_SECURITY_ERR_NOT_SUPPORTED;
}