#include "imp.h"
#include "shell.h"
#include "keyboard.h"
#include "network.h"
#include "boot_mode.h"

void kmain(uint64_t multiboot_magic, uint64_t multiboot_info_addr) {
    (void)multiboot_magic;
    (void)multiboot_info_addr;

    imp_text("BOOT 1: kernel entry\n");

    ortos_boot_mode_t mode = ortos_boot_mode_get();
    if (mode == ORTOS_BOOT_GUI) {
        imp_text("Boot mode: GUI\n");
    } else {
        imp_text("Boot mode: SHELL\n");
    }

    if (mode == ORTOS_BOOT_GUI) {
        /* GUI boot is disabled until the framebuffer path is stable.  Clear
         * the request so this reboot and every following boot enter the shell. */
        ortos_boot_mode_clear();
        imp_text("GUI mode is disabled; starting the shell.\n");
    }

    enable_key_input();
    enable_network();
    imp_cls();
    imp_color(PRINT_COLOR_BLACK, PRINT_COLOR_WHITE);
    imp_text("===========================================\n");
    imp_text("Welcome to Orange Tea OS!\n");
    imp_text("==========================================\n");
    imp_text("Boot mode: SHELL\n");

    shell_init();
    shell_run();
}
