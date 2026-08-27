#include "imp.h"
#include "shell.h"
#include "keyboard.h"
#include "network.h"
#include "boot_mode.h"
#include "test.h"
#include "timer.h"
#include "framebuffer.h"
#include "desktop.h"
#include "irq.h"
#include "memory.h"
#include "process.h"
#include "cursor.h"


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


void kmain(uint64_t multiboot_magic,
           uint64_t multiboot_info_addr)
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


    /* Initialize framebuffer. */
    fb_init(multiboot_info_addr);

    /* Initialize memory/processes. */
    memory_init();
    process_init();


    /*
     * Initialize interrupts and timer.
     */
    irq_init();
    timer_init(1000);


    /*
     * GUI mode.
     */
    /*
     * GUI mode.
     */
    if (fb_is_available())
    {
        ortos_boot_mode_set(ORTOS_BOOT_MODE_GUI);

        enable_key_input();
        enable_network();

        imp_text("Starting graphical desktop...\n");

    /*
     * Start desktop.
     */
    //desktop_init(0);
    //desktop_run();
    }


    /*
     * Shell mode.
     */
    imp_text("No compatible framebuffer; starting shell...\n");

    imp_text("Shell mode has start\n");

    ortos_boot_mode_set(ORTOS_BOOT_MODE_SHELL);

    start_shell();
}