#include "ORgui.h"
#include "framebuffer.h"

static ORWindow *first_window = NULL;
static ORButton button;
static int button_pressed = 0;

void first_app_init(void)
{
    first_window = ORgui_create_window("First App", 80, 60, 280, 180);
    if (first_window == NULL) {
        return;
    }

    button.x = 96;
    button.y = 132;
    button.width = 120;
    button.height = 28;
    button.background = 0xFFB23A16;
    button.border = 0xFFE8C34E;
    button.text_color = 0xFFF7E2C8;
    button.text = "Press Me";
    button.visible = 1;
    button.pressed = 0;
}

void first_app_update(void)
{
    if (first_window == NULL) {
        return;
    }
    if (first_window->close_requested) {
        ORgui_destroy_window(first_window);
        first_window = NULL;
    }
}

void first_app_draw(void)
{
    if (first_window == NULL) {
        return;
    }

    ORgui_draw_window(first_window);
    ORgui_draw_panel(first_window->x + 18, first_window->y + 42, first_window->width - 36, 84, 0xFF1A0B0A, 0xFF7A1C11);
    ORgui_draw_text(first_window->x + 24, first_window->y + 56, "Welcome to ORgui", 0xFFF7E2C8);
    ORgui_draw_text(first_window->x + 24, first_window->y + 74, "This is a real framebuffer window.", 0xFFB69374);
    ORgui_draw_button(first_window->x + 24, first_window->y + 104, 120, 28, button.text, button.background, button.pressed);
}

void first_app_handle_event(const OREvent *event)
{
    if (first_window == NULL || event == NULL) {
        return;
    }

    if (event->type == OR_EVENT_MOUSE_DOWN) {
        int x = event->mouse_x;
        int y = event->mouse_y;
        if (x >= first_window->x + 24 && x < first_window->x + 144 && y >= first_window->y + 104 && y < first_window->y + 132) {
            button.pressed = 1;
            ORgui_select_window(first_window);
        }
    } else if (event->type == OR_EVENT_MOUSE_UP) {
        button.pressed = 0;
    }
}
