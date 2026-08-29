#include "keyboard.h"
#include "io.h"
#include "terminal_input_ring.h"
#include <stdint.h>
#include <stdbool.h>

#define KBD_DATA_PORT 0x60
#define KBD_STATUS_PORT 0x64
#define KBD_RING_SIZE 128

static char kb_buf[KBD_RING_SIZE];
static terminal_input_ring_t keyboard_ring;
static volatile int kb_enabled = 0;
static volatile int extended_scancode = 0;

static void keyboard_ring_init(void)
{
    static int initialized = 0;
    if (!initialized) {
        terminal_input_ring_init(&keyboard_ring, kb_buf, sizeof(kb_buf));
        initialized = 1;
    }
}

static const char base_map[128] = {
	[0x01] = 27,
	[0x02] = '1', [0x03] = '2', [0x04] = '3', [0x05] = '4', [0x06] = '5',
	[0x07] = '6', [0x08] = '7', [0x09] = '8', [0x0A] = '9', [0x0B] = '0',
	[0x0C] = '-', [0x0D] = '=',
	[0x10] = 'q', [0x11] = 'w', [0x12] = 'e', [0x13] = 'r', [0x14] = 't',
	[0x15] = 'y', [0x16] = 'u', [0x17] = 'i', [0x18] = 'o', [0x19] = 'p',
	[0x1A] = '[', [0x1B] = ']', [0x1C] = '\n',
	[0x1E] = 'a', [0x1F] = 's', [0x20] = 'd', [0x21] = 'f', [0x22] = 'g',
	[0x23] = 'h', [0x24] = 'j', [0x25] = 'k', [0x26] = 'l', [0x27] = ';', [0x28] = '\'', [0x29] = '`',
	[0x2B] = '\\',
	[0x2C] = 'z', [0x2D] = 'x', [0x2E] = 'c', [0x2F] = 'v', [0x30] = 'b',
	[0x31] = 'n', [0x32] = 'm', [0x33] = ',', [0x34] = '.', [0x35] = '/',
	[0x39] = ' ', [0x0E] = '\b'
};

static const char shift_map[128] = {
	[0x01] = 27,
	[0x02] = '!', [0x03] = '@', [0x04] = '#', [0x05] = '$', [0x06] = '%',
	[0x07] = '^', [0x08] = '&', [0x09] = '*', [0x0A] = '(', [0x0B] = ')',
	[0x0C] = '_', [0x0D] = '+',
	[0x10] = 'Q', [0x11] = 'W', [0x12] = 'E', [0x13] = 'R', [0x14] = 'T',
	[0x15] = 'Y', [0x16] = 'U', [0x17] = 'I', [0x18] = 'O', [0x19] = 'P',
	[0x1A] = '{', [0x1B] = '}', [0x1C] = '\n',
	[0x1E] = 'A', [0x1F] = 'S', [0x20] = 'D', [0x21] = 'F', [0x22] = 'G',
	[0x23] = 'H', [0x24] = 'J', [0x25] = 'K', [0x26] = 'L', [0x27] = ':', [0x28] = '"', [0x29] = '~',
	[0x2B] = '|',
	[0x2C] = 'Z', [0x2D] = 'X', [0x2E] = 'C', [0x2F] = 'V', [0x30] = 'B',
	[0x31] = 'N', [0x32] = 'M', [0x33] = '<', [0x34] = '>', [0x35] = '?',
	[0x39] = ' ', [0x0E] = '\b'
};

static volatile int shift_state = 0;
static volatile int caps_lock = 0;
static const char* active_layout = "en-us";

static const char fr_base_map[128] = {
	[0x01] = 27,
	[0x02] = '1', [0x03] = '2', [0x04] = '3', [0x05] = '4', [0x06] = '5',
	[0x07] = '6', [0x08] = '7', [0x09] = '8', [0x0A] = '9', [0x0B] = '0',
	[0x0C] = '-', [0x0D] = '=',
	[0x10] = 'a', [0x11] = 'z', [0x12] = 'e', [0x13] = 'r', [0x14] = 't',
	[0x15] = 'y', [0x16] = 'u', [0x17] = 'i', [0x18] = 'o', [0x19] = 'p',
	[0x1A] = '^', [0x1B] = '$', [0x1C] = '\n',
	[0x1E] = 'q', [0x1F] = 's', [0x20] = 'd', [0x21] = 'f', [0x22] = 'g',
	[0x23] = 'h', [0x24] = 'j', [0x25] = 'k', [0x26] = 'l', [0x27] = 'm', [0x28] = '*', [0x29] = '`',
	[0x2B] = '\\',
	[0x2C] = 'w', [0x2D] = 'x', [0x2E] = 'c', [0x2F] = 'v', [0x30] = 'b',
	[0x31] = 'n', [0x32] = ',', [0x33] = ';', [0x34] = '.', [0x35] = '/',
	[0x39] = ' ', [0x0E] = '\b'
};

