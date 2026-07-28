#pragma once

#ifndef DESKTOP_H
#define DESKTOP_H

#include <stdint.h>

/* Desktop colors */
#define DESKTOP_BG_COLOR 0x003080
#define TASKBAR_COLOR    0x202020

/* Desktop size */
extern int desktop_width;
extern int desktop_height;

/* Initialize desktop */
void desktop_init(int width, int height);

/* Draw the desktop */
void desktop_draw(void);

/* Draw taskbar */
void desktop_draw_taskbar(void);

#endif