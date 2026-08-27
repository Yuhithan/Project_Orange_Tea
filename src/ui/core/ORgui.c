#include "ORgui.h"
#include "framebuffer.h"

static ORWindow windows[ORGUI_MAX_WINDOWS];
static int z_order[ORGUI_MAX_WINDOWS];
static int window_count;
static ORWindow *active_window;
static ORWindow *drag_window;
static int drag_offset_x;
static int drag_offset_y;

static int within(int x, int y, int left, int top, int width, int height)
{
    return x >= left && y >= top && x < left + width && y < top + height;
}

void ORgui_init(void)
{
    window_count = 0;
    active_window = 0;
    drag_window = 0;
    for (int i = 0; i < ORGUI_MAX_WINDOWS; i++) { windows[i].visible = 0; z_order[i] = i; }
}

ORWindow *ORgui_create_window(int x, int y, int width, int height, const char *title)
{
    if (width < 80 || height < 55 || title == 0) return 0;
    int index = 0;
    while (index < window_count && windows[index].visible) index++;
    if (index == window_count) {
        if (window_count >= ORGUI_MAX_WINDOWS) return 0;
        z_order[window_count] = window_count;
        window_count++;
    }
    ORWindow *window = &windows[index];
    window->x = x;
    window->y = y;
    window->width = width;
    window->height = height;
    window->title = title;
    window->visible = 1;
    window->movable = 1;
    window->active = 0;
    window->close_requested = 0;
    window->on_event = 0;
    window->on_draw = 0;
    ORgui_set_active(window);
    return window;
}

void ORgui_destroy_window(ORWindow *window)
{
    if (window == 0) return;
    window->visible = 0;
    window->close_requested = 1;
    if (active_window == window) {
        active_window = 0;
        for (int i = window_count - 1; i >= 0; i--) {
            if (windows[z_order[i]].visible) {
                ORgui_set_active(&windows[z_order[i]]);
                break;
            }
        }
    }
    if (drag_window == window) drag_window = 0;
}

void ORgui_set_active(ORWindow *window)
{
    if (window == 0 || !window->visible) return;
    for (int i = 0; i < window_count; i++) windows[i].active = 0;
    int index = (int)(window - windows);
    int position = 0;
    while (position < window_count && z_order[position] != index) position++;
    if (position < window_count) {
        for (int i = position; i + 1 < window_count; i++) z_order[i] = z_order[i + 1];
        z_order[window_count - 1] = index;
    }
    window->active = 1;
    active_window = window;
}

ORWindow *ORgui_active_window(void) { return active_window; }
int ORgui_window_count(void) { return window_count; }
ORWindow *ORgui_window_at(int index) { return index >= 0 && index < window_count ? &windows[index] : 0; }

void ORgui_draw_panel(int x, int y, int width, int height, uint32_t color)
{
    fb_fill_rect(x, y, width, height, color);
    fb_draw_rect(x, y, width, height, OR_COLOR_BORDER);
}

void ORgui_draw_text(int x, int y, const char *text, uint32_t color)
{
    fb_draw_string(x, y, text, color);
}

void ORgui_draw_button(int x, int y, int width, int height, const char *label, int pressed)
{
    uint32_t fill = pressed ? OR_COLOR_FIRE_RED : OR_COLOR_FIRE_ORANGE;
    fb_fill_rect(x, y, width, height, fill);
    fb_draw_rect(x, y, width, height, OR_COLOR_FIRE_YELLOW);
    fb_draw_string(x + 8, y + (height - 5) / 2, label, OR_COLOR_TEXT);
}

static void draw_window(ORWindow *window)
{
    if (!window->visible) return;
    uint32_t title_color = window->active ? OR_COLOR_TITLEBAR : OR_COLOR_PANEL;
    fb_fill_rect(window->x, window->y, window->width, window->height, OR_COLOR_WINDOW);
    fb_draw_rect(window->x, window->y, window->width, window->height, OR_COLOR_BORDER);
    fb_fill_rect(window->x + 1, window->y + 1, window->width - 2, 20, title_color);
    fb_draw_string(window->x + 7, window->y + 8, window->title, OR_COLOR_TEXT);
    ORgui_draw_button(window->x + window->width - 24, window->y + 4, 17, 14, "X", 0);
    if (window->on_draw) window->on_draw(window);
}

void ORgui_handle_event(const OREvent *event)
{
    if (event == 0) return;
    if (event->type == OR_EVENT_MOUSE_MOVE && drag_window && drag_window->visible) {
        drag_window->x = event->x - drag_offset_x;
        drag_window->y = event->y - drag_offset_y;
        if (drag_window->x < 0) drag_window->x = 0;
        if (drag_window->y < 0) drag_window->y = 0;
        return;
    }
    if (event->type == OR_EVENT_MOUSE_UP && event->button == 1) drag_window = 0;
    if (event->type == OR_EVENT_MOUSE_DOWN) {
        if (event->button != 1) return;
        for (int i = window_count - 1; i >= 0; i--) {
            ORWindow *window = &windows[z_order[i]];
            if (!window->visible || !within(event->x, event->y, window->x, window->y, window->width, window->height)) continue;
            ORgui_set_active(window);
            if (within(event->x, event->y, window->x + window->width - 24, window->y + 4, 17, 14))
                ORgui_destroy_window(window);
            else if (window->movable && within(event->x, event->y, window->x, window->y, window->width, 20)) {
                drag_window = window;
                drag_offset_x = event->x - window->x;
                drag_offset_y = event->y - window->y;
            }
            else if (window->on_event) window->on_event(window, event);
            return;
        }
    }
    if (active_window && active_window->visible && active_window->on_event)
        active_window->on_event(active_window, event);
}

void ORgui_draw(void)
{
    for (int i = 0; i < window_count; i++) draw_window(&windows[z_order[i]]);
}
