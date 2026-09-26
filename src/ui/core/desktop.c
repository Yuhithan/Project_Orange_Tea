#include "desktop.h"
#include "framebuffer.h"
#include "ORgui.h"
#include "terminal.h"
#include "keyboard.h"
#include "cursor.h"
#include "taskbar.h"
#include "desktop_apps.h"
#include "imp.h"
#include "timer.h"
#include "ui_animation.h"

#define DESKTOP_FRAME_TIME_MS 16u
#define DESKTOP_BOOT_ANIMATION_MS 320u

static int desktop_ready;
static int desktop_running;
static int mouse_irq_reported;
static uint64_t desktop_started;

void desktop_init(uint64_t multiboot_info_addr)
{
    if (multiboot_info_addr != 0) fb_init(multiboot_info_addr);
    if (!fb_is_available()) { desktop_ready = 0; return; }
    ORgui_init();
    mouse_init();
    mouse_irq_reported = 0;
    terminal_init();
    desktop_apps_init();
    desktop_started = timer_get_ticks();
    ORWindow *window = ORgui_active_window();
    if (window) {
        window->x = (fb_width() - window->width) / 2;
        window->y = (fb_height() - 28 - window->height) / 2;
        if (window->y < 20) window->y = 20;
    }
    desktop_ready = 1;
}

void desktop_draw(void)
{
    if (!desktop_ready) return;
    fb_clear(OR_COLOR_BACKGROUND);
    fb_fill_rect(0, 0, fb_width(), 34, OR_COLOR_PANEL);
    fb_draw_line(0, 33, fb_width() - 1, 33, OR_COLOR_FIRE_RED);
    if (ui_animation_running(desktop_started, DESKTOP_BOOT_ANIMATION_MS)) {
        int progress = ui_animation_progress(desktop_started, DESKTOP_BOOT_ANIMATION_MS,
                                             UI_ANIMATION_EASE_OUT);
        int width = fb_width() * progress / 1000;
        if (width > 0) fb_draw_line(0, 33, width - 1, 33, OR_COLOR_FIRE_ORANGE);
    }
    ORgui_draw_text(14, 14, "ORTOS DESKTOP - ALPHA-3.2.2", OR_COLOR_FIRE_YELLOW);
    ORgui_draw_text(190, 14, "WILDFIRE", OR_COLOR_FIRE_ORANGE);
    desktop_apps_draw();
    ORgui_draw();
    taskbar_draw();
    cursor_begin_frame();
    mouse_draw_cursor();
    /* Draw to the off-screen buffer first, then copy the final frame once to the hardware framebuffer. */
    fb_flush();
}

void desktop_run(void)
{
    if (!desktop_ready) {
        imp_text("ORgui unavailable: GRUB did not supply a compatible framebuffer.\n");
        return;
    }
    if (desktop_running) return;
    desktop_running = 1;
    desktop_draw();
    for (;;) {
        int key;
        OREvent event;
        int redraw = 0;
        int cursor_update = 0;
        ORgui_update_animations();
        desktop_apps_update_animations();
        if (!mouse_irq_reported && mouse_irq_count() != 0) {
            imp_text("PS/2 mouse IRQ12 count: ");
            imp_uint64_dec(mouse_irq_count());
            imp_text("\n");
            mouse_irq_reported = 1;
        }
        while (mouse_try_get_event(&event)) {
            int app_event = desktop_apps_handle_event(&event);
            if (!app_event || event.type == OR_EVENT_MOUSE_MOVE) ORgui_handle_event(&event);
            int pointer_changed = ORgui_track_pointer(&event);
            if (ORgui_event_requires_redraw(&event) || app_event == 2 || pointer_changed) redraw = 1;
            else cursor_update = 1;
        }
        while (keyboard_try_getchar(&key)) {
            if (key == 27 || key == 'q' || key == 'Q') {
                desktop_running = 0;
                return;
            }
            event = (OREvent){ OR_EVENT_KEY_DOWN, key, 0, 0, 0 };
            ORgui_handle_event(&event);
            redraw = 1;
        }
        if (redraw || ORgui_animations_active() || desktop_apps_animations_active() ||
            ui_animation_running(desktop_started, DESKTOP_BOOT_ANIMATION_MS)) {
            desktop_draw();
        } else if (cursor_update) {
            mouse_draw_cursor();
            fb_flush();
        } else {
            timer_sleep(DESKTOP_FRAME_TIME_MS);
        }
    }
}

int desktop_is_running(void)
{
    return desktop_running;
}
