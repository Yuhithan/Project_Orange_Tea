#include "ORgui.h"
#include "framebuffer.h"
#include "ui/font.h"
#include "timer.h"
#include "ui_animation.h"

#define ORGUI_WINDOW_ANIMATION_MS 140u
#define ORGUI_WINDOW_SCALE_MIN 940

static ORWindow windows[ORGUI_MAX_WINDOWS];
static int z_order[ORGUI_MAX_WINDOWS];
static int window_count;
static ORWindow *active_window;
static ORWindow *drag_window;
static int drag_offset_x;
static int drag_offset_y;
static uint64_t window_animation_started[ORGUI_MAX_WINDOWS];
static uint64_t window_close_started[ORGUI_MAX_WINDOWS];
static int window_closing[ORGUI_MAX_WINDOWS];
static int pointer_x;
static int pointer_y;
static int hovered_close = -1;
static int drag_position_changed;

static int window_index(const ORWindow *window)
{
    int index = (int)(window - windows);
    return index >= 0 && index < window_count && &windows[index] == window ? index : -1;
}

static void activate_previous_window(void)
{
    active_window = 0;
    for (int i = window_count - 1; i >= 0; i--) {
        ORWindow *candidate = &windows[z_order[i]];
        if (candidate->visible && !window_closing[z_order[i]]) {
            ORgui_set_active(candidate);
            break;
        }
    }
}

static int within(int x, int y, int left, int top, int width, int height)
{
    return x >= left && y >= top && x < left + width && y < top + height;
}

static int window_scale(const ORWindow *window)
{
    int index = window_index(window);
    if (index < 0) return 1000;
    if (window_closing[index]) {
        int progress = ui_animation_progress(window_close_started[index],
                                             ORGUI_WINDOW_ANIMATION_MS,
                                             UI_ANIMATION_EASE_IN);
        return 1000 - (1000 - ORGUI_WINDOW_SCALE_MIN) * progress / 1000;
    }
    if (ui_animation_running(window_animation_started[index],
                             ORGUI_WINDOW_ANIMATION_MS)) {
        int progress = ui_animation_progress(window_animation_started[index],
                                             ORGUI_WINDOW_ANIMATION_MS,
                                             UI_ANIMATION_EASE_OUT);
        return ORGUI_WINDOW_SCALE_MIN +
            (1000 - ORGUI_WINDOW_SCALE_MIN) * progress / 1000;
    }
    return 1000;
}

static void window_display_bounds(const ORWindow *window, int *x, int *y,
                                  int *width, int *height)
{
    int scale = window_scale(window);
    int actual_x = window->x;
    int actual_y = window->y;
    int actual_width = window->width;
    int actual_height = window->height;
    *width = actual_width * scale / 1000;
    *height = actual_height * scale / 1000;
    *x = actual_x + (actual_width - *width) / 2;
    *y = actual_y + (actual_height - *height) / 2;
}

static void clamp_window_position(const ORWindow *window, int *x, int *y)
{
    int screen_width = fb_width();
    int screen_height = fb_height();
    int visible_width = window->width < 64 ? window->width : 64;
    int visible_height = window->height < 20 ? window->height : 20;

    if (*x < 0) *x = 0;
    if (*y < 0) *y = 0;
    if (screen_width > 0) {
        int max_x = screen_width - visible_width;
        if (max_x < 0) max_x = 0;
        if (*x > max_x) *x = max_x;
    }
    if (screen_height > 0) {
        int usable_height = screen_height >= 28 ? screen_height - 28 : screen_height;
        int max_y = usable_height - visible_height;
        if (max_y < 0) max_y = 0;
        if (*y > max_y) *y = max_y;
    }
}

static ORWindow *window_at_point(int x, int y)
{
    for (int i = window_count - 1; i >= 0; i--) {
        int index = z_order[i];
        ORWindow *window = &windows[index];
        int display_x, display_y, display_width, display_height;
        if (!window->visible || window_closing[index]) continue;
        window_display_bounds(window, &display_x, &display_y,
                              &display_width, &display_height);
        if (within(x, y, display_x, display_y, display_width, display_height))
            return window;
    }
    return 0;
}

