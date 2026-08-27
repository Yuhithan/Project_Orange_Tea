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
        OR_COLOR_TEXT_DIM
    );
}


/* ---------------------------------------------------------
 * Draw a taskbar application button
 * --------------------------------------------------------- */
static void taskbar_app_button(
    int x,
    int y,
    const char *name,
    int active
)
{
    int width = 78;
    int height = 20;

    /*
     * Button background
     */
    fb_fill_rect(
        x,
        y,
        width,
        height,
        active ? OR_COLOR_WINDOW : OR_COLOR_PANEL
    );

    /*
     * Button border
     */
    fb_draw_rect(
        x,
        y,
        width,
        height,
        OR_COLOR_BORDER
    );

    /*
     * Small application icon
     */
    fb_fill_rect(
        x + 5,
        y + 6,
        7,
        7,
        OR_COLOR_FIRE_ORANGE
    );

    /*
     * Application name
     *
     * y + 7 keeps the text inside the 20px button.
     */
    fb_draw_string(
        x + 17,
        y + 7,
        name,
        OR_COLOR_TEXT_DIM
    );
}


/* ---------------------------------------------------------
 * Draw taskbar
 * --------------------------------------------------------- */
void taskbar_draw(void)
{
    int height = fb_height();
    int width = fb_width();

    /*
     * Don't draw if the screen is too small.
     */
    if (height < 28 || width < 100)
        return;

    int y = height - 28;


    /* -----------------------------------------------------
     * Main taskbar
     * ----------------------------------------------------- */

    fb_fill_rect(
        0,
        y,
        width,
        28,
        OR_COLOR_PANEL
    );


    /* -----------------------------------------------------
     * Orange top line
     * ----------------------------------------------------- */

    fb_draw_line(
        0,
        y,
        width - 1,
        y,
        OR_COLOR_FIRE_ORANGE
    );


    /* -----------------------------------------------------
     * ORT / Start button
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
     * Pinned applications
     * ----------------------------------------------------- */

    /*
     * Notepad
     */
    taskbar_app_button(
        72,
        y + 4,
        "Notepad",
        0
    );


    /*
     * System
     */
    taskbar_app_button(
        153,
        y + 4,
        "System",
        0
    );


    /*
     * Terminal
     */
    taskbar_app_button(
        234,
        y + 4,
        "Terminal",
        0
    );


    /* -----------------------------------------------------
     * Active window
     * ----------------------------------------------------- */

    ORWindow *active = ORgui_active_window();

    if (active && active->visible) {

        int active_x = 320;
        int active_y = y + 4;
        int active_width = 150;
        int active_height = 20;


        /*
         * Active window background
         */
        fb_fill_rect(
            active_x,
            active_y,
            active_width,
            active_height,
            OR_COLOR_WINDOW
        );


        /*
         * Active window border
         */
        fb_draw_rect(
            active_x,
            active_y,
            active_width,
            active_height,
            OR_COLOR_BORDER
        );


        /*
         * Active window title
         */
        fb_draw_string(
            active_x + 7,
            active_y + 7,
            active->title,
            OR_COLOR_TEXT_DIM
        );
    }


    /* -----------------------------------------------------
     * System information
     * ----------------------------------------------------- */

    fb_draw_string(
        width - 86,
        y + 7,
        "TICKS",
        OR_COLOR_TEXT_DIM
    );


    draw_number(
        width - 50,
        y + 7,
        timer_get_ticks()
    );
}