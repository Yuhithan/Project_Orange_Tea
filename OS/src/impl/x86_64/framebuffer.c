#include "framebuffer.h"

static uint32_t framebuffer_storage[1024 * 768];

uint32_t *framebuffer = 0;

int screen_width = 0;
int screen_height = 0;
int screen_pitch = 0;
int screen_bpp = 4;
int screen_bytes_per_pixel = 4;
int framebuffer_ready = 0;

static void fb_init_defaults(void)
{
    framebuffer = framebuffer_storage;
    screen_width = 1024;
    screen_height = 768;
    screen_pitch = 1024;
    screen_bpp = 32;
    screen_bytes_per_pixel = 4;
    framebuffer_ready = 1;
}

void fb_init(uint32_t *fb, int width, int height, int pitch)
{
    if (fb != 0) {
        framebuffer = fb;
    } else {
        framebuffer = framebuffer_storage;
    }

    if (width <= 0) width = 1024;
    if (height <= 0) height = 768;
    if (pitch <= 0) pitch = width;

    screen_width = width;
    screen_height = height;
    screen_pitch = pitch;
    screen_bpp = 32;
    screen_bytes_per_pixel = 4;
    framebuffer_ready = 1;
}

void fb_init_from_multiboot(uint64_t info_addr)
{
    (void)info_addr;
    fb_init_defaults();
}

void fb_put_pixel(int x, int y, uint32_t color)
{
    if (framebuffer == 0 || x < 0 || y < 0)
        return;

    if (x >= screen_width || y >= screen_height)
        return;

    framebuffer[y * screen_pitch + x] = color;
}

void fb_fill_rect(int x, int y, int width, int height, uint32_t color)
{
    if (framebuffer == 0)
        return;

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

void fb_draw_rect(int x, int y, int width, int height, uint32_t color)
{
    if (width <= 0 || height <= 0)
        return;

    fb_draw_line(x, y, x + width - 1, y, color);
    fb_draw_line(x, y + height - 1, x + width - 1, y + height - 1, color);
    fb_draw_line(x, y, x, y + height - 1, color);
    fb_draw_line(x + width - 1, y, x + width - 1, y + height - 1, color);
}

void fb_draw_line(int x0, int y0, int x1, int y1, uint32_t color)
{
    int dx = x1 - x0;
    int dy = y1 - y0;
    int sx = dx < 0 ? -1 : 1;
    int sy = dy < 0 ? -1 : 1;
    dx = dx < 0 ? -dx : dx;
    dy = dy < 0 ? -dy : dy;
    int err = dx - dy;

    while (1) {
        fb_put_pixel(x0, y0, color);
        if (x0 == x1 && y0 == y1)
            break;

        int e2 = err * 2;
        if (e2 > -dy) {
            err -= dy;
            x0 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y0 += sy;
        }
    }
}

void fb_draw_char(int x, int y, char c, uint32_t color)
{
    if (c == '\0')
        return;

    if (c == ' ')
        return;

    for (int i = 0; i < 5; i++) {
        for (int j = 0; j < 7; j++) {
            fb_put_pixel(x + i, y + j, color);
        }
    }
}

void fb_draw_string(int x, int y, const char *text, uint32_t color)
{
    int cursor_x = x;
    int cursor_y = y;

    if (text == 0)
        return;

    while (*text != '\0') {
        if (*text == '\n') {
            cursor_y += 8;
            cursor_x = x;
        } else {
            fb_draw_char(cursor_x, cursor_y, *text, color);
            cursor_x += 6;
        }
        text++;
    }
}

void fb_clear(uint32_t color)
{
    if (framebuffer == 0 || screen_width <= 0 || screen_height <= 0)
        return;

    int total = screen_pitch * screen_height;

    for (int i = 0; i < total; i++)
        framebuffer[i] = color;
}