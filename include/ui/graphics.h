#ifndef ORTOS_GRAPHICS_H
#define ORTOS_GRAPHICS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * ORTos graphics API.
 *
 * Pixel colors passed to this API use 0x00RRGGBB.
 * gfx_present() expects a 32-bit source buffer with that same
 * numeric pixel format. source_pitch is measured in BYTES.
 *
 * This is a small framebuffer abstraction for an SDL2 port to call.
 * It is not, by itself, a complete SDL2 video driver.
 */

int gfx_init(uint64_t multiboot_info_addr);
int gfx_is_available(void);

int gfx_width(void);
int gfx_height(void);

/* Draw into the kernel framebuffer backbuffer. */
void gfx_clear(uint32_t color);
void gfx_put_pixel(int x, int y, uint32_t color);
void gfx_fill_rect(int x, int y, int width, int height, uint32_t color);
void gfx_draw_rect(int x, int y, int width, int height, uint32_t color);
void gfx_draw_line(int x1, int y1, int x2, int y2, uint32_t color);

/*
 * Copy a 32-bit 0x00RRGGBB pixel buffer to the screen.
 * source_pitch is the number of bytes between source rows.
 * The destination rectangle is clipped to the screen.
 */
void gfx_present(const uint32_t *pixels, int width, int height,
                 uint32_t source_pitch);
void gfx_present_rect(int x, int y, int width, int height,
                      const uint32_t *pixels, uint32_t source_pitch);

/* Copy pending backbuffer changes to the hardware framebuffer. */
void gfx_flush(void);

#ifdef __cplusplus
}
#endif

#endif /* ORTOS_GRAPHICS_H */
