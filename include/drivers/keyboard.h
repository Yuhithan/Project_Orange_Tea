#pragma once

#include <stdint.h>
#include <stddef.h>

/* Internal non-printable key values returned by keyboard_getchar(). */
#define KEY_SCROLL_UP 0x100
#define KEY_SCROLL_DOWN 0x101
#define KEY_PAGE_UP 0x102
#define KEY_PAGE_DOWN 0x103
#define KEY_ARROW_UP 0x104
#define KEY_ARROW_DOWN 0x105
#define KEY_ARROW_LEFT 0x106
#define KEY_ARROW_RIGHT 0x107
#define KEY_HOME 0x108
#define KEY_END 0x109
#define KEY_DELETE 0x10A
#define KEY_INSERT 0x10B
#define KEY_F1 0x110
#define KEY_F2 0x111
#define KEY_F3 0x112
#define KEY_F4 0x113
#define KEY_F5 0x114
#define KEY_F6 0x115
#define KEY_F7 0x116
#define KEY_F8 0x117
#define KEY_F9 0x118
#define KEY_F10 0x119
#define KEY_F11 0x11A
#define KEY_F12 0x11B

void enable_key_input();
void disable_key_input();

// Query API
int keyboard_has_char();
int keyboard_getchar();
/* Non-blocking read. Returns 1 when a character was written to out. */
int keyboard_try_getchar(int *out);

// Layout control
void keyboard_set_layout(const char* layout);
/* Called by the PS/2 IRQ handler; polling remains available to callers. */
void keyboard_handle_irq(void);
