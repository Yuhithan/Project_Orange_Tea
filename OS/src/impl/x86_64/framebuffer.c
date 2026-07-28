#include "framebuffer.h"

uint32_t *framebuffer = 0;

int screen_width = 0;
int screen_height = 0;
int screen_pitch = 0;

void fb_init(uint32_t *fb, int width, int height, int pitch)
{
    framebuffer = fb;
    screen_width = width;
    screen_height = height;
    screen_pitch = pitch;
}

void fb_put_pixel(int x, int y, uint32_t color)
{
    if (x < 0 || y < 0)
        return;

    if (x >= screen_width || y >= screen_height)
        return;

    framebuffer[y * screen_pitch + x] = color;
}

void fb_fill_rect(int x, int y, int width, int height, uint32_t color)
{
    if (x < 0) {
        width += x;
        x = 0;
    }

    if (y < 0) {
        height += y;
        y = 0;
    }

    if (x + width > screen_width)
        width = screen_width - x;

    if (y + height > screen_height)
        height = screen_height - y;

    if (width <= 0 || height <= 0)
        return;

    for (int iy = 0; iy < height; iy++)
    {
        uint32_t *row = framebuffer + (y + iy) * screen_pitch + x;

        for (int ix = 0; ix < width; ix++)
            row[ix] = color;
    }
}

void fb_clear(uint32_t color)
{
    int total = screen_pitch * screen_height;

    for (int i = 0; i < total; i++)
        framebuffer[i] = color;
}