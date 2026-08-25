#pragma once

#ifndef BOOT_MODE_H
#define BOOT_MODE_H

typedef enum {
    ORTOS_BOOT_MODE_SHELL = 0,
    ORTOS_BOOT_MODE_GUI = 1,
    ORTOS_BOOT_MODE_FRAMEBUFFER = 2
} ortos_boot_mode_t;

/* Set the current boot/display mode */
void ortos_boot_mode_set(ortos_boot_mode_t mode);

/* Reset mode back to VGA */
void ortos_boot_mode_clear(void);

/* Get the current mode */
ortos_boot_mode_t ortos_boot_mode_get(void);

/* Reboot the machine */
void ortos_reboot(void);

#endif