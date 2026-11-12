
#include <stddef.h>
#include <stdint.h>

#include "core/bool.h"
#include "drivers/printf.h"
#include "drivers/keyboard.h"
#include "x86_64/idt.h"
#include "x86_64/ps2.h"

#define KEYBOARD_EXTENDED_SCAN_CODE 0xE0

void (*keyboard_handler_user)(struct KeyboardEvent event) = NULL;

void keyboard_handler(void)
{
    static bool is_extended = false;

    uint8_t scan_code = ps2_read_scan_code();

    /* Debug raw scancode */
    // printf_("RAW SCANCODE: 0x%x\n", scan_code);

    if (scan_code == KEYBOARD_EXTENDED_SCAN_CODE) {
        is_extended = true;
        return;
    }

    uint16_t code = scan_code & 0x7F;

    if (is_extended) {
        code |= 0xE000;
        is_extended = false;
    }

    if (keyboard_handler_user == NULL) {
        return;
    }

    struct KeyboardEvent event;

    if (scan_code & 0x80) {
        event.type = KEYBOARD_EVENT_TYPE_BREAK;
    } else {
        event.type = KEYBOARD_EVENT_TYPE_MAKE;
    }

    event.code = code;

    keyboard_handler_user(event);
}

void keyboard_init(void)
{
    idt_init();
    idt_set_handler_keyboard(keyboard_handler);
}

void keyboard_set_handler(
    void (*handler)(struct KeyboardEvent event))
{
    keyboard_handler_user = handler;
}
