#include "imp.h"
#include "shell.h"
#include "keyboard.h"
#include "framebuffer.h"
#include "network.h"

void kmain(uint64_t multiboot_magic, uint64_t multiboot_info_addr) {
    (void)multiboot_magic;
    fb_init(0, 0, 0, 0);
    fb_init_from_multiboot(multiboot_info_addr);

    if (framebuffer_ready) {
        fb_clear(0xFF140B0B);
        fb_fill_rect(80, 80, 320, 160, 0xFF2C0F0D);
        fb_draw_rect(80, 80, 320, 160, 0xFFE8C34E);
        fb_draw_string(110, 105, "ORTOS FRAMEBUFFER", 0xFFF7E2C8);
        fb_draw_string(120, 135, "GUI MODE READY", 0xFFB8201C);
    }

    enable_key_input();
    enable_network();
    imp_cls();
    imp_color(PRINT_COLOR_BLACK, PRINT_COLOR_WHITE);
    imp_text("===========================================\n");
    imp_text("Welcome to Orange Tea OS!\n");
    imp_text("==========================================\n");

    shell_init();
    shell_run();
}