#include "ui/font.h"
#include "framebuffer.h"

/*
 * ORTos fallback bitmap font.
 *
 * Each character is 5 pixels wide and 7 pixels high.
 *
 * Bit 0 = leftmost pixel.
 *
 * This is intentionally kept inside the font renderer rather than
 * framebuffer.c. The framebuffer driver should only be responsible
 * for drawing pixels.
 */

#define FONT_WIDTH  5
#define FONT_HEIGHT 7
#define FONT_SPACING 1

static uint32_t font_default_color = 0xFFFFFF;

/*
 * 5x7 font.
 *
 * Characters:
 *   A-Z
 *   0-9
 *   space
 *   .
 *   ,
 *   !
 *   ?
 *   -
 *   _
 *   :
 *   /
 *   \
 *   (
 *   )
 *
 * Unknown characters use '?'.
 */

static const uint8_t font_letters[26][7] = {
    /* A */
    {
        0x0E,
        0x11,
        0x11,
        0x1F,
        0x11,
        0x11,
        0x11
    },

    /* B */
    {
        0x1E,
        0x11,
        0x11,
        0x1E,
        0x11,
        0x11,
        0x1E
    },

    /* C */
    {
        0x0F,
        0x10,
        0x10,
        0x10,
        0x10,
        0x10,
        0x0F
    },

    /* D */
    {
        0x1E,
        0x11,
        0x11,
        0x11,
        0x11,
        0x11,
        0x1E
    },

    /* E */
    {
        0x1F,
        0x10,
        0x10,
        0x1E,
        0x10,
        0x10,
        0x1F
    },

    /* F */
    {
        0x1F,
        0x10,
        0x10,
        0x1E,
        0x10,
        0x10,
        0x10
    },

    /* G */
    {
        0x0F,
        0x10,
        0x10,
        0x17,
        0x11,
        0x11,
        0x0F
    },

    /* H */
    {
        0x11,
        0x11,
        0x11,
        0x1F,
        0x11,
        0x11,
        0x11
    },

    /* I */
    {
        0x1F,
        0x04,
        0x04,
        0x04,
        0x04,
        0x04,
        0x1F
    },

    /* J */
    {
        0x01,
        0x01,
        0x01,
        0x01,
        0x11,
        0x11,
        0x0E
    },

    /* K */
    {
        0x11,
        0x12,
        0x14,
        0x18,
        0x14,
        0x12,
        0x11
    },

    /* L */
    {
        0x10,
        0x10,
        0x10,
        0x10,
        0x10,
        0x10,
        0x1F
    },

    /* M */
    {
        0x11,
        0x1B,
        0x15,
        0x15,
        0x11,
        0x11,
        0x11
    },

    /* N */
    {
        0x11,
        0x19,
        0x15,
        0x13,
        0x11,
        0x11,
        0x11
    },

    /* O */
    {
        0x0E,
        0x11,
        0x11,
        0x11,
        0x11,
        0x11,
        0x0E
    },

    /* P */
    {
        0x1E,
        0x11,
        0x11,
        0x1E,
        0x10,
        0x10,
        0x10
    },

    /* Q */
    {
        0x0E,
        0x11,
        0x11,
        0x11,
        0x15,
        0x12,
        0x0D
    },

    /* R */
    {
        0x1E,
        0x11,
        0x11,
        0x1E,
        0x14,
        0x12,
        0x11
    },

    /* S */
    {
        0x0F,
        0x10,
        0x10,
        0x0E,
        0x01,
        0x01,
        0x1E
    },

    /* T */
    {
        0x1F,
        0x04,
        0x04,
        0x04,
        0x04,
        0x04,
        0x04
    },

    /* U */
    {
        0x11,
        0x11,
        0x11,
        0x11,
        0x11,
        0x11,
        0x0E
    },

    /* V */
    {
        0x11,
        0x11,
        0x11,
        0x11,
        0x0A,
        0x0A,
        0x04
    },

    /* W */
    {
        0x11,
        0x11,
        0x11,
        0x15,
        0x15,
        0x1B,
        0x11
    },

    /* X */
    {
        0x11,
        0x11,
        0x0A,
        0x04,
        0x0A,
        0x11,
        0x11
    },

    /* Y */
    {
        0x11,
        0x11,
        0x0A,
        0x04,
        0x04,
        0x04,
        0x04
    },

    /* Z */
    {
        0x1F,
        0x01,
        0x02,
        0x04,
        0x08,
        0x10,
        0x1F
    }
};

