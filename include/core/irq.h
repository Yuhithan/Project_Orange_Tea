#pragma once

#include <stdint.h>

void irq_init(void);
void irq_exception(uint64_t vector, uint64_t error_code);
