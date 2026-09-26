#pragma once

#include <stdint.h>

enum {
    UI_ANIMATION_EASE_IN,
    UI_ANIMATION_EASE_OUT
};

void ui_animation_set_reduced_motion(int enabled);
int ui_animation_reduced_motion(void);
int ui_animation_progress(uint64_t started, uint32_t duration, int easing);
int ui_animation_running(uint64_t started, uint32_t duration);
uint32_t ui_animation_mix_color(uint32_t from, uint32_t to, int progress);