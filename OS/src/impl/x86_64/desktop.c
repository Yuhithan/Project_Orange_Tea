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
    int usable_height = screen_height - 32;
    fb_clear(0xFF120A0A);
    /* A restrained, framebuffer-only sunset background. */
    for (int y = 0; y < usable_height; y += 4) {
        uint32_t shade = 0xFF160B0B + ((uint32_t)(y * 12 / (usable_height ? usable_height : 1)) << 8);
        fb_fill_rect(0, y, screen_width, 4, shade);
    }
    fb_fill_rect(0, 0, screen_width, 42, 0xE61D0D0D);
    fb_draw_line(0, 41, screen_width - 1, 41, 0xFFDC651A);
    ORgui_draw_text(20, 15, "ORTos  /  ORgui Desktop", 0xFFFFE9CF);

    int card_width = screen_width > 520 ? 300 : screen_width - 40;
    ORgui_draw_panel(20, 64, card_width, 112, 0xE62A1110, 0xFF8A2818);
    ORgui_draw_text(38, 84, "ORANGE TEA OS", 0xFFFFC85A);
    ORgui_draw_text(38, 106, "A bare-metal desktop session", 0xFFFFE9CF);
    ORgui_draw_text(38, 130, "Keyboard ready  |  Framebuffer ready", 0xFFD7A67F);
    ORgui_draw_text(38, 151, "Esc closes this desktop", 0xFFD7A67F);
}

void desktop_init(void)
{
    desktop_running = 1;
    ORgui_init();
    first_app_init();
}

void desktop_draw(void)
{
    while (desktop_running) {
        ORgui_begin_frame();
        desktop_draw_background();
        taskbar_draw(screen_width, screen_height);
        ORgui_draw_windows();
        first_app_draw();
        ORgui_end_frame();

        int key;
        while (keyboard_try_getchar(&key)) {
            OREvent event = { .type = OR_EVENT_KEY, .key = key, .pressed = 1 };
            ORgui_handle_event(&event);
            first_app_handle_event(&event);
            if (key == 27 || key == 'q') {
                desktop_running = 0;
                break;
            }
        }

        first_app_update();
        ORgui_update();
    }
}
