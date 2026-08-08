#include "ORgui.h"
#include "keyboard.h"
#include <stddef.h>

ORguiContext ORgui;

static uint32_t ORgui_theme_window = 0xFF1A0B0A;
static uint32_t ORgui_theme_border = 0xFF7A1C11;
static uint32_t ORgui_theme_title = 0xFFB23A16;
static uint32_t ORgui_theme_fire_red = 0xFFB8201C;
static uint32_t ORgui_theme_fire_orange = 0xFFD95A12;
static uint32_t ORgui_theme_fire_yellow = 0xFFE8C34E;
static uint32_t ORgui_theme_text = 0xFFF7E2C8;

void ORgui_init(void)
{
    ORgui.window_count = 0;
    ORgui.active_window = -1;
    ORgui.mouse_x = 0;
    ORgui.mouse_y = 0;
    ORgui.mouse_button = 0;
    ORgui.mouse_visible = 1;
}

void ORgui_begin_frame(void)
{
    if (screen_width <= 0 || screen_height <= 0) {
        return;
    }
}

void ORgui_end_frame(void)
{
}

void ORgui_draw_panel(int x, int y, int width, int height, uint32_t color, uint32_t border)
{
    fb_fill_rect(x, y, width, height, color);
    fb_draw_rect(x, y, width, height, border);
}

void ORgui_draw_button(int x, int y, int width, int height, const char *label, uint32_t base_color, int pressed)
{
    uint32_t fill = pressed ? ORgui_theme_fire_red : base_color;
    uint32_t border = pressed ? ORgui_theme_fire_yellow : ORgui_theme_border;
    fb_fill_rect(x, y, width, height, fill);
    fb_draw_rect(x, y, width, height, border);
    ORgui_draw_text(x + 8, y + 6, label, ORgui_theme_text);
}

void ORgui_draw_text(int x, int y, const char *text, uint32_t color)
{
    fb_draw_string(x, y, text, color);
}

void ORgui_draw_window(ORWindow *window)
{
    if (window == NULL || !window->visible) {
        return;
    }

    uint32_t title_color = window->focused ? ORgui_theme_title : ORgui_theme_fire_orange;
    fb_fill_rect(window->x, window->y, window->width, window->height, ORgui_theme_window);
    fb_fill_rect(window->x, window->y, window->width, 24, title_color);
    fb_draw_rect(window->x, window->y, window->width, window->height, ORgui_theme_border);
    ORgui_draw_text(window->x + 8, window->y + 6, window->title, ORgui_theme_text);

    if (window->closeable) {
        fb_fill_rect(window->x + window->width - 18, window->y + 4, 12, 12, ORgui_theme_fire_red);
        ORgui_draw_text(window->x + window->width - 15, window->y + 6, "x", ORgui_theme_text);
    }
}

ORWindow *ORgui_create_window(const char *title, int x, int y, int width, int height)
{
    if (ORgui.window_count >= 8) {
        return NULL;
    }

    ORWindow *window = &ORgui.windows[ORgui.window_count++];
    window->x = x;
    window->y = y;
    window->width = width;
    window->height = height;
    window->background = ORgui_theme_window;
    window->border = ORgui_theme_border;
    window->title = title;
    window->visible = 1;
    window->movable = 1;
    window->focused = 1;
    window->drag = 0;
    window->closeable = 1;
    window->close_requested = 0;
    window->drag_offset_x = 0;
    window->drag_offset_y = 0;
    return window;
}

void ORgui_destroy_window(ORWindow *window)
{
    if (window == NULL) {
        return;
    }

    for (int i = 0; i < ORgui.window_count; i++) {
        if (&ORgui.windows[i] == window) {
            for (int j = i; j + 1 < ORgui.window_count; j++) {
                ORgui.windows[j] = ORgui.windows[j + 1];
            }
            ORgui.window_count--;
            break;
        }
    }
}

void ORgui_show_window(ORWindow *window)
{
    if (window != NULL) {
        window->visible = 1;
    }
}

void ORgui_hide_window(ORWindow *window)
{
    if (window != NULL) {
        window->visible = 0;
    }
}

void ORgui_select_window(ORWindow *window)
{
    if (window == NULL) {
        return;
    }

    for (int i = 0; i < ORgui.window_count; i++) {
        ORgui.windows[i].focused = 0;
    }

    window->focused = 1;
    ORgui.active_window = (int)(window - ORgui.windows);
}

void ORgui_draw_windows(void)
{
    for (int i = 0; i < ORgui.window_count; i++) {
        ORgui_draw_window(&ORgui.windows[i]);
    }
}

void ORgui_handle_event(const OREvent *event)
{
    if (event == NULL) {
        return;
    }

    if (event->type == OR_EVENT_MOUSE_DOWN) {
        ORgui.mouse_button = event->button;
        ORgui.mouse_x = event->mouse_x;
        ORgui.mouse_y = event->mouse_y;
        for (int i = ORgui.window_count - 1; i >= 0; i--) {
            ORWindow *window = &ORgui.windows[i];
            if (!window->visible) {
                continue;
            }

            if (event->mouse_x >= window->x && event->mouse_x < window->x + window->width &&
                event->mouse_y >= window->y && event->mouse_y < window->y + window->height) {
                ORgui_select_window(window);
                if (window->closeable && event->mouse_x >= window->x + window->width - 18 &&
                    event->mouse_x < window->x + window->width - 6 &&
                    event->mouse_y >= window->y + 4 && event->mouse_y < window->y + 16) {
                    window->close_requested = 1;
                }
                break;
            }
        }
    }

    if (event->type == OR_EVENT_MOUSE_MOVE) {
        ORgui.mouse_x = event->mouse_x;
        ORgui.mouse_y = event->mouse_y;
    }
}

void ORgui_update(void)
{
    for (int i = 0; i < ORgui.window_count; i++) {
        if (ORgui.windows[i].close_requested) {
            ORgui_destroy_window(&ORgui.windows[i]);
            i--;
        }
    }
}

void ORgui_set_mouse(int x, int y, int button)
{
    ORgui.mouse_x = x;
    ORgui.mouse_y = y;
    ORgui.mouse_button = button;
}

void ORgui_set_mouse_visible(int visible)
{
    ORgui.mouse_visible = visible;
}
