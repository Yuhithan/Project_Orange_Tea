#include "desktop.h"
#include "cursor.h"
#include "framebuffer.h"
#include "keyboard.h"
#include "ORgui.h"
#include "taskbar.h"
#include "first_app.h"

static int desktop_running;

void desktop_init(void)
{
    desktop_running = 1;
    cursor_init(screen_width / 2, screen_height / 2);
}

void desktop_draw(void)
{
    while (desktop_running) {
        ORgui_begin_frame();
        fb_clear(0xFF1677C8);
        taskbar_draw(screen_width, screen_height);
        ORgui_draw_windows();
        first_app_draw();
        cursor_draw();
        ORgui_end_frame();

        int key;
        while (keyboard_try_getchar(&key)) {
            if (key == 27 || key == 'q') {
                desktop_running = 0;
            } else if (key == 'w') {
                cursor_move(0, -8);
            } else if (key == 'a') {
                cursor_move(-8, 0);
            } else if (key == 's') {
                cursor_move(0, 8);
            } else if (key == 'd') {
                cursor_move(8, 0);
            }
        }

        ORgui_update();
        first_app_update();
    }
}
