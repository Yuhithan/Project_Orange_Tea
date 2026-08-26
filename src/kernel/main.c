#include "imp.h"
#include "shell.h"
#include "keyboard.h"
#include "network.h"
#include "boot_mode.h"
#include "test.h"
#include "timer.h"
#include "framebuffer.h"
#include "irq.h"
#include "memory.h"
#include "process.h"

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

    fb_init(multiboot_info_addr);
    memory_init();
    process_init();

    /* Start in shell mode; the shell can enter the framebuffer desktop. */
    ortos_boot_mode_set(ORTOS_BOOT_MODE_SHELL);

    imp_text("BOOT MODE: setting SHELL...\n");

    /*
     * Test boot_mode.
     */
    //test_boot();

    irq_init();
    timer_init(1000);

    /*
     * Continue into the shell.
     */
    imp_text("Starting shell...\n");

    start_shell();
}
