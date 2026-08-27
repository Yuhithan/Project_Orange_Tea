#include "boot_mode.h"
#include <stdint.h>

static ortos_boot_mode_t current_mode = ORTOS_BOOT_MODE_GUI;

void ortos_boot_mode_set(ortos_boot_mode_t mode)
{
    current_mode = mode;
}

void ortos_boot_mode_clear(void)
{
    current_mode = ORTOS_BOOT_MODE_SHELL;
}

ortos_boot_mode_t ortos_boot_mode_get(void)
{
    return current_mode;
}

void ortos_reboot(void)
{
    /*
     * Ask the CPU to reboot through the keyboard controller.
     */

    uint8_t good = 0x02;

    while (good & 0x02) {
        __asm__ volatile (
            "inb $0x64, %0"
            : "=a"(good)
        );
    }

    __asm__ volatile (
        "outb %0, $0x64"
        :
        : "a"((uint8_t)0xFE)
    );

    /* If reboot failed, stop the CPU. */
    for (;;) {
        __asm__ volatile ("cli; hlt");
    }
}