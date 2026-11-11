#include <stdint.h>

#include "drivers/printf.h"
#include "core/memory.h"
#include "x86_64/rtc.h"
#include "drivers/keyboard.h"
#include "apps/shell/shell.h"
#include "x86_64/port.h"
#include "custom/text.h"
#include <stdbool.h>


void kmain(uint64_t multiboot_magic,
           uint64_t multiboot_info_addr)
{
    console_clear();
    if (multiboot_magic == 0x36d76289u &&
        multiboot_info_addr != 0)
    {
        printf_(GREEN "###################################################\n" RESET);
        printf_(GREEN "####           ORTOS             3.3.9         ####\n" RESET);
        printf_(GREEN "####        Copyright (c) 2026 Yuhithan.       ####\n" RESET);
        printf_(GREEN "####                   MIT                     ####\n" RESET);
        printf_(GREEN "###################################################\n" RESET);
    }
    else
    {
        printf_("ERROR: invalid multiboot2 information.\n");
        halt();
    }
    memory_init();
    //keyboard_set_handler(handle_input);
    keyboard_init();

    printf_("Orange Tea OS kernel\n");
    printf_("Multiboot2 information received.\n");
    shell_loop();
    halt();
}