#pragma once

#include <stdint.h>

void desktop_init(uint64_t multiboot_info_addr);
void desktop_run(void);
void desktop_draw(void);
