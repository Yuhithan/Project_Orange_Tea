#include "ui/bmp.h"
#include "ui/framebuffer.h"
#include "memory.h"

void image_draw(image_t *image, int x, int y)
{
    if (!image || !image->pixels || !fb_is_available()) return;
    int sw = fb_width(), sh = fb_height();
    if (sw <= 0 || sh <= 0) return;
    for (uint32_t iy = 0; iy < image->height; iy++) {
        int64_t dy = (int64_t)y + iy;
        if (dy < 0 || dy >= sh) continue;
        for (uint32_t ix = 0; ix < image->width; ix++) {
            int64_t dx = (int64_t)x + ix;
            if (dx < 0 || dx >= sw) continue;
            fb_put_pixel((int)dx, (int)dy, image->pixels[(size_t)iy * image->width + ix]);
        }
    }
    fb_flush();
}

