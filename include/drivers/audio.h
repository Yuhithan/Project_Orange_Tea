#pragma once

#include <stdint.h>

void speaker_init(void);
void audio_init(void);
void audio_play_sound(uint16_t frequency, uint32_t duration_ms);
void audio_stop_sound(void);