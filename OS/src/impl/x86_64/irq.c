#include "irq.h"
#include "timer.h"
#include <stdint.h>

static inline void outb(uint16_t port, uint8_t value)
{
    __asm__ volatile (
        "outb %0, %1"
        :
        : "a"(value), "Nd"(port)
    );
}

void irq0_handler(void)
{
    /* Tell the timer that one tick occurred */
    timer_handler();

    /* Send End Of Interrupt to the master PIC */
    outb(0x20, 0x20);
}