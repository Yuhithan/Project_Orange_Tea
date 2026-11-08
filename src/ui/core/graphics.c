#include "ui/graphics.h"
#include "framebuffer.h"

static int graphics_ready = 0;

int gfx_init(uint64_t multiboot_info_addr)
{
    graphics_ready = 0;

    fb_init(multiboot_info_addr);

    if (!fb_is_available())
        return 0;

    graphics_ready = 1;
    return 1;
}

int gfx_is_available(void)
{
    return graphics_ready && fb_is_available();
}

int gfx_width(void)
{
    return gfx_is_available() ? fb_width() : 0;
}

int gfx_height(void)
{
    return gfx_is_available() ? fb_height() : 0;
}

void gfx_clear(uint32_t color)
{
    if (!gfx_is_available())
        return;

    fb_clear(color);
}

void gfx_put_pixel(int x, int y, uint32_t color)
{
    if (!gfx_is_available())
        return;

    fb_put_pixel(x, y, color);
}

void gfx_fill_rect(int x, int y, int width, int height, uint32_t color)
{
    if (!gfx_is_available())
        return;

    fb_fill_rect(x, y, width, height, color);
}

void gfx_draw_rect(int x, int y, int width, int height, uint32_t color)
{
    if (!gfx_is_available())
        return;

    fb_draw_rect(x, y, width, height, color);
}

void gfx_draw_line(int x1, int y1, int x2, int y2, uint32_t color)
{
    if (!gfx_is_available())
        return;

    fb_draw_line(x1, y1, x2, y2, color);
}

void gfx_present_rect(int x, int y, int width, int height,
                      const uint32_t *pixels, uint32_t source_pitch)
{
    if (!gfx_is_available() || pixels == 0 || width <= 0 || height <= 0)
        return;

    /*
     * fb_blit() performs clipping and flushes after the copy.
     * It expects source pixels as numeric 0x00RRGGBB values.
     */
    fb_blit(x, y, width, height, pixels, source_pitch);
}

void gfx_present(const uint32_t *pixels, int width, int height,
                 uint32_t source_pitch)
{
    gfx_present_rect(0, 0, width, height, pixels, source_pitch);
}

void gfx_flush(void)
{
    if (!gfx_is_available())
        return;

    fb_flush();
}
