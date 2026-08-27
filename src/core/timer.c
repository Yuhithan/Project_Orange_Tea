#include "timer.h"
#include <stdint.h>

/*
 * Intel 8253/8254 PIT
 *
 * PIT input frequency:
 *     1193182 Hz
 */

#define PIT_FREQUENCY 1193182

#define PIT_CHANNEL0  0x40
#define PIT_COMMAND   0x43

#define PIT_COMMAND_CHANNEL0  0x00
#define PIT_COMMAND_LOHI     0x30
#define PIT_COMMAND_MODE2    0x04

static volatile uint64_t timer_ticks = 0;
static uint32_t timer_frequency = 1000;

/*
 * Write one byte to an I/O port.
 */
static inline void outb(uint16_t port, uint8_t value)
{
    __asm__ volatile (
        "outb %0, %1"
        :
        : "a"(value), "Nd"(port)
    );
}

/*
 * Read one byte from an I/O port.
 */
static inline uint8_t inb(uint16_t port)
{
    uint8_t value;

    __asm__ volatile (
        "inb %1, %0"
        : "=a"(value)
        : "Nd"(port)
    );

    return value;
}

void timer_init(uint32_t frequency)
{
    if (frequency == 0)
        frequency = 1000;

    timer_frequency = frequency;

    uint32_t divisor = PIT_FREQUENCY / frequency;

    if (divisor > 65535)
        divisor = 65535;

    if (divisor < 1)
        divisor = 1;

    /*
     * Channel 0
     * Access mode: low byte / high byte
     * Mode 2: rate generator
     * Binary mode
     */
    outb(PIT_COMMAND,
         PIT_COMMAND_CHANNEL0 |
         PIT_COMMAND_LOHI |
         PIT_COMMAND_MODE2);

    /* Send divisor */
    outb(PIT_CHANNEL0, divisor & 0xFF);
    outb(PIT_CHANNEL0, (divisor >> 8) & 0xFF);

    timer_ticks = 0;
}

void timer_handler(void)
{
    timer_ticks++;
}

uint64_t timer_get_ticks(void)
{
    return timer_ticks;
}

void timer_sleep(uint32_t milliseconds)
{
    /*
     * Convert milliseconds to timer ticks.
     */
    uint64_t ticks_to_wait =
        ((uint64_t)milliseconds * timer_frequency) / 1000;

    uint64_t start = timer_ticks;

    while ((timer_ticks - start) < ticks_to_wait) {
        __asm__ volatile ("hlt");
    }
}