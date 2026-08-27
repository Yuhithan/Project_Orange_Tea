#include "cursor.h"
#include "framebuffer.h"
#include "io.h"

#define PS2_DATA_PORT          0x60
#define PS2_STATUS_PORT        0x64
#define PS2_COMMAND_PORT      0x64

#define PS2_STATUS_OUTPUT_FULL 0x01
#define PS2_STATUS_INPUT_FULL  0x02
#define PS2_STATUS_AUX_DATA    0x20

#define MOUSE_QUEUE_SIZE       32

static volatile int mouse_available;
static volatile int packet_index;
static volatile uint8_t packet[3];

static volatile int cursor_x;
static volatile int cursor_y;
static volatile uint8_t buttons;

static volatile int event_head;
static volatile int event_tail;
static OREvent event_queue[MOUSE_QUEUE_SIZE];


/* Wait until PS/2 controller can accept data. */
static int wait_for_input_empty(void)
{
    for (int i = 0; i < 100000; i++) {
        if (!(io_inb(PS2_STATUS_PORT) & PS2_STATUS_INPUT_FULL))
            return 1;
    }

    return 0;
}


/* Wait until PS/2 controller has data available. */
static int wait_for_output_full(void)
{
    for (int i = 0; i < 100000; i++) {
        if (io_inb(PS2_STATUS_PORT) & PS2_STATUS_OUTPUT_FULL)
            return 1;
    }

    return 0;
}


/* Send command to PS/2 controller. */
static int write_command(uint8_t command)
{
    if (!wait_for_input_empty())
        return 0;

    io_outb(PS2_COMMAND_PORT, command);
    return 1;
}


/* Remove old data from PS/2 output buffer. */
static void drain_output(void)
{
    for (int i = 0; i < 32; i++) {
        uint8_t status = io_inb(PS2_STATUS_PORT);

        if (!(status & PS2_STATUS_OUTPUT_FULL))
            break;

        (void)io_inb(PS2_DATA_PORT);
    }
}


/*
 * Send command to PS/2 mouse.
 */
static int write_mouse(uint8_t command)
{
    /* Tell controller next byte is for mouse. */
    if (!write_command(0xD4))
        return 0;

    if (!wait_for_input_empty())
        return 0;

    io_outb(PS2_DATA_PORT, command);

    /* Wait for mouse ACK. */
    if (!wait_for_output_full())
        return 0;

    uint8_t response = io_inb(PS2_DATA_PORT);

    return response == 0xFA;
}


/* Put mouse event into queue. */
static void queue_event(OREventType type, int button)
{
    int next = (event_head + 1) % MOUSE_QUEUE_SIZE;

    if (next == event_tail)
        return;

    event_queue[event_head].type = type;
    event_queue[event_head].key = 0;
    event_queue[event_head].x = cursor_x;
    event_queue[event_head].y = cursor_y;
    event_queue[event_head].button = button;

    event_head = next;
}


/*
 * Initialize PS/2 mouse.
 */
void mouse_init(void)
{
    mouse_available = 0;
    packet_index = 0;

    event_head = 0;
    event_tail = 0;

    buttons = 0;

    if (!fb_is_available())
        return;

    /* Start cursor in center of screen. */
    cursor_x = fb_width() / 2;
    cursor_y = fb_height() / 2;

    /*
     * Enable PS/2 auxiliary device.
     */
    if (!write_command(0xA8))
        return;

    /*
     * Remove old keyboard/mouse data.
     */
    drain_output();

    /*
     * Read controller configuration byte.
     */
    if (!write_command(0x20))
        return;

    if (!wait_for_output_full())
        return;

    uint8_t controller_config = io_inb(PS2_DATA_PORT);

    /*
     * Enable IRQ12 and the auxiliary device clock.
     * Bit 1 = IRQ12 enable. Bit 5 = mouse clock disable (must be clear).
     */
    controller_config |= 0x02;
    controller_config &= (uint8_t)~0x20;

    /*
     * Write configuration byte.
     */
    if (!write_command(0x60))
        return;

    if (!wait_for_input_empty())
        return;

    io_outb(PS2_DATA_PORT, controller_config);

    /*
     * Reset packet state.
     */
    packet_index = 0;
    buttons = 0;

    /*
     * Set mouse defaults.
     */
    if (!write_mouse(0xF6))
        return;

    /*
     * Enable mouse data reporting.
     */
    if (!write_mouse(0xF4))
        return;

    mouse_available = 1;
}


