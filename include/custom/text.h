#pragma once

#include "stdbool.h"

#define VGA_COLUMNS 80
#define VGA_ROWS 25
#define VGA_MEMORY ((volatile uint16_t *)0xB8000)
#define VGA_ATTRIBUTE 0x0F00
#define VGA_CURSOR_COMMAND 0x3D4
#define VGA_CURSOR_DATA 0x3D5
#define RED "\033[1;31m"
#define GREEN "\033[1;32m"
#define YELLOW "\033[1;33m"
#define BLUE "\033[1;34m"
#define RESET "\033[0m"
#define NEWLINE "\n"
#define VGA_DEFAULT_FOREGROUND 0x0F



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
#define KEY_CODE_L_SHIFT 0x2A
#define KEY_CODE_R_SHIFT 0x36
#define KEY_CODE_1 0x02
#define KEY_CODE_2 0x03
#define KEY_CODE_3 0x04
#define KEY_CODE_4 0x05
#define KEY_CODE_5 0x06
#define KEY_CODE_6 0x07
#define KEY_CODE_7 0x08
#define KEY_CODE_8 0x09
#define KEY_CODE_9 0x0A
#define KEY_CODE_0 0x0B
#define KEY_CODE_MINUS 0x0C
#define KEY_CODE_EQUALS 0x0D
#define KEY_CODE_L_BRACKET 0x1A
#define KEY_CODE_R_BRACKET 0x1B
#define KEY_CODE_SEMICOLON 0x27
#define KEY_CODE_APOSTROPHE 0x28
#define KEY_CODE_GRAVE 0x29
#define KEY_CODE_BACKSLASH 0x2B
#define KEY_CODE_COMMA 0x33
#define KEY_CODE_PERIOD 0x34
#define KEY_CODE_SLASH 0x35

void console_update_cursor(void);

void console_scroll(void);

void console_clear(void);

uint16_t console_attribute(void);

void console_reset_color(void);

void console_apply_ansi(void);

void putchar_(char character);

void handle_input(struct KeyboardEvent event);

void test_handle_input(struct KeyboardEvent event);

void halt(void);
