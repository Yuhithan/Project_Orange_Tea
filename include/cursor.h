#pragma once

#include "ORgui.h"

enum {
	CURSOR_OK = 0,
	CURSOR_ERROR_INVALID_ARGUMENT = -1,
	CURSOR_ERROR_MISSING = -2,
	CURSOR_ERROR_MALFORMED = -3,
	CURSOR_ERROR_UNSUPPORTED_FORMAT = -4,
	CURSOR_ERROR_NOMEM = -5
};

/* Initializes a standard three-button PS/2 mouse.  It is safe to call when
 * no mouse controller is present; mouse_is_available() will stay false. */
void mouse_init(void);
void mouse_handle_irq(void);
int mouse_is_available(void);
int mouse_try_get_event(OREvent *event);
int mouse_x(void);
int mouse_y(void);
int cursor_load(const char *path);
int cursor_load_data(const unsigned char *data, unsigned long size);
void cursor_destroy(void);
void cursor_set_position(int x, int y);
void cursor_draw(void);
void cursor_show(void);
void cursor_hide(void);
void cursor_begin_frame(void);
void mouse_draw_cursor(void);
