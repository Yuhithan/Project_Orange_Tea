#include <stdint.h>

#include "drivers/printf.h"
#include "core/memory.h"
#include "x86_64/rtc.h"
#include "drivers/keyboard.h"
#include "apps/shell/shell.h"
#include "x86_64/port.h"
#include "custom/text.h"


uint8_t console_foreground = VGA_DEFAULT_FOREGROUND;
uint8_t console_bold = 0;

/* ANSI escape parser states */
uint8_t ansi_state = 0;
uint8_t ansi_params[4];
uint8_t ansi_param_count = 0;
uint8_t ansi_param_value = 0;
uint8_t ansi_has_value = 0;

size_t console_row;
size_t console_column;


char to_ascii(uint16_t code) {
    switch (code) {
        case KEY_CODE_A: return 'a';
        case KEY_CODE_B: return 'b';
        case KEY_CODE_C: return 'c';
        case KEY_CODE_D: return 'd';
        case KEY_CODE_E: return 'e';
        case KEY_CODE_F: return 'f';
        case KEY_CODE_G: return 'g';
        case KEY_CODE_H: return 'h';
        case KEY_CODE_I: return 'i';
        case KEY_CODE_J: return 'j';
        case KEY_CODE_K: return 'k';
        case KEY_CODE_L: return 'l';
        case KEY_CODE_M: return 'm';
        case KEY_CODE_N: return 'n';
        case KEY_CODE_O: return 'o';
        case KEY_CODE_P: return 'p';
        case KEY_CODE_Q: return 'q';
        case KEY_CODE_R: return 'r';
        case KEY_CODE_S: return 's';
        case KEY_CODE_T: return 't';
        case KEY_CODE_U: return 'u';
        case KEY_CODE_V: return 'v';
        case KEY_CODE_W: return 'w';
        case KEY_CODE_X: return 'x';
        case KEY_CODE_Y: return 'y';
        case KEY_CODE_Z: return 'z';
        case KEY_CODE_SPACE: return ' ';
        case KEY_CODE_ENTER: return '\n';
        case KEY_CODE_DELETE: return '\b';
    }    
    
    return '?';
}

void console_update_cursor(void)
{
    uint16_t position = (uint16_t)(console_row * VGA_COLUMNS + console_column);
    port_outb(VGA_CURSOR_COMMAND, 0x0F);
    port_outb(VGA_CURSOR_DATA, (uint8_t)position);
    port_outb(VGA_CURSOR_COMMAND, 0x0E);
    port_outb(VGA_CURSOR_DATA, (uint8_t)(position >> 8));
}

void console_scroll(void)
{
    for (size_t row = 1; row < VGA_ROWS; row++) {
        for (size_t column = 0; column < VGA_COLUMNS; column++) {
            VGA_MEMORY[(row - 1) * VGA_COLUMNS + column] =
                VGA_MEMORY[row * VGA_COLUMNS + column];
        }
    }

    for (size_t column = 0; column < VGA_COLUMNS; column++) {
        VGA_MEMORY[(VGA_ROWS - 1) * VGA_COLUMNS + column] =
            VGA_ATTRIBUTE | ' ';
    }
    console_row = VGA_ROWS - 1;
}

void console_clear(void)
{
    for (size_t i = 0; i < VGA_COLUMNS * VGA_ROWS; i++) {
        VGA_MEMORY[i] = VGA_ATTRIBUTE | ' ';
    }
    console_row = 0;
    console_column = 0;
    console_update_cursor();
}

uint16_t console_attribute(void)
{
    uint8_t foreground = console_foreground;

    if (console_bold && foreground < 8) {
        foreground += 8;
    }

    return (uint16_t)foreground << 8;
}

void console_reset_color(void)
{
    console_foreground = VGA_DEFAULT_FOREGROUND;
    console_bold = 0;
}

void console_apply_ansi(void)
{
    for (uint8_t i = 0; i < ansi_param_count; i++) {
        uint8_t code = ansi_params[i];

        if (code == 0) {
            console_reset_color();
        }
        else if (code == 1) {
            console_bold = 1;
        }
        else if (code >= 30 && code <= 37) {
            /*
             * ANSI: black, red, green, yellow,
             *       blue, magenta, cyan, white
             *
             * VGA:  black, blue, green, cyan,
             *       red, magenta, brown, light grey
             */
            const uint8_t colors[8] = {
                0, 4, 2, 6, 1, 5, 3, 7
            };

            console_foreground = colors[code - 30];
        }
        else if (code >= 90 && code <= 97) {
            const uint8_t bright_colors[8] = {
                8, 12, 10, 14, 9, 13, 11, 15
            };

            console_foreground = bright_colors[code - 90];
        }
    }

    /*
     * If bold is enabled, brighten normal ANSI colours.
     * Bright ANSI colours are already bright.
     */
    if (console_bold && console_foreground < 8) {
        console_foreground += 8;
    }
}

void putchar_(char character)
{
    /*
     * ANSI escape sequence handling.
     *
     * State 0: normal text
     * State 1: ESC received
     * State 2: CSI parameters
     */

    if (ansi_state == 1) {
        if (character == '[') {
            ansi_state = 2;
            ansi_param_count = 0;
            ansi_param_value = 0;
            ansi_has_value = 0;
        } else {
            ansi_state = 0;
        }

        return;
    }

    if (ansi_state == 2) {
        if (character >= '0' && character <= '9') {
            ansi_param_value =
                ansi_param_value * 10 + (character - '0');

            ansi_has_value = 1;
            return;
        }

        if (character == ';') {
            if (ansi_param_count < 4) {
                ansi_params[ansi_param_count++] =
                    ansi_has_value ? ansi_param_value : 0;
            }

            ansi_param_value = 0;
            ansi_has_value = 0;
            return;
        }

        if (character == 'm') {
            if (ansi_param_count < 4) {
                ansi_params[ansi_param_count++] =
                    ansi_has_value ? ansi_param_value : 0;
            }

            console_apply_ansi();
        }

        ansi_state = 0;
        return;
    }

    if ((uint8_t)character == 0x1B) {
        ansi_state = 1;
        return;
    }

    /* Normal console output */

    if (character == '\n') {
        console_column = 0;
        console_row++;
    }
    else if (character == '\r') {
        console_column = 0;
    }
    else if (character == '\b') {
        if (console_column > 0) {
            console_column--;

            VGA_MEMORY[console_row * VGA_COLUMNS + console_column] =
                console_attribute() | ' ';
        }
    }
    else {
        VGA_MEMORY[console_row * VGA_COLUMNS + console_column] =
            console_attribute() | (uint8_t)character;

        console_column++;

        if (console_column == VGA_COLUMNS) {
            console_column = 0;
            console_row++;
        }
    }

    if (console_row >= VGA_ROWS) {
        console_scroll();
    }

    console_update_cursor();
}

void handle_input(struct KeyboardEvent event) {
    if (event.type == KEYBOARD_EVENT_TYPE_MAKE) {
        printf_("%c", to_ascii(event.code));
    }
}

void halt(void)
{
    for (;;) {
        __asm__ volatile ("hlt");
    }
}