static const char fr_shift_map[128] = {
	[0x01] = 27,
	[0x02] = '!', [0x03] = '@', [0x04] = '#', [0x05] = '$', [0x06] = '%',
	[0x07] = '^', [0x08] = '&', [0x09] = '*', [0x0A] = '(', [0x0B] = ')',
	[0x0C] = '_', [0x0D] = '+',
	[0x10] = 'A', [0x11] = 'Z', [0x12] = 'E', [0x13] = 'R', [0x14] = 'T',
	[0x15] = 'Y', [0x16] = 'U', [0x17] = 'I', [0x18] = 'O', [0x19] = 'P',
	[0x1A] = 0x22, [0x1B] = 0x7C, [0x1C] = '\n',
	[0x1E] = 'Q', [0x1F] = 'S', [0x20] = 'D', [0x21] = 'F', [0x22] = 'G',
	[0x23] = 'H', [0x24] = 'J', [0x25] = 'K', [0x26] = 'L', [0x27] = 'M', [0x28] = 0x7E, [0x29] = 0x7E,
	[0x2B] = '|',
	[0x2C] = 'W', [0x2D] = 'X', [0x2E] = 'C', [0x2F] = 'V', [0x30] = 'B',
	[0x31] = 'N', [0x32] = '?', [0x33] = '.', [0x34] = '/', [0x35] = '\\',
	[0x39] = ' ', [0x0E] = '\b'
};

static inline const char* layout_base_map(void) {
	if (active_layout[0] == 'f' && active_layout[1] == 'r') {
		return fr_base_map;
	}
	return base_map;
}

static inline const char* layout_shift_map(void) {
	if (active_layout[0] == 'f' && active_layout[1] == 'r') {
		return fr_shift_map;
	}
	return shift_map;
}

static inline char scancode_to_ascii(uint8_t sc) {
	if (sc >= 128) return 0;
	char c = 0;
	if (shift_state)
		c = layout_shift_map()[sc];
	else
		c = layout_base_map()[sc];

	// If letter, apply caps lock (XOR shift)
	if (c >= 'a' && c <= 'z') {
		if (caps_lock && !shift_state) c = c - 'a' + 'A';
	} else if (c >= 'A' && c <= 'Z') {
		if (caps_lock && shift_state) c = c - 'A' + 'a';
	}

	return c;
}

static void kb_push(char c) {
    keyboard_ring_init();
    /*
     * Keep the interrupt path tiny: simply enqueue the scancode-derived byte if
     * there is room. Any terminal redraw or shell processing happens later in the
     * non-interrupt path, so the IRQ remains bounded and does not touch the
     * terminal window or rendering state.
     */
    if (!terminal_input_ring_push(&keyboard_ring, c)) {
        /* Drop the newest character instead of corrupting the ring or crashing.
         * This preserves the rest of the queue and prevents a burst of keys from
         * overwriting active input. */
    }
}

/* Polling is intentional: ORTos has no IRQ dispatcher yet. Keep all PS/2
 * decoding in one place so shell and ORgui observe identical input. */
static void keyboard_poll(void) {
    if (!kb_enabled) return;

    uint8_t status = io_inb(KBD_STATUS_PORT);
    if (!(status & 1)) return;
    /* Bit 5 means the waiting byte is from the mouse, not the keyboard. */
    if (status & 0x20) return;

    uint8_t sc = io_inb(KBD_DATA_PORT);

	if (sc == 0xE0) {
		extended_scancode = 1;
		return;
	}

    int released = sc & 0x80;
    uint8_t code = sc & 0x7F;

	if (extended_scancode) {
		extended_scancode = 0;
		if (!released) {
			if (code == 0x48) kb_push(KEY_SCROLL_UP);
			else if (code == 0x50) kb_push(KEY_SCROLL_DOWN);
			else if (code == 0x49) kb_push(KEY_PAGE_UP);
			else if (code == 0x51) kb_push(KEY_PAGE_DOWN);
		}
		return;
	}

    if (code == 0x2A || code == 0x36) {
        shift_state = released ? 0 : 1;
        return;
    }
    if (code == 0x3A && !released) {
        caps_lock = !caps_lock;
        return;
    }
    if (!released) {
        char c = scancode_to_ascii(code);
        if (c) kb_push(c);
    }
}

void keyboard_handle_irq(void)
{
    keyboard_poll();
}

void enable_key_input() {
    keyboard_ring_init();
	kb_enabled = 1;
}

void disable_key_input() {
	kb_enabled = 0;
}

void keyboard_set_layout(const char* layout) {
	if (layout != NULL && layout[0] == 'f' && layout[1] == 'r') {
		active_layout = "fr";
	} else {
		active_layout = "en-us";
	}
}

int keyboard_has_char() {
    keyboard_ring_init();
    return !terminal_input_ring_empty(&keyboard_ring);
}

int keyboard_try_getchar(int *out) {
    unsigned long flags;
    int available;

    keyboard_ring_init();
    __asm__ volatile ("pushfq; popq %0; cli" : "=r"(flags) : : "memory");
    available = !terminal_input_ring_empty(&keyboard_ring);
    if (available) {
        char value = 0;
        if (!terminal_input_ring_pop(&keyboard_ring, &value)) {
            available = 0;
        } else if (out != NULL) {
            *out = (unsigned char)value;
        }
    }
    __asm__ volatile ("pushq %0; popfq" : : "r"(flags) : "memory");
    return available;
}

int keyboard_getchar() {
    /* The IRQ handler owns PS/2 reads; this function only waits for queued data. */
    int character;
    while (!keyboard_try_getchar(&character)) {
        __asm__ volatile ("hlt");
    }
    return character;
}
