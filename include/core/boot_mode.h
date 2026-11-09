#pragma once

#ifndef BOOT_MODE_H
#define BOOT_MODE_H

typedef enum {
    ORTOS_BOOT_MODE_SHELL = 0
} ortos_boot_mode_t;

/* Set the current boot mode. */
void ortos_boot_mode_set(ortos_boot_mode_t mode);

/* Reset mode to the VGA shell. */
void ortos_boot_mode_clear(void);

/* Get the current mode */
ortos_boot_mode_t ortos_boot_mode_get(void);

/* Reboot the machine */
void ortos_reboot(void);

#endif