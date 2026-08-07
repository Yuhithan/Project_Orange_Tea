#include "imp.h"
#include "shell.h"
#include "keyboard.h"
#include "framebuffer.h"
#include "network.h"
#include "boot_mode.h"

extern void gui_start(uint64_t multiboot_info_addr);

void kmain(uint64_t multiboot_magic, uint64_t multiboot_info_addr) {
    (void)multiboot_magic;

    ortos_boot_mode_t mode = ortos_boot_mode_get();

    if (mode == ORTOS_BOOT_GUI) {
        ortos_boot_mode_clear();
        fb_init(0, 0, 0, 0);
        fb_init_from_multiboot(multiboot_info_addr);

        if (framebuffer_ready) {
            imp_text("ORTos kernel starting...\n");
            imp_text("Boot mode: GUI\n");
            imp_text("Starting ORgui...\n");
            gui_start(multiboot_info_addr);
            return;
        }

        imp_text("GUI boot requested but no usable framebuffer was found.\n");
        imp_text("Falling back to shell mode.\n");
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