#pragma once

#include <stdint.h>
#include <stddef.h>

void enable_key_input();
void disable_key_input();

// Query API
int keyboard_has_char();
int keyboard_getchar();

// Layout control
void keyboard_set_layout(const char* layout);