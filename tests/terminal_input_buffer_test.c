#include <assert.h>
#include <stdint.h>

#include "terminal_input_ring.h"

int main(void)
{
    char storage[8];
    terminal_input_ring_t ring;
    terminal_input_ring_init(&ring, storage, sizeof(storage));

    assert(terminal_input_ring_empty(&ring));
    assert(terminal_input_ring_size(&ring) == 0);

    for (int i = 0; i < 7; ++i)
    {
        assert(terminal_input_ring_push(&ring, (char)('a' + i)) == 1);
    }

    assert(terminal_input_ring_size(&ring) == 7);
    assert(!terminal_input_ring_empty(&ring));

    for (int i = 0; i < 7; ++i)
    {
        char value = 0;
        assert(terminal_input_ring_pop(&ring, &value));
        assert(value == (char)('a' + i));
    }

    assert(terminal_input_ring_empty(&ring));

    for (int i = 0; i < 8; ++i)
    {
        assert(terminal_input_ring_push(&ring, (char)('A' + i)) == 1);
    }

    assert(terminal_input_ring_push(&ring, 'X') == 0);
    assert(terminal_input_ring_size(&ring) == 8);

    for (int i = 0; i < 8; ++i)
    {
        char value = 0;
        assert(terminal_input_ring_pop(&ring, &value));
        assert(value == (char)('A' + i));
    }

    assert(terminal_input_ring_empty(&ring));
    assert(terminal_input_ring_pop(&ring, &(char){0}) == 0);

    return 0;
}
