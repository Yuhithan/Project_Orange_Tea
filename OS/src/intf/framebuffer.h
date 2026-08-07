#pragma once

#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include <stdint.h>

extern uint32_t *framebuffer;
extern int screen_width;
extern int screen_height;
extern int screen_pitch;
extern int screen_bpp;

/* Initialize framebuffer */
void fb_init(uint32_t *fb, int width, int height, int pitch);

/* Draw one pixel */
void fb_put_pixel(int x, int y, uint32_t color);

/* Draw a filled rectangle */
void fb_fill_rect(int x, int y, int width, int height, uint32_t color);

/* Draw a rectangle outline */
void fb_draw_rect(int x, int y, int width, int height, uint32_t color);

/* Draw a line */
void fb_draw_line(int x0, int y0, int x1, int y1, uint32_t color);

/* Draw a simple character cell */
void fb_draw_char(int x, int y, char c, uint32_t color);

/* Draw a string */
void fb_draw_string(int x, int y, const char *text, uint32_t color);

/* Clear screen */
void fb_clear(uint32_t color);

#endif