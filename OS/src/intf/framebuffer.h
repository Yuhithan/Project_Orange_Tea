#pragma once

void init_screen(void);
//void framebuffer_clear(void);
void set_pixel(int x, int y, uint32_t color);
void set_rect(int x, int y, int width, int height, uint32_t color);
void set_line(int x1, int y1, int x2, int y2, uint32_t color);