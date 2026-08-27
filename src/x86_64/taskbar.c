#include "taskbar.h"

#include "framebuffer.h"
#include "ORgui.h"
#include "timer.h"


/* ---------------------------------------------------------
 * Draw a number
 * --------------------------------------------------------- */
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

    fb_draw_string(
        x,
        y,
        text,
        OR_COLOR_TEXT
    );
}


/* ---------------------------------------------------------
 * Application button
 * --------------------------------------------------------- */
static void taskbar_app_button(
    int x,
    int y,
    int width,
    const char *name
)
{
    /*
     * Panel
     */
    fb_fill_rect(
        x,
        y,
        width,
        18,
        OR_COLOR_WINDOW
    );

    /*
     * Border
     */
    fb_draw_rect(
        x,
        y,
        width,
        18,
        OR_COLOR_BORDER
    );

    /*
     * Small icon
     */
    fb_fill_rect(
        x + 5,
        y + 5,
        7,
        7,
        OR_COLOR_FIRE_ORANGE
    );

    /*
     * TEXT
     *
     * This uses OR_COLOR_TEXT instead of
     * OR_COLOR_TEXT_DIM so it is clearly visible.
     */
    fb_draw_string(
        x + 17,
        y + 12,
        name,
        OR_COLOR_TEXT
    );
}


/* ---------------------------------------------------------
 * Taskbar
 * --------------------------------------------------------- */
void taskbar_draw(void)
{
    int height = fb_height();
    int width = fb_width();

    if (height < 28 || width < 100)
        return;


    int y = height - 28;


    /* -----------------------------------------------------
     * Background
     * ----------------------------------------------------- */

    fb_fill_rect(
        0,
        y,
        width,
        28,
        OR_COLOR_PANEL
    );


    /* -----------------------------------------------------
     * Top border
     * ----------------------------------------------------- */

    fb_draw_line(
        0,
        y,
        width - 1,
        y,
        OR_COLOR_FIRE_ORANGE
    );


    /* -----------------------------------------------------
     * ORT
     * ----------------------------------------------------- */

    ORgui_draw_button(
        7,
        y + 5,
        58,
        18,
        "ORT",
        0
    );


    /* -----------------------------------------------------
     * Notepad
     * ----------------------------------------------------- */

    taskbar_app_button(
        72,
        y + 5,
        78,
        "Notepad"
    );


    /* -----------------------------------------------------
     * System
     * ----------------------------------------------------- */

    taskbar_app_button(
        155,
        y + 5,
        70,
        "System"
    );


    /* -----------------------------------------------------
     * Terminal
     * ----------------------------------------------------- */

    taskbar_app_button(
        230,
        y + 5,
        82,
        "Terminal"
    );


    /* -----------------------------------------------------
     * Active window
     * ----------------------------------------------------- */

    ORWindow *active = ORgui_active_window();

    if (active && active->visible) {

        int active_x = 320;
        int active_y = y + 5;

        fb_fill_rect(
            active_x,
            active_y,
            150,
            18,
            OR_COLOR_WINDOW
        );

        fb_draw_rect(
            active_x,
            active_y,
            150,
            18,
            OR_COLOR_BORDER
        );

        fb_draw_string(
            active_x + 7,
            active_y + 12,
            active->title,
            OR_COLOR_TEXT
        );
    }


    /* -----------------------------------------------------
     * TICKS
     * ----------------------------------------------------- */

    fb_draw_string(
        width - 86,
        y + 12,
        "TICKS",
        OR_COLOR_TEXT
    );


    draw_number(
        width - 50,
        y + 12,
        timer_get_ticks()
    );
}