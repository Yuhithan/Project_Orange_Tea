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
    for (int iy = 0; iy < height; iy++)
    {
        for (int ix = 0; ix < width; ix++)
        {
            fb_put_pixel(x + ix, y + iy, color);
        }
    }
}

void fb_clear(uint32_t color)
{
    fb_fill_rect(0, 0, screen_width, screen_height, color);
}