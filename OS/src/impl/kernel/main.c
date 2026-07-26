#include "imp.h"
#include "shell.h"
#include "keyboard.h"
#include "gui.h"

void kmain() {
    enable_key_input();
    enable_gui();
    imp_cls();
    imp_color(PRINT_COLOR_BLACK, PRINT_COLOR_WHITE);
    imp_text("===========================================\n");
    imp_text("Welcome to Orange Tea OS!\n");
    imp_text("==========================================\n");

    shell_init();
    shell_run();
}