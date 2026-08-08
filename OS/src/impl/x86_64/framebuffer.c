#include "framebuffer.h"
#include "imp.h"

typedef struct {
    uint32_t type;
    uint32_t size;
    uint64_t framebuffer_addr;
    uint32_t framebuffer_pitch;
    uint32_t framebuffer_width;
    uint32_t framebuffer_height;
    uint8_t framebuffer_bpp;
    uint8_t framebuffer_type;
    uint8_t reserved[6];
} __attribute__((packed)) multiboot2_framebuffer_tag_t;

static uint32_t framebuffer_storage[1024 * 768];

uint32_t *framebuffer = 0;

int screen_width = 0;
int screen_height = 0;
int screen_pitch = 0;
int screen_bpp = 4;
int screen_bytes_per_pixel = 4;
int framebuffer_ready = 0;

void fb_init(uint32_t *fb, int width, int height, int pitch)
{
    if (fb != 0) {
        framebuffer = fb;
    } else {
        framebuffer = framebuffer_storage;
    }

    if (width <= 0) width = 1024;
    if (height <= 0) height = 768;
    if (pitch <= 0) pitch = width * 4;

    screen_width = width;
    screen_height = height;
    screen_pitch = pitch;
    screen_bpp = 32;
    screen_bytes_per_pixel = 4;
    framebuffer_ready = 1;
}

void fb_init_from_multiboot(uint64_t info_addr)
{
    imp_text("BOOT 4: framebuffer tag search started\n");

    uint32_t *info = (uint32_t *)(uintptr_t)info_addr;
    if (info == 0) {
        imp_text("framebuffer info pointer is NULL\n");
        framebuffer_ready = 0;
        return;
    }

    uint32_t total_size = info[0];
    if (total_size < 24) {
        imp_text("invalid multiboot info size\n");
        framebuffer_ready = 0;
        return;
    }

    uint32_t offset = 8;
    while (offset + 8 <= total_size) {
        uint32_t type = info[offset / 4];
        uint32_t size = info[(offset / 4) + 1];
        if (size == 0 || size < 8) {
            break;
        }

        if (type == 8) {
            imp_text("BOOT 5: framebuffer tag found\n");
            multiboot2_framebuffer_tag_t *tag = (multiboot2_framebuffer_tag_t *)((uint8_t *)info + offset);
            if (tag->framebuffer_addr == 0) {
                imp_text("framebuffer address is zero\n");
                framebuffer_ready = 0;
                return;
            }

            if (tag->framebuffer_width == 0 || tag->framebuffer_height == 0) {
                imp_text("framebuffer dimensions invalid\n");
                framebuffer_ready = 0;
                return;
            }

            if (tag->framebuffer_bpp != 32) {
                imp_text("Unsupported framebuffer BPP\n");
                framebuffer_ready = 0;
                return;
            }

            framebuffer = (uint32_t *)(uintptr_t)tag->framebuffer_addr;
            screen_width = tag->framebuffer_width;
            screen_height = tag->framebuffer_height;
            screen_bpp = tag->framebuffer_bpp;
            screen_bytes_per_pixel = (tag->framebuffer_bpp + 7) / 8;
            screen_pitch = tag->framebuffer_pitch;
            framebuffer_ready = 1;

            imp_text("framebuffer address = ");
            // Note: imp_text only prints strings; we lack a number formatter here.
            // Keep diagnostics minimal for now.
            imp_text("(address logged)\n");
            imp_text("framebuffer width = ");
            imp_text("(width logged)\n");
            imp_text("framebuffer height = ");
            imp_text("(height logged)\n");
            imp_text("framebuffer pitch = ");
            imp_text("(pitch logged)\n");
            imp_text("framebuffer bpp = ");
            imp_text("(bpp logged)\n");
            imp_text("framebuffer type = ");
            imp_text("(type logged)\n");
            return;
        }

        offset += size;
        if (offset % 8 != 0) {
            offset += 8 - (offset % 8);
        }
    }

    imp_text("framebuffer tag not found\n");
    framebuffer_ready = 0;
}

