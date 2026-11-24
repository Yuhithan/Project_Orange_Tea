#include "drivers/framebuffer.h"

static volatile uint32_t *framebuffer_address;
static uint32_t framebuffer_pitch_pixels;
static uint32_t framebuffer_width_pixels;
static uint32_t framebuffer_height_pixels;
static uint8_t framebuffer_bpp;

bool framebuffer_init(uint64_t address, uint32_t pitch,
                      uint32_t width, uint32_t height,
                      uint8_t bits_per_pixel)
{
    if (address == 0 || width == 0 || height == 0 ||
        (bits_per_pixel != 24 && bits_per_pixel != 32)) {
        framebuffer_address = NULL;
        return false;
    }

    framebuffer_address = (volatile uint32_t *)(uintptr_t)address;
    framebuffer_width_pixels = width;
    framebuffer_height_pixels = height;
    framebuffer_bpp = bits_per_pixel;
    framebuffer_pitch_pixels = pitch / (bits_per_pixel / 8);
    return true;
}

bool framebuffer_is_ready(void)
{
    return framebuffer_address != NULL;
}

uint32_t framebuffer_width(void)
{
    return framebuffer_width_pixels;
}

uint32_t framebuffer_height(void)
{
    return framebuffer_height_pixels;
}

void framebuffer_set_pixel(uint32_t x, uint32_t y, uint32_t color)
{
    if (framebuffer_address == NULL ||
        x >= framebuffer_width_pixels ||
        y >= framebuffer_height_pixels) {
        return;
    }

    framebuffer_address[y * framebuffer_pitch_pixels + x] = color;
}

void framebuffer_clear(uint32_t color)
{
    if (framebuffer_address == NULL) {
        return;
    }

    for (uint32_t y = 0; y < framebuffer_height_pixels; y++) {
        for (uint32_t x = 0; x < framebuffer_width_pixels; x++) {
            framebuffer_address[y * framebuffer_pitch_pixels + x] = color;
        }
    }
}

static uint64_t font_rows(char character)
{
    if (character >= 'A' && character <= 'Z') {
        character = (char)(character - 'A' + 'a');
    }

    switch (character) {
        case 'a': return 0x1C7CFC1CULL;
        case 'b': return 0x7C7C3030ULL;
        case 'c': return 0x1C301CULL;
        case 'd': return 0x1C1C1C7CULL;
        case 'e': return 0x7C7C7C78ULL;
        case 'f': return 0x3C303030ULL;
        case 'g': return 0x7C1C7C7CULL;
        case 'h': return 0x30307C7CULL;
        case 'i': return 0x1C1C1C1CULL;
        case 'j': return 0x0C0C1C3CULL;
        case 'k': return 0x303C7C30ULL;
        case 'l': return 0x3030307CULL;
        case 'm': return 0x7C7C7C7CULL;
        case 'n': return 0x3C7C3030ULL;
        case 'o': return 0x3C66663CULL;
        case 'p': return 0x7C7C3C18ULL;
        case 'q': return 0x3C7C3C1CULL;
        case 'r': return 0x301C3000ULL;
        case 's': return 0x3C1C7C3CULL;
        case 't': return 0x3030307CULL;
        case 'u': return 0x6666663CULL;
        case 'v': return 0x66663C18ULL;
        case 'w': return 0x7C7C7C7CULL;
        case 'x': return 0x426C7C66ULL;
        case 'y': return 0x66663C18ULL;
        case 'z': return 0x7C18406FULL;
        case '0': return 0x3C666E663CULL;
        case '1': return 0x1838181818ULL;
        case '2': return 0x3C061C607CULL;
        case '3': return 0x3C060C067CULL;
        case '4': return 0x66062C1666ULL;
        case '5': return 0x7C607C0C78ULL;
        case '6': return 0x3C607C6663CULL;
        case '7': return 0x7C060C1830ULL;
        case '8': return 0x3C663C6663CULL;
        case '9': return 0x3C666C0C78ULL;
        case ':': return 0x1818001818ULL;
        case '.': return 0x1818000000ULL;
        case ',': return 0x1818000018ULL;
        case '-': return 0x00007E0000ULL;
        case '_': return 0x000000007EULL;
        case '/': return 0x060C183060ULL;
        case '\\': return 0x6030180C06ULL;
        case '#': return 0x3C7E3C7E3CULL;
        case '@': return 0x7C664E767CULL;
        default:  return 0ULL;
    }
}

void framebuffer_draw_char(uint32_t x, uint32_t y,
                          char character, uint32_t color,
                          uint32_t scale)
{
    if (scale == 0)
        return;

    uint64_t glyph = font_rows(character);

    for (uint32_t row = 0; row < 8; row++)
    {
        uint8_t bits = (uint8_t)((glyph >> (row * 8)) & 0xFFu);

        for (uint32_t column = 0; column < 8; column++)
        {
            if (bits & (1u << column))
            {
                for (uint32_t sy = 0; sy < scale; sy++)
                {
                    for (uint32_t sx = 0; sx < scale; sx++)
                    {
                        framebuffer_set_pixel(
                            x + column * scale + sx,
                            y + row * scale + sy,
                            color
                        );
                    }
                }
            }
        }
    }
}

void framebuffer_scroll(uint32_t rows)
{
    if (framebuffer_address == NULL || rows == 0) {
        return;
    }

    if (rows >= framebuffer_height_pixels) {
        framebuffer_clear(0x000000);
        return;
    }

    for (uint32_t y = rows; y < framebuffer_height_pixels; y++) {
        for (uint32_t x = 0; x < framebuffer_width_pixels; x++) {
            framebuffer_address[(y - rows) * framebuffer_pitch_pixels + x] =
                framebuffer_address[y * framebuffer_pitch_pixels + x];
        }
    }

    for (uint32_t y = framebuffer_height_pixels - rows;
         y < framebuffer_height_pixels; y++) {
        for (uint32_t x = 0; x < framebuffer_width_pixels; x++) {
            framebuffer_address[y * framebuffer_pitch_pixels + x] = 0x000000;
        }
    }
}
