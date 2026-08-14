#include "imp.h"
#include "shell.h"
#include "keyboard.h"
#include "network.h"
#include "boot_mode.h"
#include "test.h"

extern int gui_start(uint64_t multiboot_info_addr);

static void start_shell(void)
{
    enable_key_input();
    enable_network();

    imp_cls();
    imp_color(PRINT_COLOR_BLACK, PRINT_COLOR_WHITE);

    imp_text("===========================================\n");
    imp_text("       Welcome to Orange Tea OS!\n");
    imp_text("===========================================\n");

    imp_text("Boot mode: SHELL\n");

    shell_init();
    shell_run();
}

void kmain(uint64_t multiboot_magic, uint64_t multiboot_info_addr)
{
    imp_text("BOOT 1: kernel entry\n");

    if (multiboot_magic == 0x36d76289u &&
        multiboot_info_addr != 0)
    {
        imp_text("BOOT 2: multiboot2 information received\n");
        imp_text("BOOT 3: multiboot2 information parsed\n");
    }
    else
    {
        imp_text("ERROR: invalid multiboot2 information.\n");
        return;
    }

    /*
     * Start in SHELL mode.
     */
    ortos_boot_mode_set(ORTOS_BOOT_MODE_SHELL);

    imp_text("BOOT MODE: setting SHELL...\n");

    /*
     * Test boot_mode.
     */
    test_boot();

    /*
     * Continue into the shell.
     */
    imp_text("Starting shell...\n");

    start_shell();
}