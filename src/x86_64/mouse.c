#include "mouse.h"
#include "framebuffer.h"
#include "io.h"

#define PS2_DATA_PORT    0x60
#define PS2_STATUS_PORT  0x64
#define PS2_COMMAND_PORT 0x64
#define PS2_STATUS_OUTPUT_FULL 0x01
#define PS2_STATUS_INPUT_FULL  0x02
#define PS2_STATUS_AUX_DATA    0x20

#define MOUSE_QUEUE_SIZE 32

static volatile int mouse_available;
static volatile int packet_index;
static volatile uint8_t packet[3];
static volatile int cursor_x;
static volatile int cursor_y;
static volatile uint8_t buttons;
static volatile int event_head;
static volatile int event_tail;
static OREvent event_queue[MOUSE_QUEUE_SIZE];

static int wait_for_input_empty(void)
{
    for (int i = 0; i < 100000; i++)
        if (!(io_inb(PS2_STATUS_PORT) & PS2_STATUS_INPUT_FULL)) return 1;
    return 0;
}

static int wait_for_output_full(void)
{
    for (int i = 0; i < 100000; i++)
        if (io_inb(PS2_STATUS_PORT) & PS2_STATUS_OUTPUT_FULL) return 1;
    return 0;
}

static int write_command(uint8_t command)
{
    if (!wait_for_input_empty()) return 0;
    io_outb(PS2_COMMAND_PORT, command);
    return 1;
}

static int write_mouse(uint8_t command)
{
    if (!write_command(0xD4) || !wait_for_input_empty()) return 0;
    io_outb(PS2_DATA_PORT, command);
    if (!wait_for_output_full()) return 0;
    return io_inb(PS2_DATA_PORT) == 0xFA;
}

static void queue_event(OREventType type, int button)
{
    int next = (event_head + 1) % MOUSE_QUEUE_SIZE;
    if (next == event_tail) return;
    event_queue[event_head].type = type;
    event_queue[event_head].key = 0;
    event_queue[event_head].x = cursor_x;
    event_queue[event_head].y = cursor_y;
    event_queue[event_head].button = button;
    event_head = next;
}

void mouse_init(void)
{
    mouse_available = 0;
    packet_index = 0;
    event_head = event_tail = 0;
    buttons = 0;
    cursor_x = fb_width() / 2;
    cursor_y = fb_height() / 2;

    if (!fb_is_available() || !write_command(0xA8) || !write_command(0x20) ||
        !wait_for_output_full()) return;
    uint8_t controller_config = io_inb(PS2_DATA_PORT);
    controller_config |= 0x02; /* Enable IRQ12. */
    if (!write_command(0x60) || !wait_for_input_empty()) return;
    io_outb(PS2_DATA_PORT, controller_config);

    if (!write_mouse(0xF6) || !write_mouse(0xF4)) return;
    mouse_available = 1;
}

void mouse_handle_irq(void)
{
    uint8_t status = io_inb(PS2_STATUS_PORT);
    if (!(status & PS2_STATUS_OUTPUT_FULL)) return;
    uint8_t value = io_inb(PS2_DATA_PORT);
    if (!mouse_available || !(status & PS2_STATUS_AUX_DATA)) return;

    if (packet_index == 0 && !(value & 0x08)) return;
    packet[packet_index++] = value;
    if (packet_index != 3) return;
    packet_index = 0;

    uint8_t flags = packet[0];
    if (!(flags & 0xC0)) {
        cursor_x += (int)(int8_t)packet[1];
        cursor_y -= (int)(int8_t)packet[2];
        if (cursor_x < 0) cursor_x = 0;
        if (cursor_y < 0) cursor_y = 0;
        if (cursor_x >= fb_width()) cursor_x = fb_width() - 1;
        if (cursor_y >= fb_height()) cursor_y = fb_height() - 1;
        if (packet[1] || packet[2]) queue_event(OR_EVENT_MOUSE_MOVE, 0);
    }

    uint8_t new_buttons = flags & 0x07;
    for (int button = 0; button < 3; button++) {
        uint8_t bit = (uint8_t)(1u << button);
        if ((buttons & bit) != (new_buttons & bit))
            queue_event((new_buttons & bit) ? OR_EVENT_MOUSE_DOWN : OR_EVENT_MOUSE_UP, button + 1);
    }
    buttons = new_buttons;
}

int mouse_is_available(void) { return mouse_available; }
int mouse_x(void) { return cursor_x; }
int mouse_y(void) { return cursor_y; }

int mouse_try_get_event(OREvent *event)
{
    if (event_tail == event_head) return 0;
    if (event) *event = event_queue[event_tail];
    event_tail = (event_tail + 1) % MOUSE_QUEUE_SIZE;
    return 1;
}

void mouse_draw_cursor(void)
{
    if (!mouse_available) return;
    int x = cursor_x, y = cursor_y;
    /* Compact black-outlined white arrow, drawn last on every desktop frame. */
    for (int row = 0; row < 12; row++) {
        for (int col = 0; col <= row / 2; col++) {
            uint32_t color = (col == 0 || col == row / 2 || row == 11) ? 0x000000 : 0xFFFFFF;
            fb_put_pixel(x + col, y + row, color);
        }
    }
    for (int i = 0; i < 6; i++) fb_put_pixel(x + 3 + i, y + 8 + i, 0x000000);
}
