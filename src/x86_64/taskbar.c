#include "taskbar.h"

#include "framebuffer.h"
#include "ORgui.h"
#include "timer.h"

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

    fb_draw_string(x, y, text, OR_COLOR_TEXT_DIM);
}

static void taskbar_app_button(int x, int y, const char *name, int active)
{
    int width = 78;
    int height = 20;

    uint32_t bg = active ? OR_COLOR_WINDOW : OR_COLOR_PANEL;

    fb_fill_rect(x, y, width, height, bg);
    fb_draw_rect(x, y, width, height, OR_COLOR_BORDER);

    /* Small application icon */
    fb_fill_rect(x + 6, y + 6, 7, 7,
                 active ? OR_COLOR_FIRE_ORANGE : OR_COLOR_TEXT_DIM);

    fb_draw_string(x + 18, y + 6, name,
                   active ? OR_COLOR_TEXT : OR_COLOR_TEXT_DIM);
}

void taskbar_draw(void)
{
    int height = fb_height();
    int width = fb_width();

    if (height < 28 || width < 100)
        return;

    int y = height - 28;

    /* Main taskbar */
    fb_fill_rect(0, y, width, 28, OR_COLOR_PANEL);

    /* Top highlight */
    fb_draw_line(
        0,
        y,
        width - 1,
        y,
        OR_COLOR_FIRE_ORANGE
    );

    /* Start button */
    ORgui_draw_button(
        7,
        height - 23,
        58,
        18,
        "ORT",
        0
    );

    /*
     * Pinned/demo applications
     *
     * Notepad | System | Terminal
     */
    taskbar_app_button(72,  height - 24, "Notepad", 0);
    taskbar_app_button(153, height - 24, "System",  0);
    taskbar_app_button(234, height - 24, "Terminal", 0);

    /*
     * Active window
     */
    ORWindow *active = ORgui_active_window();

    if (active && active->visible) {
        int x = 320;
        int w = 150;

        fb_fill_rect(
            x,
            height - 24,
            w,
            20,
            OR_COLOR_WINDOW
        );

        fb_draw_rect(
            x,
            height - 24,
            w,
            20,
            OR_COLOR_BORDER
        );

        fb_draw_string(
            x + 7,
            height - 17,
            active->title,
            OR_COLOR_TEXT
        );
    }

    /*
     * System status area
     */
    fb_draw_string(
        width - 86,
        height - 17,
        "TICKS",
        OR_COLOR_TEXT_DIM
    );

    draw_number(
        width - 50,
        height - 17,
        timer_get_ticks()
    );
}