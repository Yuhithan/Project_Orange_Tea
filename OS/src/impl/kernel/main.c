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
    imp_text("Type something...\n");
    //imp_text("\n \n");

    while (1) {
        int h = keyboard_getchar();
        if (h == '\n') {
            imp_text("\n");
        } else if (h == '\b') {
            // rudimentary backspace handling: print backspace and space and backspace
            imp_text("\b \b");
        } else {
            imp_char((char)h);
        }
    }
}