#include "ORgui.h"
#include "framebuffer.h"
#include "keyboard.h"

static int taskbar_height = 28;

void taskbar_draw(int width, int height)
{
    int bar_y = height - taskbar_height;
    fb_fill_rect(0, bar_y, width, taskbar_height, 0xFF2C0F0D);
    fb_draw_rect(0, bar_y, width, taskbar_height, 0xFF7A1C11);
    ORgui_draw_panel(8, bar_y + 4, 84, 20, 0xFFB23A16, 0xFFE8C34E);
    ORgui_draw_text(22, bar_y + 8, "ORgui", 0xFFF7E2C8);

    ORgui_draw_panel(width - 92, bar_y + 4, 84, 20, 0xFF2C0F0D, 0xFF7A1C11);
    ORgui_draw_text(width - 76, bar_y + 8, "08:00", 0xFFF7E2C8);
}
