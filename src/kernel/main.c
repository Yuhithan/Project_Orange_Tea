#include <stdint.h>

#include "drivers/printf.h"
#include "core/memory.h"
#include "x86_64/rtc.h"
#include "drivers/keyboard.h"
#include "userland/shell/shell.h"
#include "x86_64/port.h"
#include "custom/text.h"
#include "drivers/framebuffer.h"
#include <stdbool.h>

struct multiboot2_tag {
    uint32_t type;
    uint32_t size;
};

struct multiboot2_framebuffer_tag {
    uint32_t type;
    uint32_t size;
    uint64_t address;
    uint32_t pitch;
    uint32_t width;
    uint32_t height;
    uint8_t bpp;
    uint8_t framebuffer_type;
    uint16_t reserved;
};

static bool parse_framebuffer_tag(uint64_t multiboot_info_addr)
{
    uint8_t *info = (uint8_t *)(uintptr_t)multiboot_info_addr;
    uint32_t total_size = *(uint32_t *)info;
    uint8_t *tag = info + 8;

    while ((uint32_t)(tag - info) + 8 <= total_size) {
        struct multiboot2_tag *entry =
            (struct multiboot2_tag *)(uintptr_t)tag;

        if (entry->type == 0) {
            break;
        }

        if (entry->type == 8) {
            struct multiboot2_framebuffer_tag *framebuffer =
                (struct multiboot2_framebuffer_tag *)(uintptr_t)tag;

            return framebuffer_init(framebuffer->address,
                                    framebuffer->pitch,
                                    framebuffer->width,
                                    framebuffer->height,
                                    (uint8_t)framebuffer->bpp);
        }

        if (entry->size < 8) {
            break;
        }
        tag += (entry->size + 7u) & ~7u;
    }

    return false;
}

void kmain(uint64_t multiboot_magic,
           uint64_t multiboot_info_addr)
{
    if (multiboot_magic == 0x36d76289u &&
        multiboot_info_addr != 0)
    {
        parse_framebuffer_tag(multiboot_info_addr);
        console_clear();
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