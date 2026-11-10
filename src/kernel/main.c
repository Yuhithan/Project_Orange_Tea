#include <stdint.h>

#include "drivers/printf.h"
#include "core/memory.h"
#include "x86_64/rtc.h"
#include "drivers/keyboard.h"
#include "x86_64/port.h"

#define VGA_COLUMNS 80
#define VGA_ROWS 25
#define VGA_MEMORY ((volatile uint16_t *)0xB8000)
#define VGA_ATTRIBUTE 0x0F00
#define VGA_CURSOR_COMMAND 0x3D4
#define VGA_CURSOR_DATA 0x3D5

static size_t console_row;
static size_t console_column;

static void console_update_cursor(void)
{
    uint16_t position = (uint16_t)(console_row * VGA_COLUMNS + console_column);
    port_outb(VGA_CURSOR_COMMAND, 0x0F);
    port_outb(VGA_CURSOR_DATA, (uint8_t)position);
    port_outb(VGA_CURSOR_COMMAND, 0x0E);
    port_outb(VGA_CURSOR_DATA, (uint8_t)(position >> 8));
}

static void console_scroll(void)
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

static void console_clear(void)
{
    for (size_t i = 0; i < VGA_COLUMNS * VGA_ROWS; i++) {
        VGA_MEMORY[i] = VGA_ATTRIBUTE | ' ';
    }
    console_row = 0;
    console_column = 0;
    console_update_cursor();
}

void putchar_(char character)
{
    if (character == '\n') {
        console_column = 0;
        console_row++;
    } else if (character == '\r') {
        console_column = 0;
    } else if (character == '\b') {
        if (console_column > 0) {
            console_column--;
            VGA_MEMORY[console_row * VGA_COLUMNS + console_column] =
                VGA_ATTRIBUTE | ' ';
        }
    } else {
        VGA_MEMORY[console_row * VGA_COLUMNS + console_column] =
            VGA_ATTRIBUTE | (uint8_t)character;
        console_column++;
        if (console_column == VGA_COLUMNS) {
            console_column = 0;
            console_row++;
        }
    }

    if (console_row == VGA_ROWS) {
        console_scroll();
    }
    console_update_cursor();
}

#define KEY_CODE_A 0x1E
#define KEY_CODE_B 0x30
#define KEY_CODE_C 0x2E
#define KEY_CODE_D 0x20
#define KEY_CODE_E 0x12
#define KEY_CODE_F 0x21
#define KEY_CODE_G 0x22
#define KEY_CODE_H 0x23
#define KEY_CODE_I 0x17
#define KEY_CODE_J 0x24
#define KEY_CODE_K 0x25
#define KEY_CODE_L 0x26
#define KEY_CODE_M 0x32
#define KEY_CODE_N 0x31
#define KEY_CODE_O 0x18
#define KEY_CODE_P 0x19
#define KEY_CODE_Q 0x10
#define KEY_CODE_R 0x13
#define KEY_CODE_S 0x1F
#define KEY_CODE_T 0x14
#define KEY_CODE_U 0x16
#define KEY_CODE_V 0x2F
#define KEY_CODE_W 0x11
#define KEY_CODE_X 0x2D
#define KEY_CODE_Y 0x15
#define KEY_CODE_Z 0x2C
#define KEY_CODE_SPACE 0x39
#define KEY_CODE_ENTER 0x1C
#define KEY_CODE_DELETE 0x0E

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

void handle_input(struct KeyboardEvent event) {
    if (event.type == KEYBOARD_EVENT_TYPE_MAKE) {
        printf_("%c", to_ascii(event.code));
    }
}

static void halt(void)
{
    for (;;) {
        __asm__ volatile ("hlt");
    }
}

void kmain(uint64_t multiboot_magic,
           uint64_t multiboot_info_addr)
{
    console_clear();
    if (multiboot_magic == 0x36d76289u &&
        multiboot_info_addr != 0)
    {
        printf_("=============================================\n");
        printf_("======================ORT====================\n");
        printf_("=============================================\n");
    }
    else
    {
        printf_("ERROR: invalid multiboot2 information.\n");
        halt();
    }
    memory_init();
    keyboard_set_handler(handle_input);
    keyboard_init();

    printf_("Orange Tea OS kernel\n");
    printf_("Multiboot2 information received.\n");
    int8_t prev_sec = 0;
    for (uint8_t i = 0; i < 5;) {
        uint8_t seconds = rtc_seconds();

        if (seconds != prev_sec) {
            i++;
            printf_("\nSeconds: %u", (unsigned)seconds);
        }

        prev_sec = seconds;
    }

    printf_("- seconds loop done.\n");
    halt();
}