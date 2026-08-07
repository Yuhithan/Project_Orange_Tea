#include "imp.h"
#include "shell.h"
#include "keyboard.h"
#include "framebuffer.h"
#include "network.h"

void kmain() {
    fb_init(0, 1024, 768, 1024);
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