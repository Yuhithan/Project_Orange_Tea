#pragma once

void desktop_init(void);
void desktop_render(void);
void mouse_render(void);
void mouse_update_position(int x, int y);
void mouse_update_button_state(int button, int pressed);
void mouse_update_wheel(int delta);
void taskbar_render(void);
void deskop_enter(void);