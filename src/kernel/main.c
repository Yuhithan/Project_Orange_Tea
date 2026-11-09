#include "imp.h"
#include "drivers/audio.h"
#include "shell.h"
#include "keyboard.h"
#include "network.h"
#include "boot_mode.h"
#include "timer.h"
#include "irq.h"
#include "memory.h"
#include "process.h"
#include "ata.h"
#include "storage.h"
#include "vfs.h"
#include "drivers/vga.h"


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
    vga_init();

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


    /* Initialize memory/processes. */
    memory_init();
    process_init();

    if (ata_primary_master_init() == 0 && vfs_attach_block_device(ata_primary_master_device()) == STORAGE_OK)
        imp_text("Storage: ATA disk0 detected\n");
    else
        imp_text("Storage: no supported disk detected\n");
    vfs_init();


    /*
     * Initialize interrupts and timer.
     */
    irq_init();
    timer_init(1000);
    audio_init();
    storage_set_clock(timer_get_ticks);


    ortos_boot_mode_set(ORTOS_BOOT_MODE_SHELL);
    start_shell();
}