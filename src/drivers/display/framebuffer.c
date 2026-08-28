#include "framebuffer.h"

/* Multiboot2 information tags are 8-byte aligned. */
struct multiboot_tag {
    uint32_t type;
    uint32_t size;
};

struct multiboot_framebuffer_tag {
    uint32_t type;
    uint32_t size;
    uint64_t address;
    uint32_t pitch;
    uint32_t width;
    uint32_t height;
    uint8_t bpp;
    uint8_t framebuffer_type;
    uint16_t reserved;
};

static volatile uint8_t *framebuffer;
static uint32_t framebuffer_pitch;
static uint32_t framebuffer_width;
static uint32_t framebuffer_height;
static uint8_t framebuffer_bytes_per_pixel;
static uint8_t red_position, red_mask_size;
static uint8_t green_position, green_mask_size;
static uint8_t blue_position, blue_mask_size;

static int fb_valid(void)
{
    return framebuffer != 0 && framebuffer_width != 0 && framebuffer_height != 0 &&
           (framebuffer_bytes_per_pixel == 3 || framebuffer_bytes_per_pixel == 4);
}

void fb_init(uint64_t multiboot_info_addr)
{
    framebuffer = 0;
    framebuffer_pitch = 0;
    framebuffer_width = 0;
    framebuffer_height = 0;
    framebuffer_bytes_per_pixel = 0;

    if (multiboot_info_addr == 0) return;

    const uint8_t *info = (const uint8_t *)(uintptr_t)multiboot_info_addr;
    uint32_t total_size = *(const uint32_t *)info;
    if (total_size < 16) return;

    uint32_t offset = 8;
    while (offset + sizeof(struct multiboot_tag) <= total_size) {
        const struct multiboot_tag *tag = (const struct multiboot_tag *)(info + offset);
        if (tag->type == 0 || tag->size < sizeof(*tag)) break;
        if (tag->type == 8 && tag->size >= sizeof(struct multiboot_framebuffer_tag)) {
            const struct multiboot_framebuffer_tag *fb = (const struct multiboot_framebuffer_tag *)tag;
            /* ORTos's early identity map covers the low 4 GiB only. */
            const uint8_t *colors = (const uint8_t *)fb + sizeof(*fb);
            uint8_t bytes_per_pixel = (uint8_t)(fb->bpp / 8u);
            if ((fb->address >> 32) == 0 && fb->framebuffer_type == 1 &&
                (fb->bpp == 24 || fb->bpp == 32) && tag->size >= sizeof(*fb) + 6 &&
                colors[1] > 0 && colors[1] <= 8 && colors[3] > 0 && colors[3] <= 8 &&
                colors[5] > 0 && colors[5] <= 8 && colors[0] < fb->bpp &&
                colors[2] < fb->bpp && colors[4] < fb->bpp &&
                fb->width != 0 && fb->height != 0 && fb->pitch >= fb->width * bytes_per_pixel) {
                framebuffer = (volatile uint8_t *)(uintptr_t)fb->address;
                framebuffer_pitch = fb->pitch;
                framebuffer_width = fb->width;
                framebuffer_height = fb->height;
                framebuffer_bytes_per_pixel = bytes_per_pixel;
                red_position = colors[0]; red_mask_size = colors[1];
                green_position = colors[2]; green_mask_size = colors[3];
                blue_position = colors[4]; blue_mask_size = colors[5];
            }
            return;
        }
        offset = (offset + tag->size + 7u) & ~7u;
    }
}

int fb_is_available(void) { return fb_valid(); }
int fb_width(void) { return (int)framebuffer_width; }
int fb_height(void) { return (int)framebuffer_height; }

uint32_t fb_get_pixel(int x, int y)
{
    if (!fb_valid() || x < 0 || y < 0 || (uint32_t)x >= framebuffer_width ||
        (uint32_t)y >= framebuffer_height) return 0;

    const volatile uint8_t *pixel = framebuffer +
        (uint32_t)y * framebuffer_pitch + (uint32_t)x * framebuffer_bytes_per_pixel;
    uint32_t native = 0;
    for (uint8_t byte = 0; byte < framebuffer_bytes_per_pixel; byte++)
        native |= (uint32_t)pixel[byte] << (byte * 8u);

    uint32_t red = (native >> red_position) & ((1u << red_mask_size) - 1u);
    uint32_t green = (native >> green_position) & ((1u << green_mask_size) - 1u);
    uint32_t blue = (native >> blue_position) & ((1u << blue_mask_size) - 1u);
    red = (red * 255u) / ((1u << red_mask_size) - 1u);
    green = (green * 255u) / ((1u << green_mask_size) - 1u);
    blue = (blue * 255u) / ((1u << blue_mask_size) - 1u);
    return (red << 16) | (green << 8) | blue;
}

void fb_put_pixel(int x, int y, uint32_t color)
{
    if (!fb_valid() || x < 0 || y < 0 || (uint32_t)x >= framebuffer_width || (uint32_t)y >= framebuffer_height) return;
    uint32_t native = (((color >> 16) & 0xFFu) >> (8u - red_mask_size)) << red_position;
    native |= (((color >> 8) & 0xFFu) >> (8u - green_mask_size)) << green_position;
    native |= ((color & 0xFFu) >> (8u - blue_mask_size)) << blue_position;
    volatile uint8_t *pixel = framebuffer + (uint32_t)y * framebuffer_pitch + (uint32_t)x * framebuffer_bytes_per_pixel;
    for (uint8_t byte = 0; byte < framebuffer_bytes_per_pixel; byte++) pixel[byte] = (uint8_t)(native >> (byte * 8u));
}

