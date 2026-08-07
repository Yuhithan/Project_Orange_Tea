#include "boot_mode.h"

#define ORTOS_BOOT_MAGIC 0x4F525447u
#define ORTOS_BOOT_GUI_MAGIC 0x47554901u
#define ORTOS_BOOT_FLAG_ADDR 0x4000u

static volatile uint32_t *const boot_flag = (volatile uint32_t *)ORTOS_BOOT_FLAG_ADDR;

static int ortos_boot_flag_is_valid(void)
{
    return *boot_flag == ORTOS_BOOT_MAGIC;
}

void ortos_boot_mode_request_gui(void)
{
    *boot_flag = ORTOS_BOOT_MAGIC;
    *(boot_flag + 1) = ORTOS_BOOT_GUI_MAGIC;
}

void ortos_boot_mode_request_shell(void)
{
    *boot_flag = ORTOS_BOOT_MAGIC;
    *(boot_flag + 1) = 0u;
}

void ortos_boot_mode_clear(void)
{
    *boot_flag = 0u;
    *(boot_flag + 1) = 0u;
}

ortos_boot_mode_t ortos_boot_mode_get(void)
{
    if (!ortos_boot_flag_is_valid()) {
        return ORTOS_BOOT_SHELL;
    }

    if (*(boot_flag + 1) == ORTOS_BOOT_GUI_MAGIC) {
        return ORTOS_BOOT_GUI;
    }

    return ORTOS_BOOT_SHELL;
}

void ortos_reboot(void)
{
    asm volatile ("cli");
    for (int i = 0; i < 10000; i++) {
        asm volatile ("outb %%al, $0x64" : : "a"(0xFE));
    }
    for (;;)
        asm volatile ("hlt");
}
