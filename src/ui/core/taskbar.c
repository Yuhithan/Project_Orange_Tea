#include "taskbar.h"

#include "framebuffer.h"
#include "ORgui.h"
#include "timer.h"
#include "desktop_apps.h"

static void draw_number(int x, int y, uint64_t value)
{
    char text[12];
    int length = 0;

    do {
        text[length++] = (char)('0' + value % 10);
        value /= 10;
    } while (value && length < 11);

    for (int i = 0; i < length / 2; i++) {
        char t = text[i];
        text[i] = text[length - i - 1];
        text[length - i - 1] = t;
    }

    text[length] = '\0';

    fb_draw_string(x, y, text, OR_COLOR_TEXT);
}

void taskbar_draw(void)
{
    int height = fb_height();
    int width = fb_width();

    if (height < 28 || width < 100)
        return;

    /* Taskbar */
    fb_fill_rect(
        0,
        height - 28,
        width,
        28,
        OR_COLOR_PANEL
    );

    /* Top line */
    fb_draw_line(
        0,
        height - 28,
        width - 1,
        height - 28,
        OR_COLOR_FIRE_ORANGE
    );

    /* ORT */
    ORgui_draw_button(
        7,
        height - 23,
        58,
        18,
        "ORT",
        0
    );

    int x = desktop_apps_draw_taskbar(74, height - 23, width);
    for (int index = 0; index < ORGUI_MAX_WINDOWS; index++) {
        ORWindow *window = ORgui_window_at(index);
        if (!window || !window->visible) continue;
        fb_fill_rect(x, height - 23, 110, 18,
                     window->active ? OR_COLOR_TITLEBAR : OR_COLOR_WINDOW);
        fb_draw_rect(x, height - 23, 110, 18, OR_COLOR_BORDER);
        fb_draw_string(x + 8, height - 17, window->title,
                       window->active ? OR_COLOR_TEXT : OR_COLOR_FIRE_RED);
        x += 114;
        if (x > width - 120) break;
    }

    /* Ticks */
    fb_draw_string(
        width - 86,
        height - 17,
        "TICKS",
        OR_COLOR_TEXT
    );

    draw_number(
        width - 50,
        height - 17,
        timer_get_ticks()
    );
}