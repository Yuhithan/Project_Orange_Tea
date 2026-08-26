#include "desktop.h"
#include "framebuffer.h"
#include "ORgui.h"
#include "first_app.h"
#include "keyboard.h"
#include "mouse.h"
#include "taskbar.h"
#include "imp.h"

static int desktop_ready;

void desktop_init(uint64_t multiboot_info_addr)
{
    if (multiboot_info_addr != 0) fb_init(multiboot_info_addr);
    if (!fb_is_available()) { desktop_ready = 0; return; }
    ORgui_init();
    mouse_init();
    first_app_init();
    ORWindow *window = ORgui_active_window();
    if (window) {
        window->x = (fb_width() - window->width) / 2;
        window->y = (fb_height() - 28 - window->height) / 2;
        if (window->y < 20) window->y = 20;
    }
    desktop_ready = 1;
}

void desktop_draw(void)
{
    if (!desktop_ready) return;
    fb_clear(OR_COLOR_BACKGROUND);
    fb_fill_rect(0, 0, fb_width(), 34, OR_COLOR_PANEL);
    fb_draw_line(0, 33, fb_width() - 1, 33, OR_COLOR_FIRE_RED);
    ORgui_draw_text(14, 14, "ORTOS DESKTOP", OR_COLOR_FIRE_YELLOW);
    ORgui_draw_text(150, 14, "WILDFIRE", OR_COLOR_FIRE_ORANGE);
    ORgui_draw();
    taskbar_draw();
    mouse_draw_cursor();
}

void desktop_run(void)
{
    if (!desktop_ready) {
        imp_text("ORgui unavailable: GRUB did not supply a compatible framebuffer.\n");
        return;
    }
    desktop_draw();
    for (;;) {
        int key;
        OREvent event;
        int redraw = 0;
        while (mouse_try_get_event(&event)) {
            ORgui_handle_event(&event);
            redraw = 1;
        }
        while (keyboard_try_getchar(&key)) {
            if (key == 27 || key == 'q' || key == 'Q') return;
            event = (OREvent){ OR_EVENT_KEY_DOWN, key, 0, 0, 0 };
            ORgui_handle_event(&event);
            redraw = 1;
        }
        if (redraw) desktop_draw();
    }
}