void fb_clear(uint32_t color)
{
    if (!fb_valid()) return;
    for (uint32_t y = 0; y < framebuffer_height; y++)
        for (uint32_t x = 0; x < framebuffer_width; x++)
            fb_put_pixel((int)x, (int)y, color);
}

void fb_fill_rect(int x, int y, int width, int height, uint32_t color)
{
    if (!fb_valid() || width <= 0 || height <= 0) return;
    int x_end = x + width;
    int y_end = y + height;
    if (x_end < x || y_end < y) return;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x_end > (int)framebuffer_width) x_end = (int)framebuffer_width;
    if (y_end > (int)framebuffer_height) y_end = (int)framebuffer_height;
    for (int py = y; py < y_end; py++)
        for (int px = x; px < x_end; px++)
            fb_put_pixel(px, py, color);
}

void fb_draw_rect(int x, int y, int width, int height, uint32_t color)
{
    if (width <= 0 || height <= 0) return;
    fb_fill_rect(x, y, width, 1, color);
    fb_fill_rect(x, y + height - 1, width, 1, color);
    fb_fill_rect(x, y, 1, height, color);
    fb_fill_rect(x + width - 1, y, 1, height, color);
}

void fb_draw_line(int x1, int y1, int x2, int y2, uint32_t color)
{
    int dx = x2 >= x1 ? x2 - x1 : x1 - x2;
    int sx = x1 < x2 ? 1 : -1;
    int dy = y1 >= y2 ? y1 - y2 : y2 - y1;
    int sy = y1 < y2 ? 1 : -1;
    int err = dx + dy;
    for (;;) {
        fb_put_pixel(x1, y1, color);
        if (x1 == x2 && y1 == y2) break;
        int twice_error = err * 2;
        if (twice_error >= dy) { err += dy; x1 += sx; }
        if (twice_error <= dx) { err += dx; y1 += sy; }
    }
}

/* Compact 5x7 uppercase font. Lowercase input is rendered as uppercase. */
static const uint8_t font[][5] = {
    {0x1E,0x05,0x05,0x1E,0}, {0x1F,0x15,0x15,0x0A,0}, {0x0E,0x11,0x11,0x11,0},
    {0x1F,0x11,0x11,0x0E,0}, {0x1F,0x15,0x15,0x11,0}, {0x1F,0x05,0x05,0x01,0},
    {0x0E,0x11,0x15,0x1D,0}, {0x1F,0x04,0x04,0x1F,0}, {0x11,0x1F,0x11,0,0},
    {0x08,0x10,0x10,0x0F,0}, {0x1F,0x04,0x0A,0x11,0}, {0x1F,0x10,0x10,0x10,0},
    {0x1F,0x02,0x04,0x02,0x1F}, {0x1F,0x02,0x04,0x1F,0}, {0x0E,0x11,0x11,0x0E,0},
    {0x1F,0x05,0x05,0x02,0}, {0x0E,0x11,0x19,0x1E,0}, {0x1F,0x05,0x0D,0x12,0},
    {0x12,0x15,0x15,0x09,0}, {0x01,0x1F,0x01,0x01,0}, {0x0F,0x10,0x10,0x0F,0},
    {0x07,0x08,0x10,0x08,0x07}, {0x1F,0x08,0x04,0x08,0x1F}, {0x1B,0x04,0x04,0x1B,0},
    {0x03,0x04,0x18,0x04,0x03}, {0x19,0x15,0x13,0,0},
    {0x0E,0x11,0x11,0x0E,0}, {0x00,0x12,0x1F,0x10,0}, {0x19,0x15,0x15,0x12,0},
    {0x11,0x15,0x15,0x0A,0}, {0x07,0x04,0x04,0x1F,0}, {0x17,0x15,0x15,0x09,0},
    {0x0E,0x15,0x15,0x08,0}, {0x01,0x01,0x1D,0x03,0}, {0x0A,0x15,0x15,0x0A,0},
    {0x02,0x05,0x05,0x1E,0}
};

static int glyph_index(char c)
{
    if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= '0' && c <= '9') return 26 + c - '0';
    return -1;
}

void fb_draw_char(int x, int y, char character, uint32_t color)
{
    int glyph = glyph_index(character);
    if (glyph < 0) return;
    for (int column = 0; column < 5; column++)
        for (int row = 0; row < 5; row++)
            if (font[glyph][column] & (1u << row)) fb_put_pixel(x + column, y + row, color);
}

void fb_draw_string(int x, int y, const char *text, uint32_t color)
{
    if (text == 0) return;
    for (int i = 0; text[i] != '\0'; i++) {
        if (text[i] == '\n') { y += 8; x = 0; }
        else { fb_draw_char(x, y, text[i], color); x += 6; }
    }
}

void init_screen(void) { }
void set_pixel(int x, int y, uint32_t color) { fb_put_pixel(x, y, color); }
void set_rect(int x, int y, int width, int height, uint32_t color) { fb_fill_rect(x, y, width, height, color); }
void set_line(int x1, int y1, int x2, int y2, uint32_t color) { fb_draw_line(x1, y1, x2, y2, color); }
