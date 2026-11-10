#pragma once

#include <stdint.h>

void irq_exception(uint64_t vector, uint64_t error_code);
