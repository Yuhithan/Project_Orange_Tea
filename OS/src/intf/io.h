#pragma once

#include <stdint.h>

/* Shared x86 port-I/O helpers used by drivers and the reboot path. */
static inline uint8_t io_inb(uint16_t port)
{
    uint8_t value;
    asm volatile ("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static inline void io_outb(uint16_t port, uint8_t value)
{
    asm volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}
