#pragma once

#include <stdint.h>

typedef enum {
    ORTOS_BOOT_SHELL = 0,
    ORTOS_BOOT_GUI = 1
} ortos_boot_mode_t;

void ortos_boot_mode_request_gui(void);
void ortos_boot_mode_request_shell(void);
void ortos_boot_mode_clear(void);
ortos_boot_mode_t ortos_boot_mode_get(void);
void ortos_reboot(void);
