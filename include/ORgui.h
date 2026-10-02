#pragma once

#include <stdint.h>

#define ORGUI_MAX_WINDOWS 8

enum {
    /* =========================
     * Orange Tea OS colors
     * ========================= */

    OR_COLOR_BACKGROUND    = 0x18090A,
    OR_COLOR_PANEL         = 0x7F0000,
    OR_COLOR_WINDOW        = 0xFFEBEE,
    OR_COLOR_BORDER        = 0x966931,
    OR_COLOR_TITLEBAR      = 0xD32F2F,
    OR_COLOR_TEXT          = 0xFFFFFF,

    OR_COLOR_FIRE_RED      = 0x420500,
    OR_COLOR_FIRE_ORANGE   = 0xCC6000,
    OR_COLOR_FIRE_YELLOW   = 0xFFD700,

    OR_COLOR_TEXT_DIM      = 0x7F0000,


    /* =========================
     * Grayscale
     * ========================= */

    OR_COLOR_BLACK         = 0x000000,
    OR_COLOR_DARK_GRAY     = 0x202020,
    OR_COLOR_GRAY          = 0x808080,
    OR_COLOR_LIGHT_GRAY    = 0xC0C0C0,
    OR_COLOR_SILVER        = 0xC0C0C0,
    OR_COLOR_WHITE         = 0xFFFFFF,


    /* =========================
     * Primary colors
     * ========================= */

    OR_COLOR_RED           = 0xFF0000,
    OR_COLOR_GREEN         = 0x00FF00,
    OR_COLOR_BLUE          = 0x0000FF,


    /* =========================
     * Secondary colors
     * ========================= */

    OR_COLOR_YELLOW        = 0xFFFF00,
    OR_COLOR_CYAN          = 0x00FFFF,
    OR_COLOR_MAGENTA       = 0xFF00FF,


    /* =========================
     * Common colors
     * ========================= */

    OR_COLOR_MAROON        = 0x800000,
    OR_COLOR_OLIVE         = 0x808000,
    OR_COLOR_LIME          = 0x00FF00,
    OR_COLOR_TEAL          = 0x008080,
    OR_COLOR_NAVY          = 0x000080,
    OR_COLOR_PURPLE        = 0x800080,

    OR_COLOR_ORANGE        = 0xFFA500,
    OR_COLOR_GOLD          = 0xFFD700,
    OR_COLOR_PINK          = 0xFFC0CB,
    OR_COLOR_BROWN         = 0xA52A2A,


    /* =========================
     * Extra useful UI colors
     * ========================= */

    OR_COLOR_DARK_RED      = 0x8B0000,
    OR_COLOR_DARK_GREEN    = 0x006400,
    OR_COLOR_DARK_BLUE     = 0x00008B,

    OR_COLOR_LIGHT_RED     = 0xFF6666,
    OR_COLOR_LIGHT_GREEN   = 0x66FF66,
    OR_COLOR_LIGHT_BLUE    = 0x6666FF,

    OR_COLOR_DARK_ORANGE   = 0xCC5500,
    OR_COLOR_LIGHT_ORANGE  = 0xFFCC80,

    OR_COLOR_DARK_YELLOW   = 0xB88600,
    OR_COLOR_LIGHT_YELLOW  = 0xFFFF99,

    OR_COLOR_DARK_PURPLE   = 0x4B0082,
    OR_COLOR_LIGHT_PURPLE  = 0xCC99FF,

    OR_COLOR_DARK_CYAN     = 0x008B8B,
    OR_COLOR_LIGHT_CYAN    = 0x99FFFF,

    OR_COLOR_DARK_PINK     = 0xC71585,
    OR_COLOR_LIGHT_PINK    = 0xFFB6C1,


    /* =========================
     * UI status colors
     * ========================= */

    OR_COLOR_SUCCESS       = 0x28A745,
    OR_COLOR_WARNING       = 0xFFC107,
    OR_COLOR_ERROR         = 0xDC3545,
    OR_COLOR_INFO          = 0x17A2B8,

    OR_COLOR_FOCUS         = 0x2196F3,
    OR_COLOR_DISABLED      = 0x666666,
    OR_COLOR_SEPARATOR     = 0x444444
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
int ORgui_track_pointer(const OREvent *event);
int ORgui_event_requires_redraw(const OREvent *event);
void ORgui_update_animations(void);
int ORgui_animations_active(void);
int ORgui_pointer_x(void);
int ORgui_pointer_y(void);
void ORgui_draw(void);
void ORgui_draw_panel(int x, int y, int width, int height, uint32_t color);
void ORgui_draw_button(int x, int y, int width, int height, const char *label, int pressed);
void ORgui_draw_text(int x, int y, const char *text, uint32_t color);
