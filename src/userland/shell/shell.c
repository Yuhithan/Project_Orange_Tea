#include "userland/shell/shell.h"
#include "custom/text.h"
#include "drivers/printf.h"

#define SHELL_PROMPT "TEST$"

static void shell_handle_keyboard(struct KeyboardEvent event)
{
    if (event.type == KEYBOARD_EVENT_TYPE_MAKE &&
        event.code == KEY_CODE_ENTER) {
        printf_(NEWLINE);
        printf_(BLUE SHELL_PROMPT RESET);
    } else {
        en_us_handle_input(event);
    }
}

void shell_loop() {
    keyboard_set_handler(shell_handle_keyboard);
    printf_(BLUE SHELL_PROMPT RESET);

    for (;;) {
        halt();
    }
}
