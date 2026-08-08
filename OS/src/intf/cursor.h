#pragma once

#include <stdint.h>

void cursor_init(int x, int y);
void cursor_move(int dx, int dy);
void cursor_draw(void);
int cursor_get_x(void);
int cursor_get_y(void);
