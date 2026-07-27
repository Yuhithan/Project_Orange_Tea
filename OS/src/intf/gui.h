#pragma once

#include <stdint.h>

void enable_gui(void);
void enable_cursor(void);
void gui_enter(void);
void draw_pixel(int x, int y, uint32_t color);
void draw_line(int x0, int y0, int x1, int y1, uint32_t color);
void draw_rect(int x, int y, int width, int height, uint32_t color);
void draw_rect_outline(int x, int y, int width, int height, uint32_t color);
void fill_rect(int x, int y, int width, int height, uint32_t color);
void draw_circle(int x, int y, int radius, uint32_t color);
void draw_string(int x, int y, const char* text, uint32_t color);
void gui_console_clear(void);
void gui_console_write_char(char character);
void gui_set_console_color(uint8_t foreground, uint8_t background);
int gui_is_active(void);

struct framebuffer
{
    uint32_t* address;
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
};

extern struct framebuffer framebuffer;