#pragma once

#include <stdint.h>
#include <stddef.h>

/* Internal non-printable key values returned by keyboard_getchar(). */
#define KEY_SCROLL_UP 0x11
#define KEY_SCROLL_DOWN 0x12
#define KEY_PAGE_UP 0x13
#define KEY_PAGE_DOWN 0x14

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
