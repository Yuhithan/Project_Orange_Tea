#pragma once

#include <stdint.h>
#include <stddef.h>

void enable_key_input();
void disable_key_input();

// Query API
int keyboard_has_char();
int keyboard_getchar();
/* Non-blocking read. Returns 1 when a character was written to out. */
int keyboard_try_getchar(int *out);

// Layout control
void keyboard_set_layout(const char* layout);
