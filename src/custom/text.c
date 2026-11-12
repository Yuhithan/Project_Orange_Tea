#include <stdint.h>

#include "drivers/printf.h"
#include "core/memory.h"
#include "x86_64/rtc.h"
#include "drivers/keyboard.h"
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

#define CONSOLE_INPUT_BUFFER_SIZE 128

static volatile unsigned char console_input_buffer[CONSOLE_INPUT_BUFFER_SIZE];
static volatile uint16_t console_input_head;
static volatile uint16_t console_input_tail;

bool l_shift_pressed = false;
bool r_shift_pressed = false;

void console_input_push(char character)
{
    uint16_t head = console_input_head;

    if ((uint16_t)(head - console_input_tail) >=
        CONSOLE_INPUT_BUFFER_SIZE) {
        return;
    }

    console_input_buffer[head & (CONSOLE_INPUT_BUFFER_SIZE - 1)] =
        (unsigned char)character;
    console_input_head = (uint16_t)(head + 1);
}

int getchar(void)
{
    for (;;) {
        uint16_t tail = console_input_tail;

        if (tail != console_input_head) {
            int character =
                console_input_buffer[tail & (CONSOLE_INPUT_BUFFER_SIZE - 1)];
            console_input_tail = (uint16_t)(tail + 1);
            return character;
        }

        __asm__ volatile ("sti; hlt" ::: "memory");
    }
}