/*
 * Called from IRQ12.
 */
void mouse_handle_irq(void)
{
    uint8_t status = io_inb(PS2_STATUS_PORT);

    /*
     * No data.
     */
    if (!(status & PS2_STATUS_OUTPUT_FULL))
        return;

    /*
     * Make sure this is mouse data.
     */
    if (!(status & PS2_STATUS_AUX_DATA))
        return;

    uint8_t value = io_inb(PS2_DATA_PORT);

    if (!mouse_available)
        return;

    /*
     * First byte must have bit 3 set.
     */
    if (packet_index == 0) {
        if (!(value & 0x08))
            return;
    }

    packet[packet_index] = value;
    packet_index++;

    /*
     * Need 3 bytes.
     */
    if (packet_index < 3)
        return;

    packet_index = 0;

    uint8_t flags = packet[0];

    /*
     * Ignore X/Y overflow.
     */
    if (flags & 0xC0)
        return;

    /*
     * Convert movement to signed values.
     */
    int dx = (int)(int8_t)packet[1];
    int dy = (int)(int8_t)packet[2];

    /*
     * Move cursor.
     */
    if (dx != 0 || dy != 0) {
        cursor_x += dx;
        cursor_y -= dy;

        int width = fb_width();
        int height = fb_height();

        /*
         * Keep cursor inside screen.
         */
        if (width > 0) {
            if (cursor_x < 0)
                cursor_x = 0;

            if (cursor_x >= width)
                cursor_x = width - 1;
        }

        if (height > 0) {
            if (cursor_y < 0)
                cursor_y = 0;

            if (cursor_y >= height)
                cursor_y = height - 1;
        }

        queue_event(OR_EVENT_MOUSE_MOVE, 0);
    }

    /*
     * Mouse buttons.
     *
     * bit 0 = left
     * bit 1 = right
     * bit 2 = middle
     */
    uint8_t new_buttons = flags & 0x07;

    for (int button = 0; button < 3; button++) {
        uint8_t bit = (uint8_t)(1u << button);

        if ((buttons & bit) != (new_buttons & bit)) {

            if (new_buttons & bit) {
                queue_event(
                    OR_EVENT_MOUSE_DOWN,
                    button + 1
                );
            } else {
                queue_event(
                    OR_EVENT_MOUSE_UP,
                    button + 1
                );
            }
        }
    }

    buttons = new_buttons;
}


/*
 * Is mouse available?
 */
int mouse_is_available(void)
{
    return mouse_available;
}


/*
 * Current X position.
 */
int mouse_x(void)
{
    return cursor_x;
}


/*
 * Current Y position.
 */
int mouse_y(void)
{
    return cursor_y;
}


/*
 * Get next mouse event.
 */
int mouse_try_get_event(OREvent *event)
{
    if (event_tail == event_head)
        return 0;

    if (event != 0)
        *event = event_queue[event_tail];

    event_tail =
        (event_tail + 1) % MOUSE_QUEUE_SIZE;

    return 1;
}


/*
 * Draw mouse cursor.
 *
 * This is a simple 12x12 white arrow
 * with a black outline.
 */
void mouse_draw_cursor(void)
{
    if (!mouse_available)
        return;

    int x = cursor_x;
    int y = cursor_y;

    /*
     * Draw arrow.
     */
    for (int row = 0; row < 12; row++) {

        for (int col = 0; col <= row / 2; col++) {

            uint32_t color;

            /*
             * Black outline.
             */
            if (col == 0 ||
                col == row / 2 ||
                row == 11) {

                color = 0x000000;

            } else {

                /* White inside. */
                color = 0xFFFFFF;
            }

            int px = x + col;
            int py = y + row;

            if (px >= 0 &&
                px < fb_width() &&
                py >= 0 &&
                py < fb_height()) {

                fb_put_pixel(px, py, color);
            }
        }
    }

    /*
     * Diagonal black edge.
     */
    for (int i = 0; i < 6; i++) {

        int px = x + 3 + i;
        int py = y + 8 + i;

        if (px >= 0 &&
            px < fb_width() &&
            py >= 0 &&
            py < fb_height()) {

            fb_put_pixel(px, py, 0x000000);
        }
    }
}