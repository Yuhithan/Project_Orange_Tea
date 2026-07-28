#pragma once

#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include <stdint.h>

extern uint32_t *framebuffer;
extern int screen_width;
extern int screen_height;
extern int screen_pitch;

/* Initialize framebuffer */
void fb_init(uint32_t *fb, int width, int height, int pitch);

/* Draw one pixel */
void fb_put_pixel(int x, int y, uint32_t color);

/* Draw a filled rectangle */
void fb_fill_rect(int x, int y, int width, int height, uint32_t color);

/* Clear screen */
void fb_clear(uint32_t color);

#endif