void fb_put_pixel(int x, int y, uint32_t color)
{
    if (framebuffer == 0 || x < 0 || y < 0)
        return;

    if (x >= screen_width || y >= screen_height)
        return;

    uint8_t *pixel = (uint8_t *)framebuffer + (y * screen_pitch + x * screen_bytes_per_pixel);
    uint32_t rgba = color;

    if (screen_bytes_per_pixel >= 4) {
        pixel[0] = (uint8_t)(rgba & 0xFF);
        pixel[1] = (uint8_t)((rgba >> 8) & 0xFF);
        pixel[2] = (uint8_t)((rgba >> 16) & 0xFF);
        pixel[3] = (uint8_t)((rgba >> 24) & 0xFF);
    } else if (screen_bytes_per_pixel == 2) {
        uint16_t *p16 = (uint16_t *)pixel;
        *p16 = (uint16_t)(((rgba >> 8) & 0xF800) | ((rgba >> 5) & 0x07E0) | ((rgba >> 3) & 0x001F));
    } else if (screen_bytes_per_pixel == 1) {
        pixel[0] = (uint8_t)(rgba & 0xFF);
    }
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
        for (int ix = 0; ix < width; ix++)
            fb_put_pixel(x + ix, y + iy, color);
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
    /* 5x7 bitmap glyphs.  Keeping this here makes text a framebuffer service,
     * rather than making every ORgui caller know about pixels. */
    static const uint8_t digits[10][7] = {
        {14,17,19,21,25,17,14}, {4,12,4,4,4,4,14},
        {14,17,1,2,4,8,31}, {30,1,1,14,1,1,30},
        {2,6,10,18,31,2,2}, {31,16,30,1,1,17,14},
        {6,8,16,30,17,17,14}, {31,1,2,4,8,8,8},
        {14,17,17,14,17,17,14}, {14,17,17,15,1,2,12}
    };
    uint8_t rows[7] = {0, 0, 0, 0, 0, 0, 0};
    char upper = c;
    if (upper >= 'a' && upper <= 'z') upper = (char)(upper - 'a' + 'A');

    if (upper >= '0' && upper <= '9') {
        for (int row = 0; row < 7; row++) rows[row] = digits[upper - '0'][row];
    } else {
        switch (upper) {
        case 'A': { uint8_t g[7]={14,17,17,31,17,17,17}; for(int i=0;i<7;i++)rows[i]=g[i]; break; }
        case 'B': { uint8_t g[7]={30,17,17,30,17,17,30}; for(int i=0;i<7;i++)rows[i]=g[i]; break; }
        case 'C': { uint8_t g[7]={14,17,16,16,16,17,14}; for(int i=0;i<7;i++)rows[i]=g[i]; break; }
        case 'D': { uint8_t g[7]={30,17,17,17,17,17,30}; for(int i=0;i<7;i++)rows[i]=g[i]; break; }
        case 'E': { uint8_t g[7]={31,16,16,30,16,16,31}; for(int i=0;i<7;i++)rows[i]=g[i]; break; }
        case 'F': { uint8_t g[7]={31,16,16,30,16,16,16}; for(int i=0;i<7;i++)rows[i]=g[i]; break; }
        case 'G': { uint8_t g[7]={14,17,16,23,17,17,14}; for(int i=0;i<7;i++)rows[i]=g[i]; break; }
        case 'H': { uint8_t g[7]={17,17,17,31,17,17,17}; for(int i=0;i<7;i++)rows[i]=g[i]; break; }
        case 'I': { uint8_t g[7]={14,4,4,4,4,4,14}; for(int i=0;i<7;i++)rows[i]=g[i]; break; }
        case 'J': { uint8_t g[7]={1,1,1,1,17,17,14}; for(int i=0;i<7;i++)rows[i]=g[i]; break; }
        case 'K': { uint8_t g[7]={17,18,20,24,20,18,17}; for(int i=0;i<7;i++)rows[i]=g[i]; break; }
        case 'L': { uint8_t g[7]={16,16,16,16,16,16,31}; for(int i=0;i<7;i++)rows[i]=g[i]; break; }
        case 'M': { uint8_t g[7]={17,27,21,21,17,17,17}; for(int i=0;i<7;i++)rows[i]=g[i]; break; }
        case 'N': { uint8_t g[7]={17,25,21,19,17,17,17}; for(int i=0;i<7;i++)rows[i]=g[i]; break; }
        case 'O': { uint8_t g[7]={14,17,17,17,17,17,14}; for(int i=0;i<7;i++)rows[i]=g[i]; break; }
        case 'P': { uint8_t g[7]={30,17,17,30,16,16,16}; for(int i=0;i<7;i++)rows[i]=g[i]; break; }
        case 'Q': { uint8_t g[7]={14,17,17,17,21,18,13}; for(int i=0;i<7;i++)rows[i]=g[i]; break; }
        case 'R': { uint8_t g[7]={30,17,17,30,20,18,17}; for(int i=0;i<7;i++)rows[i]=g[i]; break; }
        case 'S': { uint8_t g[7]={15,16,16,14,1,1,30}; for(int i=0;i<7;i++)rows[i]=g[i]; break; }
        case 'T': { uint8_t g[7]={31,4,4,4,4,4,4}; for(int i=0;i<7;i++)rows[i]=g[i]; break; }
        case 'U': { uint8_t g[7]={17,17,17,17,17,17,14}; for(int i=0;i<7;i++)rows[i]=g[i]; break; }
        case 'V': { uint8_t g[7]={17,17,17,17,17,10,4}; for(int i=0;i<7;i++)rows[i]=g[i]; break; }
        case 'W': { uint8_t g[7]={17,17,17,21,21,21,10}; for(int i=0;i<7;i++)rows[i]=g[i]; break; }
        case 'X': { uint8_t g[7]={17,17,10,4,10,17,17}; for(int i=0;i<7;i++)rows[i]=g[i]; break; }
        case 'Y': { uint8_t g[7]={17,17,10,4,4,4,4}; for(int i=0;i<7;i++)rows[i]=g[i]; break; }
        case 'Z': { uint8_t g[7]={31,1,2,4,8,16,31}; for(int i=0;i<7;i++)rows[i]=g[i]; break; }
        case '-': rows[3] = 31; break;
        case '.': rows[6] = 4; break;
        case ':': rows[2] = 4; rows[5] = 4; break;
        case '/': rows[0]=1; rows[1]=2; rows[2]=4; rows[3]=8; rows[4]=16; break;
        case '|': for(int i=0;i<7;i++)rows[i]=4; break;
        case '>': rows[1]=8; rows[2]=4; rows[3]=2; rows[4]=4; rows[5]=8; break;
        case '<': rows[1]=2; rows[2]=4; rows[3]=8; rows[4]=4; rows[5]=2; break;
        default: break;
        }
    }

    for (int row = 0; row < 7; row++)
        for (int col = 0; col < 5; col++)
            if (rows[row] & (1u << (4 - col))) fb_put_pixel(x + col, y + row, color);
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

    for (int y = 0; y < screen_height; y++) {
        for (int x = 0; x < screen_width; x++) {
            fb_put_pixel(x, y, color);
        }
    }
}
