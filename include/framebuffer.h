#pragma once

#include <stdint.h>

/* Initialized from Multiboot2's framebuffer information tag. */
void fb_init(uint64_t multiboot_info_addr);
int fb_is_available(void);
int fb_width(void);
int fb_height(void);

void fb_clear(uint32_t color);
void fb_flush(void);
uint32_t fb_get_pixel(int x, int y);
void fb_put_pixel(int x, int y, uint32_t color);
void fb_fill_rect(int x, int y, int width, int height, uint32_t color);
void fb_draw_rect(int x, int y, int width, int height, uint32_t color);
void fb_draw_line(int x1, int y1, int x2, int y2, uint32_t color);
void fb_draw_char(int x, int y, char character, uint32_t color);
void fb_draw_string(int x, int y, const char *text, uint32_t color);

/* Compatibility names for early ORTos code. */
void init_screen(void);
void set_pixel(int x, int y, uint32_t color);
void set_rect(int x, int y, int width, int height, uint32_t color);
void set_line(int x1, int y1, int x2, int y2, uint32_t color);
