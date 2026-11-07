#include <assert.h>

#include "ORgui.h"

static int screen_width = 800;
static int screen_height = 600;
static uint64_t ticks;
static int event_count;
static int reduced_motion = 1;
static int expected_drag_border_x;
static int expected_drag_border_y;
static int expected_drag_border_width;
static int expected_drag_border_height;
static uint32_t expected_drag_border_color;
static int drag_border_seen;

int fb_width(void) { return screen_width; }
int fb_height(void) { return screen_height; }
void fb_fill_rect(int x, int y, int width, int height, uint32_t color)
{
    (void)x;
    (void)y;
    (void)width;
    (void)height;
    (void)color;
}
void fb_draw_rect(int x, int y, int width, int height, uint32_t color)
{
    if (x == expected_drag_border_x && y == expected_drag_border_y &&
        width == expected_drag_border_width && height == expected_drag_border_height &&
        color == expected_drag_border_color)
        drag_border_seen++;
}
int font_init(void) { return 0; }
void font_draw_string(int x, int y, const char *text, int scale, uint32_t color)
{
    (void)x;
    (void)y;
    (void)text;
    (void)scale;
    (void)color;
}
uint64_t timer_get_ticks(void) { return ticks; }
int ui_animation_reduced_motion(void) { return reduced_motion; }
int ui_animation_progress(uint64_t started, uint32_t duration, int easing)
{
    (void)easing;
    uint64_t elapsed = ticks - started;
    return elapsed >= duration ? 1000 : (int)(elapsed * 1000 / duration);
}
int ui_animation_running(uint64_t started, uint32_t duration)
{
    return ticks - started < duration;
}

static uint32_t blend_colors(uint32_t from, uint32_t to, int progress)
{
    uint32_t red = (((from >> 16) & 255u) * (1000 - progress) +
                    ((to >> 16) & 255u) * progress) / 1000;
    uint32_t green = (((from >> 8) & 255u) * (1000 - progress) +
                      ((to >> 8) & 255u) * progress) / 1000;
    uint32_t blue = ((from & 255u) * (1000 - progress) +
                     (to & 255u) * progress) / 1000;
    return (red << 16) | (green << 8) | blue;
}

uint32_t ui_animation_mix_color(uint32_t from, uint32_t to, int progress)
{
    return blend_colors(from, to, progress);
}

static void count_event(ORWindow *window, const OREvent *event)
{
    (void)window;
    (void)event;
    event_count++;
}

static int send_event(OREventType type, int x, int y, int button)
{
    OREvent event = { type, 0, x, y, button };
    ORgui_handle_event(&event);
    return ORgui_event_requires_redraw(&event);
}

int main(void)
{
    ticks = 1000;
    ORgui_init();
    ORWindow *window = ORgui_create_window(80, 80, 200, 100, "window");
    ORWindow *other = ORgui_create_window(100, 100, 200, 120, "other");
    assert(window && other);
    window->on_event = count_event;
    other->on_event = count_event;
    ticks += 200;

    window->movable = 0;
    send_event(OR_EVENT_MOUSE_DOWN, 90, 90, 1);
    assert(event_count == 1);
    send_event(OR_EVENT_MOUSE_MOVE, 120, 110, 0);
    assert(window->x == 80 && window->y == 80);

    window->movable = 1;
    send_event(OR_EVENT_MOUSE_DOWN, 90, 90, 1);
    expected_drag_border_x = window->x - 1;
    expected_drag_border_y = window->y - 1;
    expected_drag_border_width = window->width + 2;
    expected_drag_border_height = window->height + 2;
    expected_drag_border_color =
        blend_colors(OR_COLOR_FIRE_ORANGE, OR_COLOR_FIRE_YELLOW, 120);
    ORgui_draw();
    assert(drag_border_seen == 1);
    assert(send_event(OR_EVENT_MOUSE_MOVE, 125, 120, 0));
    assert(window->x == 115 && window->y == 110);
    assert(!send_event(OR_EVENT_MOUSE_MOVE, 125, 120, 0));
    send_event(OR_EVENT_MOUSE_UP, 135, 130, 1);
    assert(window->x == 125 && window->y == 120);
    send_event(OR_EVENT_MOUSE_MOVE, 200, 200, 0);
    assert(window->x == 125 && window->y == 120);
    assert(other->x == 100 && other->y == 100);

    int previous_event_count = event_count;
    send_event(OR_EVENT_MOUSE_DOWN, 160, 170, 1);
    assert(event_count == previous_event_count + 1);
    assert(ORgui_active_window() == window);

    send_event(OR_EVENT_MOUSE_DOWN, window->x + window->width - 16,
               window->y + 10, 1);
    assert(!window->visible && window->close_requested);

    ORWindow *bounded = ORgui_create_window(400, 300, 200, 100, "bounded");
    assert(bounded);
    ticks += 200;
    send_event(OR_EVENT_MOUSE_DOWN, 410, 310, 1);
    send_event(OR_EVENT_MOUSE_MOVE, 0, 0, 0);
    assert(bounded->x == 0 && bounded->y == 0);
    send_event(OR_EVENT_MOUSE_MOVE, 799, 599, 0);
    assert(bounded->x == 736 && bounded->y == 552);
    send_event(OR_EVENT_MOUSE_UP, 799, 599, 1);

    ORWindow *animated = ORgui_create_window(100, 100, 200, 120, "animated");
    assert(animated);
    send_event(OR_EVENT_MOUSE_DOWN, 272, 110, 1);
    assert(!animated->visible && animated->close_requested);

    return 0;
}
