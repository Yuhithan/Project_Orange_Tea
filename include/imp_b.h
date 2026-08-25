#pragma once

#include <stdbool.h>
#include <stdint.h>

bool imp_backend_active(void);
void imp_backend_clear(void);
void imp_backend_putchar(char c);
void imp_backend_set_color(uint8_t fg, uint8_t bg);