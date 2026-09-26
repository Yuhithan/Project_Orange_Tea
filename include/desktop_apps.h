#pragma once

#include "ORgui.h"

void desktop_apps_init(void);
void desktop_apps_draw(void);
void desktop_apps_update_animations(void);
int desktop_apps_animations_active(void);
int desktop_apps_draw_taskbar(int x, int y, int available_width);
int desktop_apps_handle_event(const OREvent *event);
void desktop_notify(const char *name, const char *message);
void browser_window_open(ORWindow *window);