static void deliver_mouse_event(ORWindow *window, const OREvent *event)
{
    if (!window || !window->on_event) return;
    int actual_x = window->x;
    int actual_y = window->y;
    int actual_width = window->width;
    int actual_height = window->height;
    window_display_bounds(window, &window->x, &window->y,
                          &window->width, &window->height);
    window->on_event(window, event);
    window->x = actual_x;
    window->y = actual_y;
    window->width = actual_width;
    window->height = actual_height;
}

static void update_drag_position(int x, int y)
{
    int display_x, display_y, display_width, display_height;
    window_display_bounds(drag_window, &display_x, &display_y,
                          &display_width, &display_height);
    int new_x = x - drag_offset_x - (display_x - drag_window->x);
    int new_y = y - drag_offset_y - (display_y - drag_window->y);
    clamp_window_position(drag_window, &new_x, &new_y);
    if (drag_window->x != new_x || drag_window->y != new_y) {
        drag_window->x = new_x;
        drag_window->y = new_y;
        drag_position_changed = 1;
    }
}

void ORgui_init(void)
{
    font_init();

    window_count = 0;
    active_window = 0;
    drag_window = 0;
    pointer_x = pointer_y = 0;
    hovered_close = -1;
    drag_position_changed = 0;

    for (int i = 0; i < ORGUI_MAX_WINDOWS; i++) {
        windows[i].visible = 0;
        z_order[i] = i;
        window_animation_started[i] = 0;
        window_close_started[i] = 0;
        window_closing[i] = 0;
    }
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
    clamp_window_position(window, &window->x, &window->y);
    window_animation_started[index] = timer_get_ticks();
    window_close_started[index] = 0;
    window_closing[index] = 0;
    ORgui_set_active(window);
    return window;
}

void ORgui_destroy_window(ORWindow *window)
{
    int index = window ? window_index(window) : -1;
    if (index < 0 || !window->visible || window_closing[index]) return;
    window->close_requested = 1;
    if (!ui_animation_reduced_motion()) {
        window_closing[index] = 1;
        window_close_started[index] = timer_get_ticks();
        window->active = 0;
        if (active_window == window) activate_previous_window();
        if (drag_window == window) drag_window = 0;
        return;
    }
    window->visible = 0;
    window->active = 0;
    if (active_window == window) activate_previous_window();
    if (drag_window == window) drag_window = 0;
}

void ORgui_set_active(ORWindow *window)
{
    if (window == 0 || !window->visible) return;
    int index = (int)(window - windows);
    if (index < 0 || index >= window_count || &windows[index] != window ||
        window_closing[index]) return;
    for (int i = 0; i < window_count; i++) windows[i].active = 0;
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
    //fb_draw_string(x, y, text, color);
    font_draw_string(x, y, text, 1, color);
}

void ORgui_draw_button(int x, int y, int width, int height, const char *label, int pressed)
{
    uint32_t fill = pressed ? OR_COLOR_FIRE_RED : OR_COLOR_FIRE_ORANGE;
    if (!pressed && within(pointer_x, pointer_y, x, y, width, height))
        fill = ui_animation_mix_color(fill, OR_COLOR_FIRE_YELLOW, 180);
    fb_fill_rect(x, y, width, height, fill);
    fb_draw_rect(x, y, width, height, OR_COLOR_FIRE_YELLOW);
    font_draw_string(x + 8, y + (height - 7) / 2, label, 1, OR_COLOR_TEXT);
}

static void draw_window(ORWindow *window)
{
    if (!window->visible) return;
    int index = window_index(window);
    if (index < 0) return;
    ORWindow actual = *window;
    window_display_bounds(window, &window->x, &window->y,
                          &window->width, &window->height);
    uint32_t title_color = window->active ? OR_COLOR_TITLEBAR : OR_COLOR_PANEL;
    fb_fill_rect(window->x, window->y, window->width, window->height, OR_COLOR_WINDOW);
    fb_fill_rect(window->x + 1, window->y + 1, window->width - 2, 20, title_color);
    font_draw_string(window->x + 7, window->y + 7, window->title, 1, OR_COLOR_TEXT);
    ORgui_draw_button(window->x + window->width - 24, window->y + 4, 17, 14, "X", 0);
    if (window->on_draw) window->on_draw(window);
    fb_draw_rect(window->x, window->y, window->width, window->height,
                 window->active ? OR_COLOR_FIRE_ORANGE : OR_COLOR_BORDER);
    if (drag_window == window) {
        uint32_t drag_outline =
            ui_animation_mix_color(OR_COLOR_FIRE_ORANGE, OR_COLOR_FIRE_YELLOW, 120);
        fb_draw_rect(window->x - 1, window->y - 1, window->width + 2,
                     window->height + 2, drag_outline);
    }
    *window = actual;
}