char en_us_to_ascii(uint16_t code, bool shift_pressed) {
    switch (code) {
        case EN_US_KEY_CODE_A: return shift_pressed ? 'A' : 'a';
        case EN_US_KEY_CODE_B: return shift_pressed ? 'B' : 'b';
        case EN_US_KEY_CODE_C: return shift_pressed ? 'C' : 'c';
        case EN_US_KEY_CODE_D: return shift_pressed ? 'D' : 'd';
        case EN_US_KEY_CODE_E: return shift_pressed ? 'E' : 'e';
        case EN_US_KEY_CODE_F: return shift_pressed ? 'F' : 'f';
        case EN_US_KEY_CODE_G: return shift_pressed ? 'G' : 'g';
        case EN_US_KEY_CODE_H: return shift_pressed ? 'H' : 'h';
        case EN_US_KEY_CODE_I: return shift_pressed ? 'I' : 'i';
        case EN_US_KEY_CODE_J: return shift_pressed ? 'J' : 'j';
        case EN_US_KEY_CODE_K: return shift_pressed ? 'K' : 'k';
        case EN_US_KEY_CODE_L: return shift_pressed ? 'L' : 'l';
        case EN_US_KEY_CODE_M: return shift_pressed ? 'M' : 'm';
        case EN_US_KEY_CODE_N: return shift_pressed ? 'N' : 'n';
        case EN_US_KEY_CODE_O: return shift_pressed ? 'O' : 'o';
        case EN_US_KEY_CODE_P: return shift_pressed ? 'P' : 'p';
        case EN_US_KEY_CODE_Q: return shift_pressed ? 'Q' : 'q';
        case EN_US_KEY_CODE_R: return shift_pressed ? 'R' : 'r';
        case EN_US_KEY_CODE_S: return shift_pressed ? 'S' : 's';
        case EN_US_KEY_CODE_T: return shift_pressed ? 'T' : 't';
        case EN_US_KEY_CODE_U: return shift_pressed ? 'U' : 'u';
        case EN_US_KEY_CODE_V: return shift_pressed ? 'V' : 'v';
        case EN_US_KEY_CODE_W: return shift_pressed ? 'W' : 'w';
        case EN_US_KEY_CODE_X: return shift_pressed ? 'X' : 'x';
        case EN_US_KEY_CODE_Y: return shift_pressed ? 'Y' : 'y';
        case EN_US_KEY_CODE_Z: return shift_pressed ? 'Z' : 'z';

        case KEY_CODE_1: return shift_pressed ? '!' : '1';
        case KEY_CODE_2: return shift_pressed ? '@' : '2';
        case KEY_CODE_3: return shift_pressed ? '#' : '3';
        case KEY_CODE_4: return shift_pressed ? '$' : '4';
        case KEY_CODE_5: return shift_pressed ? '%' : '5';
        case KEY_CODE_6: return shift_pressed ? '^' : '6';
        case KEY_CODE_7: return shift_pressed ? '&' : '7';
        case KEY_CODE_8: return shift_pressed ? '*' : '8';
        case KEY_CODE_9: return shift_pressed ? '(' : '9';
        case KEY_CODE_0: return shift_pressed ? ')' : '0';
        case EN_US_KEY_CODE_MINUS: return shift_pressed ? '_' : '-';
        case EN_US_KEY_CODE_EQUALS: return shift_pressed ? '+' : '=';
        case EN_US_KEY_CODE_L_BRACKET: return shift_pressed ? '{' : '[';
        case EN_US_KEY_CODE_R_BRACKET: return shift_pressed ? '}' : ']';
        case EN_US_KEY_CODE_SEMICOLON: return shift_pressed ? ':' : ';';
        case EN_US_KEY_CODE_APOSTROPHE: return shift_pressed ? '"' : '\'';
        case EN_US_KEY_CODE_GRAVE: return shift_pressed ? '~' : '`';
        case EN_US_KEY_CODE_BACKSLASH: return shift_pressed ? '|' : '\\';
        case EN_US_KEY_CODE_COMMA: return shift_pressed ? '<' : ',';
        case EN_US_KEY_CODE_PERIOD: return shift_pressed ? '>' : '.';
        case EN_US_KEY_CODE_SLASH: return shift_pressed ? '?' : '/';

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

char en_us_read_character(struct KeyboardEvent event)
{
    if (event.code == KEY_CODE_L_SHIFT) {
        if (event.type == KEYBOARD_EVENT_TYPE_MAKE) {
            l_shift_pressed = true;
        } else if (event.type == KEYBOARD_EVENT_TYPE_BREAK) {
            l_shift_pressed = false;
        }
        return '\0';
    }

    if (event.code == KEY_CODE_R_SHIFT) {
        if (event.type == KEYBOARD_EVENT_TYPE_MAKE) {
            r_shift_pressed = true;
        } else if (event.type == KEYBOARD_EVENT_TYPE_BREAK) {
            r_shift_pressed = false;
        }
        return '\0';
    }

    if (event.type == KEYBOARD_EVENT_TYPE_MAKE) {
        bool shift = l_shift_pressed || r_shift_pressed;
        return en_us_to_ascii(event.code, shift);
    }

    return '\0';
}

void en_us_handle_input(struct KeyboardEvent event)
{
    char character = en_us_read_character(event);
    if (character != '\0') {
        console_input_push(character);
        printf_("%c", character);
    }
}

void en_us_test_handle_input(struct KeyboardEvent event)
{
    /* LEFT SHIFT */
    if (event.code == KEY_CODE_L_SHIFT) {
        if (event.type == KEYBOARD_EVENT_TYPE_MAKE) {
            l_shift_pressed = true;
            printf_("LEFT SHIFT: DOWN\n");
        }
        else if (event.type == KEYBOARD_EVENT_TYPE_BREAK) {
            l_shift_pressed = false;
            printf_("LEFT SHIFT: UP\n");
        }
        return;
    }

    /* RIGHT SHIFT */
    if (event.code == KEY_CODE_R_SHIFT) {
        if (event.type == KEYBOARD_EVENT_TYPE_MAKE) {
            r_shift_pressed = true;
            printf_("RIGHT SHIFT: DOWN\n");
        }
        else if (event.type == KEYBOARD_EVENT_TYPE_BREAK) {
            r_shift_pressed = false;
            printf_("RIGHT SHIFT: UP\n");
        }
        return;
    }

    /* NORMAL KEYBOARD INPUT */
    if (event.type == KEYBOARD_EVENT_TYPE_MAKE) {
        bool shift_pressed =
            l_shift_pressed || r_shift_pressed;

        printf_("%c", en_us_to_ascii(event.code, shift_pressed));
    }
}

void halt(void)
{
    for (;;) {
        __asm__ volatile ("hlt");
    }
}