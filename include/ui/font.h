#ifndef ORTOS_FONT_H
#define ORTOS_FONT_H

#include <stdint.h>
#include <stddef.h>

/*
 * ORTos font system
 *
 * Coordinates are framebuffer coordinates.
 *
 * The current implementation provides a small fallback bitmap font.
 * The API is intentionally designed so a TTF rasterizer can replace
 * the backend later without changing ORgui.
 */

/* Initialize the font system. */
int font_init(void);

/*
 * Draw one character.
 *
 * x, y   - top-left position
 * c      - character to draw
 * scale  - bitmap scale (1 = normal, 2 = 2x, etc.)
 * color  - RGB color: 0xRRGGBB
 */
void font_draw_char(
    int x,
    int y,
    char c,
    int scale,
    uint32_t color
);

/*
 * Draw a null-terminated string.
 *
 * scale  - bitmap scale
 * color  - RGB color: 0xRRGGBB
 */
void font_draw_string(
    int x,
    int y,
    const char *text,
    int scale,
    uint32_t color
);

/*
 * Return the width of a string in pixels.
 */
int font_string_width(
    const char *text,
    int scale
);

/*
 * Return the height of the current font in pixels.
 */
int font_height(int scale);

/*
 * Set the default font color.
 */
void font_set_color(uint32_t color);

/*
 * Get the current default font color.
 */
uint32_t font_get_color(void);

#endif /* ORTOS_FONT_H */