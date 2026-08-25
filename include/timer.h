#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>

/* Initialize the PIT timer */
void timer_init(uint32_t frequency);

/* Get the number of timer ticks since initialization */
uint64_t timer_get_ticks(void);

/* Sleep for a number of milliseconds */
void timer_sleep(uint32_t milliseconds);

/* Timer interrupt handler */
void timer_handler(void);

#endif