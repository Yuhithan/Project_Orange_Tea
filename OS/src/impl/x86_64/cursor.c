#include "cursor.h"
#include "framebuffer.h"

static int cursor_x;
static int cursor_y;
static const int cursor_radius = 7;

void cursor_init(int x, int y)
{
    cursor_x = x;
    cursor_y = y;
}

void cursor_move(int dx, int dy)
{
    cursor_x += dx;
    cursor_y += dy;

    if (cursor_x < cursor_radius) cursor_x = cursor_radius;
    if (cursor_y < cursor_radius) cursor_y = cursor_radius;
    if (cursor_x >= screen_width - cursor_radius) cursor_x = screen_width - cursor_radius - 1;
    if (cursor_y >= screen_height - cursor_radius) cursor_y = screen_height - cursor_radius - 1;
}

void cursor_draw(void)
{
    for (int y = -cursor_radius; y <= cursor_radius; y++) {
        for (int x = -cursor_radius; x <= cursor_radius; x++) {
            int distance_squared = x * x + y * y;
            int radius_squared = cursor_radius * cursor_radius;

            if (distance_squared <= radius_squared) {
                uint32_t color = distance_squared > 25 ? 0xFF12345E : 0xFFFFFFFF;
                fb_put_pixel(cursor_x + x, cursor_y + y, color);
            }
        }
    }
}

int cursor_get_x(void) { return cursor_x; }
int cursor_get_y(void) { return cursor_y; }
