#include "desktop.h"
#include "framebuffer.h"   // Your framebuffer drawing functions

int desktop_width = 0;
int desktop_height = 0;

void desktop_init(int width, int height)
{
    desktop_width = width;
    desktop_height = height;
}

void desktop_draw(void)
{
    /* Fill background */
    fb_fill_rect(
        0,
        0,
        desktop_width,
        desktop_height,
        DESKTOP_BG_COLOR
    );

    /* Draw taskbar */
    desktop_draw_taskbar();
}

void desktop_draw_taskbar(void)
{
    fb_fill_rect(
        0,
        desktop_height - 30,
        desktop_width,
        30,
        TASKBAR_COLOR
    );
}