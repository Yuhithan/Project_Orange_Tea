#include "ui/font.h"
#include "framebuffer.h"
#include <stdint.h>

#define FONT_WIDTH   5
#define FONT_HEIGHT  7
#define FONT_SPACING 1

static uint32_t font_default_color = 0xFFFFFF;

/*
 * Bitmap convention:
 * Bit 4 = leftmost pixel.
 * Bit 0 = rightmost pixel.
 *
 * Rows are stored top to bottom.
 * 1 = foreground pixel.
 * 0 = background pixel.
 */

#define GLYPH(a,b,c,d,e,f,g) {a,b,c,d,e,f,g}

static const uint8_t font_ascii[96][7] = {
    /* 0x20 SPACE */
    GLYPH(0,0,0,0,0,0,0),

    /* ! */
    GLYPH(4,4,4,4,4,0,4),

    /* " */
    GLYPH(10,10,10,0,0,0,0),

    /* # */
    GLYPH(10,31,10,10,31,10,0),

    /* $ */
    GLYPH(4,15,20,14,5,30,4),

    /* % */
    GLYPH(24,25,2,4,8,19,3),

    /* & */
    GLYPH(12,18,20,8,21,18,13),

    /* ' */
    GLYPH(4,4,8,0,0,0,0),

    /* ( */
    GLYPH(2,4,8,8,8,4,2),

    /* ) */
    GLYPH(8,4,2,2,2,4,8),

    /* * */
    GLYPH(0,21,14,31,14,21,0),

    /* + */
    GLYPH(0,4,4,31,4,4,0),

    /* , */
    GLYPH(0,0,0,0,4,4,8),

    /* - */
    GLYPH(0,0,0,31,0,0,0),

    /* . */
    GLYPH(0,0,0,0,0,12,12),

    /* / */
    GLYPH(1,2,2,4,8,8,16),

    /* 0 */
    GLYPH(14,17,19,21,25,17,14),

    /* 1 */
    GLYPH(4,12,4,4,4,4,14),

    /* 2 */
    GLYPH(14,17,1,2,4,8,31),

    /* 3 */
    GLYPH(30,1,1,14,1,1,30),

    /* 4 */
    GLYPH(2,6,10,18,31,2,2),

    /* 5 */
    GLYPH(31,16,16,30,1,1,30),

    /* 6 */
    GLYPH(14,16,16,30,17,17,14),

    /* 7 */
    GLYPH(31,1,2,4,8,8,8),

    /* 8 */
    GLYPH(14,17,17,14,17,17,14),

    /* 9 */
    GLYPH(14,17,17,15,1,1,14),

    /* : */
    GLYPH(0,12,12,0,12,12,0),

    /* ; */
    GLYPH(0,12,12,0,4,4,8),

    /* < */
    GLYPH(2,4,8,16,8,4,2),

    /* = */
    GLYPH(0,0,31,0,31,0,0),

    /* > */
    GLYPH(8,4,2,1,2,4,8),

    /* ? */
    GLYPH(14,17,1,2,4,0,4),

    /* @ */
    GLYPH(14,17,23,21,23,16,14),

    /* A */
    GLYPH(14,17,17,31,17,17,17),

    /* B */
    GLYPH(30,17,17,30,17,17,30),

    /* C */
    GLYPH(15,16,16,16,16,16,15),

    /* D */
    GLYPH(30,17,17,17,17,17,30),

    /* E */
    GLYPH(31,16,16,30,16,16,31),

    /* F */
    GLYPH(31,16,16,30,16,16,16),

    /* G */
    GLYPH(15,16,16,23,17,17,15),

    /* H */
    GLYPH(17,17,17,31,17,17,17),

    /* I */
    GLYPH(31,4,4,4,4,4,31),

    /* J */
    GLYPH(1,1,1,1,17,17,14),

    /* K */
    GLYPH(17,18,20,24,20,18,17),

    /* L */
    GLYPH(16,16,16,16,16,16,31),

    /* M */
    GLYPH(17,27,21,21,17,17,17),

    /* N */
    GLYPH(17,25,21,19,17,17,17),

    /* O */
    GLYPH(14,17,17,17,17,17,14),

    /* P */
    GLYPH(30,17,17,30,16,16,16),

    /* Q */
    GLYPH(14,17,17,17,21,18,13),

    /* R */
    GLYPH(30,17,17,30,20,18,17),

    /* S */
    GLYPH(15,16,16,14,1,1,30),

    /* T */
    GLYPH(31,4,4,4,4,4,4),

    /* U */
    GLYPH(17,17,17,17,17,17,14),

    /* V */
    GLYPH(17,17,17,17,10,10,4),

    /* W */
    GLYPH(17,17,17,21,21,27,17),

    /* X */
    GLYPH(17,17,10,4,10,17,17),

    /* Y */
    GLYPH(17,17,10,4,4,4,4),

    /* Z */
    GLYPH(31,1,2,4,8,16,31),

    /* [ */
    GLYPH(14,8,8,8,8,8,14),

    /* backslash */
    GLYPH(16,8,8,4,2,2,1),

    /* ] */
    GLYPH(14,2,2,2,2,2,14),

    /* ^ */
    GLYPH(4,10,17,0,0,0,0),

    /* _ */
    GLYPH(0,0,0,0,0,0,31),

    /* ` */
    GLYPH(8,4,2,0,0,0,0),

    /* a */
    GLYPH(0,0,14,1,15,17,15),

    /* b */
    GLYPH(16,16,30,17,17,17,30),

    /* c */
    GLYPH(0,0,15,16,16,16,15),

    /* d */
    GLYPH(1,1,15,17,17,17,15),

    /* e */
    GLYPH(0,0,14,17,31,16,14),

    /* f */
    GLYPH(6,8,30,8,8,8,8),

    /* g */
    GLYPH(0,0,15,17,17,15,1),

    /* h */
    GLYPH(16,16,30,17,17,17,17),

    /* i */
    GLYPH(4,0,12,4,4,4,14),

    /* j */
    GLYPH(2,0,6,2,2,18,12),

    /* k */
    GLYPH(16,16,18,20,24,20,18),

    /* l */
    GLYPH(12,4,4,4,4,4,14),

    /* m */
    GLYPH(0,0,26,21,21,21,21),

    /* n */
    GLYPH(0,0,30,17,17,17,17),

    /* o */
    GLYPH(0,0,14,17,17,17,14),

    /* p */
    GLYPH(0,0,30,17,17,30,16),

    /* q */
    GLYPH(0,0,15,17,17,15,1),

    /* r */
    GLYPH(0,0,22,25,16,16,16),

    /* s */
    GLYPH(0,0,15,16,14,1,30),

    /* t */
    GLYPH(8,8,30,8,8,8,6),

    /* u */
    GLYPH(0,0,17,17,17,19,13),

    /* v */
    GLYPH(0,0,17,17,17,10,4),

    /* w */
    GLYPH(0,0,17,17,21,21,10),

    /* x */
    GLYPH(0,0,17,10,4,10,17),

    /* y */
    GLYPH(0,0,17,17,17,15,1),

    /* z */
    GLYPH(0,0,31,2,4,8,31),

    /* { */
    GLYPH(2,4,4,8,4,4,2),

    /* | */
    GLYPH(4,4,4,4,4,4,4),

    /* } */
    GLYPH(8,4,4,2,4,4,8),

    /* ~ */
    GLYPH(0,0,9,22,0,0,0),

    /* DEL */
    GLYPH(0,0,0,0,0,0,0)
};

