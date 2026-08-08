#include "desktop.h"
#include "cursor.h"
#include "framebuffer.h"
#include "keyboard.h"

static int desktop_running;

void desktop_init(void)
{
    desktop_running = 1;
    cursor_init(screen_width / 2, screen_height / 2);
}

void desktop_draw(void)
{
    while (desktop_running) {
        /* The desktop deliberately owns only the background. Applications can
         * be layered here later through ORgui without changing the cursor. */
        fb_clear(0xFF1677C8);
        cursor_draw();

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
    }
}
