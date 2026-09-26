#include "ui_animation.h"

#include "timer.h"

#ifndef ORGUI_REDUCED_MOTION
#define ORGUI_REDUCED_MOTION 0
#endif

static int reduced_motion = ORGUI_REDUCED_MOTION;

void ui_animation_set_reduced_motion(int enabled)
{
    reduced_motion = enabled != 0;
}

int ui_animation_reduced_motion(void)
{
    return reduced_motion;
}

int ui_animation_progress(uint64_t started, uint32_t duration, int easing)
{
    if (reduced_motion || duration == 0) return 1000;
    uint64_t elapsed = timer_get_ticks() - started;
    if (elapsed >= duration) return 1000;

    uint64_t progress = elapsed * 1000 / duration;
    if (easing == UI_ANIMATION_EASE_IN)
        return (int)(progress * progress * progress / 1000000);

    uint64_t remaining = 1000 - progress;
    return (int)(1000 - remaining * remaining * remaining / 1000000);
}

int ui_animation_running(uint64_t started, uint32_t duration)
{
    return !reduced_motion && duration != 0 &&
           timer_get_ticks() - started < duration;
}

uint32_t ui_animation_mix_color(uint32_t from, uint32_t to, int progress)
{
    if (progress < 0) progress = 0;
    if (progress > 1000) progress = 1000;

    uint32_t red = (((from >> 16) & 255u) * (1000 - progress) +
                    ((to >> 16) & 255u) * progress) / 1000;
    uint32_t green = (((from >> 8) & 255u) * (1000 - progress) +
                      ((to >> 8) & 255u) * progress) / 1000;
    uint32_t blue = ((from & 255u) * (1000 - progress) +
                     (to & 255u) * progress) / 1000;
    return (red << 16) | (green << 8) | blue;
}