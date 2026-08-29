#include "desktop.h"
#include "framebuffer.h"
#include "ORgui.h"
#include "terminal.h"
#include "keyboard.h"
#include "cursor.h"
#include "taskbar.h"
#include "imp.h"

static int desktop_ready;
static int desktop_running;

void desktop_init(uint64_t multiboot_info_addr)
{
    if (multiboot_info_addr != 0) fb_init(multiboot_info_addr);
    if (!fb_is_available()) { desktop_ready = 0; return; }
    ORgui_init();
    mouse_init();
    terminal_init();
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
    cursor_begin_frame();
    fb_clear(OR_COLOR_BACKGROUND);
    fb_fill_rect(0, 0, fb_width(), 34, OR_COLOR_PANEL);
    fb_draw_line(0, 33, fb_width() - 1, 33, OR_COLOR_FIRE_RED);
    ORgui_draw_text(14, 14, "ORTOS DESKTOP - ALPHA-3.0.0", OR_COLOR_FIRE_YELLOW);
    ORgui_draw_text(190, 14, "WILDFIRE", OR_COLOR_FIRE_ORANGE);
    ORgui_draw();
    taskbar_draw();
    mouse_draw_cursor();
    /* Draw to the off-screen buffer first, then copy the final frame once to the hardware framebuffer. */
    fb_flush();
}

void desktop_run(void)
{
    if (!desktop_ready) {
        imp_text("ORgui unavailable: GRUB did not supply a compatible framebuffer.\n");
        return;
    }
    if (desktop_running) return;
    desktop_running = 1;
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
            if (key == 27 || key == 'q' || key == 'Q') {
                desktop_running = 0;
                return;
            }
            event = (OREvent){ OR_EVENT_KEY_DOWN, key, 0, 0, 0 };
            ORgui_handle_event(&event);
            redraw = 1;
        }
        if (redraw) desktop_draw();
    }
}

int desktop_is_running(void)
{
    return desktop_running;
}
