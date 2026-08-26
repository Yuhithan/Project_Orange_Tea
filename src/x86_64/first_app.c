#include "first_app.h"
#include "ORgui.h"

static ORWindow *welcome_window;
static int clicked;

static void welcome_draw(ORWindow *window)
{
    int x = window->x + 18;
    int y = window->y + 39;
    ORgui_draw_text(x, y, "ORTOS GRAPHICAL ENVIRONMENT", OR_COLOR_TEXT);
    ORgui_draw_text(x, y + 18, "POWERED BY ORGUI", OR_COLOR_FIRE_YELLOW);
    ORgui_draw_text(x, y + 36, "BARE METAL DESKTOP", OR_COLOR_TEXT_DIM);
    ORgui_draw_button(x, y + 60, 94, 22, "CLICK ME", clicked);
    ORgui_draw_text(x, y + 91, clicked ? "THE FIRE IS LIT" : "PRESS ENTER", OR_COLOR_FIRE_ORANGE);
}

static void welcome_event(ORWindow *window, const OREvent *event)
{
    (void)window;
    if (event->type == OR_EVENT_KEY_DOWN && (event->key == '\n' || event->key == ' ')) clicked = !clicked;
    if (event->type == OR_EVENT_MOUSE_DOWN && event->x >= welcome_window->x + 18 &&
        event->x < welcome_window->x + 112 && event->y >= welcome_window->y + 99 &&
        event->y < welcome_window->y + 121) clicked = !clicked;
}

void first_app_init(void)
{
    welcome_window = ORgui_create_window(0, 0, 310, 155, "WELCOME TO ORTOS");
    if (welcome_window == 0) return;
    welcome_window->on_draw = welcome_draw;
    welcome_window->on_event = welcome_event;
    clicked = 0;
}
