#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "framebuffer.h"

typedef enum {
    OR_EVENT_NONE = 0,
    OR_EVENT_KEY,
    OR_EVENT_MOUSE_MOVE,
    OR_EVENT_MOUSE_DOWN,
    OR_EVENT_MOUSE_UP
} OREventType;

typedef struct {
    OREventType type;
    int key;
    int mouse_x;
    int mouse_y;
    int button;
    int pressed;
} OREvent;

typedef struct {
    int x;
    int y;
    int width;
    int height;
    uint32_t background;
    uint32_t border;
    const char *title;
    int visible;
    int movable;
    int focused;
    int drag;
    int closeable;
    int close_requested;
    int drag_offset_x;
    int drag_offset_y;
} ORWindow;

typedef struct {
    int x;
    int y;
    int width;
    int height;
    uint32_t color;
    uint32_t border;
    int visible;
} ORPanel;

typedef struct {
    int x;
    int y;
    int width;
    int height;
    uint32_t background;
    uint32_t border;
    uint32_t text_color;
    const char *text;
    int visible;
    int pressed;
} ORButton;

typedef struct {
    ORWindow windows[8];
    int window_count;
    int active_window;
    int mouse_x;
    int mouse_y;
    int mouse_button;
    int mouse_visible;
} ORguiContext;

extern ORguiContext ORgui;

void ORgui_init(void);
void ORgui_begin_frame(void);
void ORgui_end_frame(void);

void ORgui_draw_panel(int x, int y, int width, int height, uint32_t color, uint32_t border);
void ORgui_draw_button(int x, int y, int width, int height, const char *label, uint32_t base_color, int pressed);
void ORgui_draw_text(int x, int y, const char *text, uint32_t color);
void ORgui_draw_window(ORWindow *window);

ORWindow *ORgui_create_window(const char *title, int x, int y, int width, int height);
void ORgui_destroy_window(ORWindow *window);
void ORgui_show_window(ORWindow *window);
void ORgui_hide_window(ORWindow *window);
void ORgui_select_window(ORWindow *window);
void ORgui_draw_windows(void);
void ORgui_handle_event(const OREvent *event);
void ORgui_update(void);

void ORgui_set_mouse(int x, int y, int button);
void ORgui_set_mouse_visible(int visible);
