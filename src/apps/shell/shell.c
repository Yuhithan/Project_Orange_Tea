#include "apps/shell/shell.h"
#include "drivers/printf.h"
#include "x86_64/rtc.h"
#include "drivers/keyboard.h"
#include "custom/text.h"

/*
1. shell_loop: []
*/


#define SHELL_PROMPT "TestShell> "

static void shell_handle_keyboard(struct KeyboardEvent event)
{
    if (event.type != KEYBOARD_EVENT_TYPE_MAKE) {
        return;
    }

    if (event.code == KEY_CODE_ENTER) {
        printf_(NEWLINE);
        printf_(BLUE SHELL_PROMPT RESET);
    } else {
        handle_input(event);
    }
}


void shell_loop() {
    keyboard_set_handler(shell_handle_keyboard);
    printf_(BLUE SHELL_PROMPT RESET);
    
    for (;;) {
        halt();
    }
}