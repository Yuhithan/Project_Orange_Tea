#include "imp.h"
#include "shell.h"
#include "keyboard.h"
#include "network.h"
#include "boot_mode.h"

extern int gui_start(uint64_t multiboot_info_addr);

static void start_shell(void)
{
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

void kmain(uint64_t multiboot_magic, uint64_t multiboot_info_addr) {
    imp_text("BOOT 1: kernel entry\n");
    if (multiboot_magic == 0x36d76289u && multiboot_info_addr != 0) {
        imp_text("BOOT 2: multiboot2 information received\n");
        imp_text("BOOT 3: multiboot2 information parsed\n");
    } else {
        imp_text("ERROR: invalid multiboot2 information.\n");
    }

    ortos_boot_mode_t mode = ortos_boot_mode_get();
    if (mode == ORTOS_BOOT_GUI) {
        imp_text("Boot mode: GUI\n");
    } else {
        imp_text("Boot mode: SHELL\n");
    }

    if (mode == ORTOS_BOOT_GUI) {
        ortos_boot_mode_clear();
        if (multiboot_magic == 0x36d76289u && multiboot_info_addr != 0) {
            enable_key_input();
            if (gui_start(multiboot_info_addr)) {
                for (;;)
                    asm volatile ("hlt");
            }
        }

        imp_text("GUI boot requested.\n");
        imp_text("ERROR: framebuffer unavailable.\n");
        imp_text("Falling back to shell.\n");
    }

    start_shell();
}
