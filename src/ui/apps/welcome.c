#include "ORgui.h"
#include "framebuffer.h"
#include "terminal.h"
#include "welcome.h"

#define WELCOME_WINDOW_WIDTH 480
#define WELCOME_WINDOW_HEIGHT 230
#define TERMINAL_BUTTON_X 30
#define TERMINAL_BUTTON_Y 180
#define TERMINAL_BUTTON_WIDTH 130
#define TERMINAL_BUTTON_HEIGHT 24

static void welcome_draw(ORWindow *window)
{
    ORgui_draw_panel(window->x + 14, window->y + 34,
                     window->width - 28, window->height - 48,
                     OR_COLOR_CYAN);
    ORgui_draw_text(window->x + 30, window->y + 52,
                    "WELCOME TO ORANGE TEA", OR_COLOR_FIRE_YELLOW);
    ORgui_draw_text(window->x + 30, window->y + 84,
                    "Your desktop is ready.", OR_COLOR_TEXT);
    ORgui_draw_text(window->x + 30, window->y + 108,
                    "Open Applications from the taskbar to get started.",
                    OR_COLOR_TEXT);
    ORgui_draw_text(window->x + 30, window->y + 140,
                    "Terminal is ready to use.", OR_COLOR_LIGHT_ORANGE);
    ORgui_draw_text(window->x + 30, window->y + 160,
                    "Drag the title bar to move; use X to close.",
                    OR_COLOR_LIGHT_ORANGE);
    ORgui_draw_button(window->x + TERMINAL_BUTTON_X,
                      window->y + TERMINAL_BUTTON_Y,
                      TERMINAL_BUTTON_WIDTH, TERMINAL_BUTTON_HEIGHT,
                      "Open Terminal", 0);
}

static void welcome_event(ORWindow *window, const OREvent *event)
{
    int button_x = window->x + TERMINAL_BUTTON_X;
    int button_y = window->y + TERMINAL_BUTTON_Y;
    if (event->type == OR_EVENT_MOUSE_DOWN && event->button == 1 &&
        event->x >= button_x && event->x < button_x + TERMINAL_BUTTON_WIDTH &&
        event->y >= button_y && event->y < button_y + TERMINAL_BUTTON_HEIGHT)
        terminal_open();
}

ORWindow *welcome_window_open(void)
{
    int x = (fb_width() - WELCOME_WINDOW_WIDTH) / 2;
    int y = (fb_height() - 28 - WELCOME_WINDOW_HEIGHT) / 2;
    if (y < 20) y = 20;

    ORWindow *window = ORgui_create_window(
        x, y, WELCOME_WINDOW_WIDTH, WELCOME_WINDOW_HEIGHT, "Welcome");
    if (window) {
        window->on_draw = welcome_draw;
        window->on_event = welcome_event;
    }
    return window;
}