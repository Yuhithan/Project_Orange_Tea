#ifndef TERMINAL_INPUT_RING_H
#define TERMINAL_INPUT_RING_H

#include <stddef.h>

/*
 * Small bounded ring buffer for keyboard and terminal input.
 *
 * The queue is intentionally simple and fixed-capacity: it never writes past the
 * end of the backing array, never reads an empty queue, and can be used from an
 * interrupt context as long as the caller performs the usual atomic save/restore
 * of the interrupt flag around the push/pop operations.
 */
typedef struct terminal_input_ring {
    char *buffer;
    size_t capacity;
    size_t head;
    size_t tail;
    size_t count;
} terminal_input_ring_t;

static inline void terminal_input_ring_init(terminal_input_ring_t *ring,
                                           char *buffer,
                                           size_t capacity)
{
    if (ring == NULL) {
        return;
    }

    ring->buffer = buffer;
    ring->capacity = capacity > 0 ? capacity : 1u;
    ring->head = 0;
    ring->tail = 0;
    ring->count = 0;
}

static inline int terminal_input_ring_empty(const terminal_input_ring_t *ring)
{
    return ring == NULL || ring->buffer == NULL || ring->count == 0;
}

static inline size_t terminal_input_ring_size(const terminal_input_ring_t *ring)
{
    if (ring == NULL || ring->buffer == NULL) {
        return 0;
    }
    return ring->count;
}

static inline int terminal_input_ring_push(terminal_input_ring_t *ring, char value)
{
    if (ring == NULL || ring->buffer == NULL) {
        return 0;
    }

    /* The queue is full when the head pointer would catch up to the tail.
     * This preserves one unused slot so the implementation can distinguish
     * full from empty without relying on a sentinel byte. */
    if (ring->count >= ring->capacity) {
        return 0;
    }

    ring->buffer[ring->head] = value;
    ring->head = (ring->head + 1u) % ring->capacity;
    ring->count++;
    return 1;
}

static inline int terminal_input_ring_pop(terminal_input_ring_t *ring, char *out)
{
    if (ring == NULL || ring->buffer == NULL || ring->count == 0) {
        return 0;
    }

    if (out != NULL) {
        *out = ring->buffer[ring->tail];
    }

    ring->tail = (ring->tail + 1u) % ring->capacity;
    ring->count--;
    return 1;
}

static inline int terminal_input_ring_pop_value(terminal_input_ring_t *ring)
{
    char value = 0;
    if (!terminal_input_ring_pop(ring, &value)) {
        return -1;
    }
    return (unsigned char)value;
}

#endif
