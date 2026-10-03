#pragma once

#include <stdint.h>
#include "drivers/audio.h"

void audioreader_init(void);

void audioreader_read_mp4(const char* filename, uint8_t** audio_data, uint32_t* audio_size, uint32_t* sample_rate, uint16_t* channels);