#include "framebuffer.h"
#include "ORgui.h"
#include "desktop.h"

void gui_start(uint64_t multiboot_info_addr)
{
    if (!framebuffer_ready) {
        fb_init(0, 0, 0, 0);
        fb_init_from_multiboot(multiboot_info_addr);
    }

    if (!framebuffer_ready) {
        return;
    }

    ORgui_init();
    desktop_init();
    desktop_draw();
}
