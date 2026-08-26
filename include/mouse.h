#pragma once

#include "ORgui.h"

/* Initializes a standard three-button PS/2 mouse.  It is safe to call when
 * no mouse controller is present; mouse_is_available() will stay false. */
void mouse_init(void);
void mouse_handle_irq(void);
int mouse_is_available(void);
int mouse_try_get_event(OREvent *event);
int mouse_x(void);
int mouse_y(void);
void mouse_draw_cursor(void);