static const uint8_t font_digits[10][7] = {
    /* 0 */
    {
        0x0E,
        0x11,
        0x13,
        0x15,
        0x19,
        0x11,
        0x0E
    },

    /* 1 */
    {
        0x04,
        0x0C,
        0x04,
        0x04,
        0x04,
        0x04,
        0x0E
    },

    /* 2 */
    {
        0x0E,
        0x11,
        0x01,
        0x02,
        0x04,
        0x08,
        0x1F
    },

    /* 3 */
    {
        0x1E,
        0x01,
        0x01,
        0x0E,
        0x01,
        0x01,
        0x1E
    },

    /* 4 */
    {
        0x02,
        0x06,
        0x0A,
        0x12,
        0x1F,
        0x02,
        0x02
    },

    /* 5 */
    {
        0x1F,
        0x10,
        0x10,
        0x1E,
        0x01,
        0x01,
        0x1E
    },

    /* 6 */
    {
        0x0E,
        0x10,
        0x10,
        0x1E,
        0x11,
        0x11,
        0x0E
    },

    /* 7 */
    {
        0x1F,
        0x01,
        0x02,
        0x04,
        0x08,
        0x08,
        0x08
    },

    /* 8 */
    {
        0x0E,
        0x11,
        0x11,
        0x0E,
        0x11,
        0x11,
        0x0E
    },

    /* 9 */
    {
        0x0E,
        0x11,
        0x11,
        0x0F,
        0x01,
        0x01,
        0x0E
    }
};


/* Return glyph bitmap for a character. */
static const uint8_t *font_get_glyph(char c)
{
    /*
     * Convert lowercase to uppercase.
     *
     * This keeps the fallback font small.
     */
    if (c >= 'a' && c <= 'z') {
        c = (char)(c - 'a' + 'A');
    }

    if (c >= 'A' && c <= 'Z') {
        return font_letters[c - 'A'];
    }

    if (c >= '0' && c <= '9') {
        return font_digits[c - '0'];
    }

    return NULL;
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
    if (scale < 1) {
        scale = 1;
    }

    return FONT_HEIGHT * scale;
}


int font_string_width(const char *text, int scale)
{
    if (text == NULL) {
        return 0;
    }

    if (scale < 1) {
        scale = 1;
    }

    int width = 0;

    while (*text != '\0') {

        if (*text == '\n') {
            text++;
            continue;
        }

        width += (FONT_WIDTH + FONT_SPACING) * scale;

        text++;
    }

    /*
     * Remove the final spacing pixel.
     */
    if (width > 0) {
        width -= FONT_SPACING * scale;
    }

    return width;
}


void font_draw_char(
    int x,
    int y,
    char c,
    int scale,
    uint32_t color
)
{
    if (scale < 1) {
        scale = 1;
    }

    /*
     * Space is intentionally blank.
     */
    if (c == ' ') {
        return;
    }

    const uint8_t *glyph = font_get_glyph(c);

    if (glyph == NULL) {

        /*
         * Unknown character:
         *
         * Draw a simple '?'.
         */
        glyph = NULL;

        static const uint8_t question_mark[7] = {
            0x0E,
            0x11,
            0x01,
            0x02,
            0x04,
            0x00,
            0x04
        };

        for (int row = 0; row < FONT_HEIGHT; row++) {

            uint8_t bits = question_mark[row];

            for (int col = 0; col < FONT_WIDTH; col++) {

                if (bits & (1u << col)) {

                    for (int sy = 0; sy < scale; sy++) {
                        for (int sx = 0; sx < scale; sx++) {

                            fb_put_pixel(
                                x + col * scale + sx,
                                y + row * scale + sy,
                                color
                            );

                        }
                    }
                }
            }
        }

        return;
    }

    /*
     * Draw the glyph.
     */
    for (int row = 0; row < FONT_HEIGHT; row++) {

        uint8_t bits = glyph[row];

        for (int col = 0; col < FONT_WIDTH; col++) {

            if (bits & (1u << col)) {

                for (int sy = 0; sy < scale; sy++) {

                    for (int sx = 0; sx < scale; sx++) {

                        fb_put_pixel(
                            x + col * scale + sx,
                            y + row * scale + sy,
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
    if (text == NULL) {
        return;
    }

    if (scale < 1) {
        scale = 1;
    }

    int start_x = x;

    while (*text != '\0') {

        char c = *text++;

        if (c == '\n') {

            x = start_x;
            y += (FONT_HEIGHT + 1) * scale;

            continue;
        }

        font_draw_char(
            x,
            y,
            c,
            scale,
            color
        );

        x += (FONT_WIDTH + FONT_SPACING) * scale;
    }
}