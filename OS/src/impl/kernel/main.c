#include "imp.h"
#include "shell.h"
#include "keyboard.h"
#include "framebuffer.h"
#include "network.h"
#include "boot_mode.h"
#include "desktop.h"

void kmain(uint64_t multiboot_magic, uint64_t multiboot_info_addr) {
    (void)multiboot_magic;

    imp_text("BOOT 1: kernel entry\n");

    ortos_boot_mode_t mode = ortos_boot_mode_get();
    if (mode == ORTOS_BOOT_GUI) {
        imp_text("Boot mode: GUI\n");
    } else {
        imp_text("Boot mode: SHELL\n");
    }

    if (mode == ORTOS_BOOT_GUI) {
        ortos_boot_mode_clear();
        imp_text("BOOT 2: multiboot2 information received\n");
        fb_init_from_multiboot(multiboot_info_addr);
        imp_text("BOOT 3: multiboot2 information parsed\n");

        if (framebuffer_ready) {
            imp_text("BOOT 6: framebuffer initialized\n");
            imp_text("BOOT 7: entering GUI mode\n");
            enable_key_input();
            desktop_init();
            desktop_draw();
            imp_text("GUI closed; returning to shell mode.\n");
        } else {
            imp_text("GUI boot requested but no usable framebuffer was found.\n");
            imp_text("Falling back to shell mode.\n");
        }
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
