#include "desktop.h"
#include "framebuffer.h"
#include "ORgui.h"
#include "keyboard.h"
#include <stddef.h>

extern void taskbar_draw(int width, int height);
extern void first_app_init(void);
extern void first_app_update(void);
extern void first_app_draw(void);
extern void first_app_handle_event(const OREvent *event);

static int desktop_running = 1;

static void desktop_draw_background(void)
{
    fb_clear(0xFF140B0B);
    fb_fill_rect(0, 0, screen_width, screen_height - 32, 0xFF140B0B);
    fb_fill_rect(20, 24, screen_width - 40, screen_height - 80, 0xFF2C0F0D);
    fb_draw_rect(20, 24, screen_width - 40, screen_height - 80, 0xFF7A1C11);
    fb_fill_rect(48, 48, 96, 64, 0xFFB8201C);
    fb_fill_rect(80, 64, 64, 32, 0xFFD95A12);
    fb_fill_rect(96, 80, 32, 16, 0xFFE8C34E);
}

void desktop_init(void)
{
    ORgui_init();
    first_app_init();
}

void desktop_draw(void)
{
    desktop_init();
    while (desktop_running) {
        ORgui_begin_frame();
        desktop_draw_background();
        taskbar_draw(screen_width, screen_height);
        first_app_draw();
        ORgui_draw_windows();
        ORgui_end_frame();

        if (keyboard_has_char()) {
            int key = keyboard_getchar();
            if (key == 27) {
                desktop_running = 0;
                break;
            }
        }

        first_app_update();
        ORgui_update();
    }
}