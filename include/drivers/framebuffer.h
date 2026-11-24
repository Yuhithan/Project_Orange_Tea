#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

bool framebuffer_init(uint64_t address, uint32_t pitch,
                      uint32_t width, uint32_t height,
                      uint8_t bits_per_pixel);
bool framebuffer_is_ready(void);

void framebuffer_clear(uint32_t color);
void framebuffer_set_pixel(uint32_t x, uint32_t y, uint32_t color);
void framebuffer_draw_char(uint32_t x, uint32_t y,
                           char character, uint32_t color);
void framebuffer_scroll(uint32_t rows);

uint32_t framebuffer_width(void);
uint32_t framebuffer_height(void);