#undef GLYPH

static const uint8_t *font_get_glyph(char c)
{
    uint8_t ch = (uint8_t)c;

    if (ch < 32 || ch > 127) {
        ch = '?';
    }

    return font_ascii[ch - 32];
}

int font_init(void)
{
    font_default_color = 0xFFFFFF;
    return 0;
}

void font_set_color(uint32_t color)
{
    font_default_color = color;
}

uint32_t font_get_color(void)
{
    return font_default_color;
}

int font_height(int scale)
{
    if (scale < 1)
        scale = 1;

    return FONT_HEIGHT * scale;
}

int font_string_width(const char *text, int scale)
{
    if (!text)
        return 0;

    if (scale < 1)
        scale = 1;

    int width = 0;
    int current = 0;
    int maximum = 0;

    while (*text) {
        if (*text == '\n') {
            if (current > maximum)
                maximum = current;

            current = 0;
        } else {
            current += (FONT_WIDTH + FONT_SPACING) * scale;
        }

        text++;
    }

    if (current > 0)
        current -= FONT_SPACING * scale;

    if (current > maximum)
        maximum = current;

    return maximum;
}

void font_draw_char(
    int x,
    int y,
    char c,
    int scale,
    uint32_t color
)
{
    if (scale < 1)
        scale = 1;

    if (c == ' ')
        return;

    const uint8_t *glyph = font_get_glyph(c);

    for (int row = 0; row < FONT_HEIGHT; row) {

    uint8_t bits = glyph[FONT_HEIGHT - 1 - row++];

    for (int col = 0; col < FONT_WIDTH; col++) {

        if (bits & (1u << (FONT_WIDTH - 1 - col))) {

            for (int sy = 0; sy < scale; sy++) {
                for (int sx = 0; sx < scale; sx++) {

                    fb_put_pixel(
                        x + col * scale + sx,
                        y - row * scale - sy,
                        color
                    );
                }
            }
        }
    }
}
}

void font_draw_string(
    int x,
    int y,
    const char *text,
    int scale,
    uint32_t color
)
{
    if (!text)
        return;

    if (scale < 1)
        scale = 1;

    int start_x = x;

    while (*text) {
        char c = *text++;

        if (c == '\n') {
            x = start_x;
            y += (FONT_HEIGHT + 1) * scale;
            continue;
        }

        font_draw_char(x, y, c, scale, color);

        x += (FONT_WIDTH + FONT_SPACING) * scale;
    }
}