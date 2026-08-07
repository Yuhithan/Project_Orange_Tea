#include "framebuffer.h"
#include "ORgui.h"
#include "desktop.h"

void gui_start(void)
{
    fb_init(0, 0, 0, 0);
    fb_init_from_multiboot(0);

    if (!framebuffer_ready) {
        return;
    }

    ORgui_init();
    desktop_init();
    desktop_draw();
}
