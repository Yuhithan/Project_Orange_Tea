#pragma once

#include <stdint.h>

uint8_t port_inb(uint16_t port);
uint16_t port_inw(uint16_t port);
void port_outb(uint16_t port, uint8_t data);
void port_wait();