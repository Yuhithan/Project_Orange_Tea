#include "drivers/audio.h"
#include "drivers/io.h"
#include "timer.h"

#define PIT_FREQUENCY 1193182u
#define PIT_CHANNEL2  0x42
#define PIT_COMMAND   0x43
#define SPEAKER_PORT  0x61

#define PIT_CHANNEL2_SQUARE_WAVE 0xB6
#define SPEAKER_ENABLE_BITS      0x03

/*
 * Channel 2 of the PIT is dedicated to the PC speaker. It can generate
 * a square wave without changing channel 0, which the kernel timer uses.
 */

void speaker_init(void)
{
    /* Mute the speaker and stop channel 2, preserving the other 0x61 bits. */
    uint8_t control = io_inb(SPEAKER_PORT);
    io_outb(SPEAKER_PORT, control & (uint8_t)~SPEAKER_ENABLE_BITS);
}

void audio_init(void)
{
    speaker_init();
}

void audio_play_sound(uint16_t frequency, uint32_t duration_ms)
{
    if (frequency == 0) {
        audio_stop_sound();
        return;
    }

    /*
     * The PIT divides its 1,193,182 Hz input clock by this value. Its
     * channel registers are 16-bit, so clamp requests below its minimum
     * representable output frequency to the largest divisor.
     */
    uint32_t divisor = PIT_FREQUENCY / frequency;
    if (divisor > 0xFFFFu)
        divisor = 0xFFFFu;
    if (divisor == 0)
        divisor = 1;

    /* Select channel 2, low byte then high byte, mode 3 square wave. */
    io_outb(PIT_COMMAND, PIT_CHANNEL2_SQUARE_WAVE);
    io_outb(PIT_CHANNEL2, (uint8_t)(divisor & 0xFFu));
    io_outb(PIT_CHANNEL2, (uint8_t)((divisor >> 8) & 0xFFu));

    /*
     * Port 0x61 bit 0 gates the PIT channel 2 signal and bit 1 routes it
     * to the speaker. Set both while leaving the remaining control bits
     * unchanged.
     */
    uint8_t control = io_inb(SPEAKER_PORT);
    io_outb(SPEAKER_PORT, control | SPEAKER_ENABLE_BITS);

    /* Use the kernel timer rather than spinning for the requested duration. */
    timer_sleep(duration_ms);

    /* The speaker remains enabled until explicitly muted. */
    audio_stop_sound();
}

void audio_stop_sound(void)
{
    /* Clear the channel-2 gate and speaker-data bits, preserving other bits. */
    uint8_t control = io_inb(SPEAKER_PORT);
    io_outb(SPEAKER_PORT, control & (uint8_t)~SPEAKER_ENABLE_BITS);
}