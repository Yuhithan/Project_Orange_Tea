#pragma once

#ifndef IRQ_H
#define IRQ_H

#include <stdint.h>

typedef void (*irq_handler_t)(uint8_t vector);

void irq_init(void);
void irq_register_handler(uint8_t vector, irq_handler_t handler);
void irq_dispatch(uint8_t vector);
void irq_exception(uint64_t vector, uint64_t error_code);
void irq0_handler(void);

#endif
