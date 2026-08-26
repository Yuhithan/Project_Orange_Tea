#pragma once

#include <stdint.h>

#define ORGUI_MAX_WINDOWS 8

enum {
    OR_COLOR_BACKGROUND  = 0x1565C0,
    OR_COLOR_PANEL       = 0x0D47A1,
    OR_COLOR_WINDOW      = 0xE3F2FD,
    OR_COLOR_BORDER      = 0x08306B,
    OR_COLOR_TITLEBAR    = 0x1976D2,
    OR_COLOR_FIRE_RED    = 0x0B3D91,
    OR_COLOR_FIRE_ORANGE = 0x2196F3,
    OR_COLOR_FIRE_YELLOW = 0xBBDEFB,
    OR_COLOR_TEXT        = 0xFFFFFF,
    OR_COLOR_TEXT_DIM    = 0x0D47A1
};

typedef enum {
    OR_EVENT_NONE,
    OR_EVENT_KEY_DOWN,
    OR_EVENT_MOUSE_MOVE,
    OR_EVENT_MOUSE_DOWN,
    OR_EVENT_MOUSE_UP
} OREventType;

typedef struct {
    OREventType type;
    int key;
    int x;
    int y;
    int button;
} OREvent;

struct ORWindow;
typedef void (*ORWindowEventHandler)(struct ORWindow *window, const OREvent *event);
typedef void (*ORWindowDrawHandler)(struct ORWindow *window);

typedef struct ORWindow {
    int x, y, width, height;
    const char *title;
    int visible;
    int movable;
    int active;
    int close_requested;
    ORWindowEventHandler on_event;
    ORWindowDrawHandler on_draw;
} ORWindow;

void ORgui_init(void);
ORWindow *ORgui_create_window(int x, int y, int width, int height, const char *title);
void ORgui_destroy_window(ORWindow *window);
void ORgui_set_active(ORWindow *window);
ORWindow *ORgui_active_window(void);
int ORgui_window_count(void);
ORWindow *ORgui_window_at(int index);
void ORgui_handle_event(const OREvent *event);
void ORgui_draw(void);
void ORgui_draw_panel(int x, int y, int width, int height, uint32_t color);
void ORgui_draw_button(int x, int y, int width, int height, const char *label, int pressed);
void ORgui_draw_text(int x, int y, const char *text, uint32_t color);
