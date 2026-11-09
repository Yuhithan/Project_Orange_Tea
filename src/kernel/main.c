#include <stdint.h>

#include "core/print.h"
#include "core/irq.h"
#include "core/memory.h"

static void halt(void)
{
    for (;;) {
        __asm__ volatile ("hlt");
    }
}

void kmain(uint64_t multiboot_magic,
           uint64_t multiboot_info_addr)
{
    print_clear();
    if (multiboot_magic == 0x36d76289u &&
        multiboot_info_addr != 0)
    {
        print_str("Orange Tea OS minimal kernel\n");
        print_str("Multiboot2 information received.\n");
    }
    else
    {
        print_str("ERROR: invalid multiboot2 information.\n");
        halt();
    }

    memory_init();
    irq_init();

    print_str("Memory initialized. Exception handling enabled.\n");
    print_str("Kernel halted; VGA text output is ready.\n");

    print_str("=============================================\n");
    print_str("======================ORT====================\n");
    print_str("=============================================\n");
    
    halt();
}