int ORgui_track_pointer(const OREvent *event)
{
    if (!event || event->type != OR_EVENT_MOUSE_MOVE) return 0;
    pointer_x = event->x;
    pointer_y = event->y;
    int next_hovered_close = -1;
    ORWindow *window = window_at_point(pointer_x, pointer_y);
    if (window) {
        int display_x, display_y, display_width, display_height;
        window_display_bounds(window, &display_x, &display_y,
                              &display_width, &display_height);
        if (within(pointer_x, pointer_y, display_x + display_width - 24,
                   display_y + 4, 17, 14))
            next_hovered_close = window_index(window);
    }
    int changed = next_hovered_close != hovered_close;
    hovered_close = next_hovered_close;
    return changed;
}

void ORgui_update_animations(void)
{
    for (int i = 0; i < window_count; i++) {
        if (window_closing[i] &&
            !ui_animation_running(window_close_started[i], ORGUI_WINDOW_ANIMATION_MS)) {
            windows[i].visible = 0;
            window_closing[i] = 0;
            window_close_started[i] = 0;
        }
    }
}

int ORgui_animations_active(void)
{
    for (int i = 0; i < window_count; i++) {
        if (window_closing[i] ||
            (windows[i].visible && ui_animation_running(window_animation_started[i],
                                                        ORGUI_WINDOW_ANIMATION_MS)))
            return 1;
    }
    return 0;
}

int ORgui_pointer_x(void) { return pointer_x; }
int ORgui_pointer_y(void) { return pointer_y; }

void ORgui_handle_event(const OREvent *event)
{
    if (!event) return;
    drag_position_changed = 0;

    if (event->type == OR_EVENT_MOUSE_MOVE) {
        if (drag_window) {
            int index = window_index(drag_window);
            if (index < 0 || !drag_window->visible || window_closing[index]) {
                drag_window = 0;
                return;
            }
            update_drag_position(event->x, event->y);
            return;
        }
        deliver_mouse_event(window_at_point(event->x, event->y), event);
        return;
    }

    if (event->type == OR_EVENT_MOUSE_UP && event->button == 1) {
        if (drag_window) {
            ORWindow *released_window = drag_window;
            int index = window_index(released_window);
            if (index >= 0 && released_window->visible && !window_closing[index]) {
                update_drag_position(event->x, event->y);
                drag_window = 0;
                deliver_mouse_event(released_window, event);
                return;
            }
        }
        drag_window = 0;
        deliver_mouse_event(window_at_point(event->x, event->y), event);
        return;
    }

    if (event->type == OR_EVENT_MOUSE_DOWN) {
        ORWindow *window = window_at_point(event->x, event->y);
        if (!window) return;
        if (event->button == 1) {
            ORgui_set_active(window);
            int display_x, display_y, display_width, display_height;
            window_display_bounds(window, &display_x, &display_y,
                                  &display_width, &display_height);
            if (within(event->x, event->y, display_x + display_width - 24,
                       display_y + 4, 17, 14)) {
                ORgui_destroy_window(window);
                return;
            }
            if (window->movable &&
                within(event->x, event->y, display_x, display_y,
                       display_width, 20)) {
                drag_window = window;
                drag_offset_x = event->x - display_x;
                drag_offset_y = event->y - display_y;
                return;
            }
        }
        deliver_mouse_event(window, event);
        return;
    }

    if (event->type == OR_EVENT_MOUSE_UP) {
        deliver_mouse_event(window_at_point(event->x, event->y), event);
        return;
    }

    int active_index = active_window ? window_index(active_window) : -1;
    if (event->type == OR_EVENT_KEY_DOWN && active_index >= 0 &&
        active_window->visible && !window_closing[active_index] &&
        active_window->on_event) {
        active_window->on_event(active_window, event);
    }
}

int ORgui_event_requires_redraw(const OREvent *event)
{
    if (event == 0) return 0;
    return event->type != OR_EVENT_MOUSE_MOVE || drag_position_changed;
}

void ORgui_draw(void)
{
    for (int i = 0; i < window_count; i++) draw_window(&windows[z_order[i]]);
}
