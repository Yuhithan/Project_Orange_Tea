#include "framebuffer.h"
#include "ORgui.h"
#include "desktop.h"
#include "first_app.h"
#include "imp.h"

int gui_start(uint64_t multiboot_info_addr)
{
    if (!fb_init_from_multiboot(multiboot_info_addr) || !framebuffer_ready) {
        return 0;
    }

    imp_text("BOOT 7: starting GUI\n");
    ORgui_init();
    imp_text("BOOT 8: ORgui initialized\n");
    desktop_init();
    imp_text("BOOT 9: desktop initialized\n");
    first_app_init();
    desktop_draw();
    return 1;
}
