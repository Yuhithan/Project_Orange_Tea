#include "imp.h"
#include "shell.h"
#include "keyboard.h"

void kmain() {
    enable_key_input();
    imp_cls();
    imp_color(PRINT_COLOR_BLACK, PRINT_COLOR_WHITE);
    imp_text("==========================================\n");
    imp_text("Welcome to ORT, the new rival of Windows\n");
    imp_text("==========================================\n");

    shell_init();
    shell_run();
}