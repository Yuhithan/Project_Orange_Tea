#pragma once

#include <stdint.h>
#include <stddef.h>

enum {
    imp_COLOR_BLACK = 0,
	imp_COLOR_BLUE = 1,
	imp_COLOR_GREEN = 2,
	imp_COLOR_CYAN = 3,
	imp_COLOR_RED = 4,
	imp_COLOR_MAGENTA = 5,
	imp_COLOR_BROWN = 6,
	imp_COLOR_LIGHT_GRAY = 7,
	imp_COLOR_DARK_GRAY = 8,
	imp_COLOR_LIGHT_BLUE = 9,
	imp_COLOR_LIGHT_GREEN = 10,
	imp_COLOR_LIGHT_CYAN = 11,
	imp_COLOR_LIGHT_RED = 12,
	imp_COLOR_PINK = 13,
	imp_COLOR_YELLOW = 14,
	imp_COLOR_WHITE = 15,
};

void imp_clear();
void imp_char(char character);
void imp_str(char* string);
void imp_set_color(uint8_t foreground, uint8_t background);
void imp_uint64_dec(uint64_t value);
void imp_uint64_hex(uint64_t value);
void imp_uint64_bin(uint64_t value);