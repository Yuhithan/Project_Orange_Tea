#include "desktop.h"
#include "framebuffer.h"

#define WILDFIRE 0xFF5A1F

void desktop_draw(void)
{
    while (1)
    {
        fb_clear(0xFF5A1F);   // Wildfire background

        //cursor_draw();

        // Update mouse here later

        // Exit back to shell (optional)
        // if (keyboard_pressed(KEY_ESC))
        //     break;
    }
}