#include "cursor.h"
#include "framebuffer.h"
#include "io.h"

#include <stddef.h>

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

#define CURSOR_MAX_SIZE 128

static uint32_t cursor_pixels[CURSOR_MAX_SIZE * CURSOR_MAX_SIZE];
static uint32_t cursor_background[CURSOR_MAX_SIZE * CURSOR_MAX_SIZE];
static uint8_t cursor_saved[CURSOR_MAX_SIZE * CURSOR_MAX_SIZE];
static int cursor_width;
static int cursor_height;
static int cursor_hotspot_x;
static int cursor_hotspot_y;
static int cursor_loaded;
static int cursor_visible = 1;
static int cursor_rendered;
static int cursor_rendered_x;
static int cursor_rendered_y;

extern const unsigned char ortos_cursor_resource[];
extern const unsigned long ortos_cursor_resource_size;

static uint16_t cursor_read_u16(const unsigned char *data, size_t offset)
{
    return (uint16_t)data[offset] | ((uint16_t)data[offset + 1] << 8);
}

static uint32_t cursor_read_u32(const unsigned char *data, size_t offset)
{
    return (uint32_t)data[offset] |
        ((uint32_t)data[offset + 1] << 8) |
        ((uint32_t)data[offset + 2] << 16) |
        ((uint32_t)data[offset + 3] << 24);
}

static int cursor_range_valid(size_t offset, size_t length, size_t size)
{
    return offset <= size && length <= size - offset;
}

static uint32_t cursor_blend(uint32_t source, uint32_t destination)
{
    uint32_t alpha = source >> 24;
    uint32_t inverse = 255u - alpha;
    uint32_t red = (((source >> 16) & 0xFFu) * alpha +
        ((destination >> 16) & 0xFFu) * inverse + 127u) / 255u;
    uint32_t green = (((source >> 8) & 0xFFu) * alpha +
        ((destination >> 8) & 0xFFu) * inverse + 127u) / 255u;
    uint32_t blue = ((source & 0xFFu) * alpha +
        (destination & 0xFFu) * inverse + 127u) / 255u;
    return (red << 16) | (green << 8) | blue;
}

static void cursor_restore(void)
{
    if (!cursor_rendered) return;

    for (int row = 0; row < cursor_height; row++) {
        for (int column = 0; column < cursor_width; column++) {
            int index = row * cursor_width + column;
            if (cursor_saved[index]) {
                fb_put_pixel(cursor_rendered_x + column,
                    cursor_rendered_y + row, cursor_background[index]);
            }
        }
    }
    cursor_rendered = 0;
}

static int cursor_parse(const unsigned char *data, size_t size)
{
    if (data == 0 || size < 6) return CURSOR_ERROR_MALFORMED;
    if (cursor_read_u16(data, 0) != 0 || cursor_read_u16(data, 2) != 2)
        return CURSOR_ERROR_MALFORMED;

    uint16_t image_count = cursor_read_u16(data, 4);
    if (image_count == 0 || !cursor_range_valid(6, 16, size))
        return CURSOR_ERROR_MALFORMED;

    for (uint16_t image = 0; image < image_count; image++) {
        size_t entry = 6u + (size_t)image * 16u;
        if (!cursor_range_valid(entry, 16, size)) return CURSOR_ERROR_MALFORMED;

        int width = data[entry] == 0 ? 256 : data[entry];
        int height = data[entry + 1] == 0 ? 256 : data[entry + 1];
        uint32_t image_size = cursor_read_u32(data, entry + 8);
        uint32_t image_offset = cursor_read_u32(data, entry + 12);
        if (width > CURSOR_MAX_SIZE || height > CURSOR_MAX_SIZE ||
            image_size == 0 || image_offset > size ||
            image_size > size - image_offset || image_size < 40)
            continue;

        size_t offset = image_offset;
        uint32_t dib_size = cursor_read_u32(data, offset);
        if (dib_size < 40 || dib_size > image_size) continue;
        int32_t dib_width = (int32_t)cursor_read_u32(data, offset + 4);
        int32_t dib_height = (int32_t)cursor_read_u32(data, offset + 8);
        if (dib_width != width || dib_height <= 0 || dib_height != height * 2 ||
            cursor_read_u16(data, offset + 12) != 1 ||
            cursor_read_u16(data, offset + 14) != 32 ||
            cursor_read_u32(data, offset + 16) != 0)
            continue;

        size_t pixel_bytes = (size_t)width * (size_t)height * 4u;
        size_t mask_stride = ((size_t)width + 31u) / 32u * 4u;
        size_t mask_bytes = mask_stride * (size_t)height;
        size_t pixel_offset = offset + dib_size;
        if (pixel_offset < offset || !cursor_range_valid(pixel_offset,
            pixel_bytes, size) || !cursor_range_valid(pixel_offset + pixel_bytes,
            mask_bytes, size) || pixel_offset + pixel_bytes + mask_bytes >
            (size_t)image_offset + image_size)
            continue;

        int has_alpha = 0;
        for (int row = 0; row < height; row++) {
            int source_row = height - 1 - row;
            for (int column = 0; column < width; column++) {
                size_t pixel = pixel_offset + ((size_t)source_row * width + column) * 4u;
                if (data[pixel + 3] != 0) has_alpha = 1;
            }
        }

        for (int row = 0; row < height; row++) {
            int source_row = height - 1 - row;
            for (int column = 0; column < width; column++) {
                size_t pixel = pixel_offset + ((size_t)source_row * width + column) * 4u;
                uint8_t alpha = data[pixel + 3];
                if (!has_alpha) {
                    size_t mask = pixel_offset + pixel_bytes +
                        (size_t)source_row * mask_stride + (size_t)column / 8u;
                    alpha = (data[mask] & (uint8_t)(0x80u >> (column % 8))) ? 0 : 255;
                }
                cursor_pixels[row * width + column] =
                    ((uint32_t)alpha << 24) |
                    ((uint32_t)data[pixel + 2] << 16) |
                    ((uint32_t)data[pixel + 1] << 8) |
                    data[pixel];
            }
        }

        cursor_width = width;
        cursor_height = height;
        cursor_hotspot_x = cursor_read_u16(data, entry + 4);
        cursor_hotspot_y = cursor_read_u16(data, entry + 6);
        if (cursor_hotspot_x >= width || cursor_hotspot_y >= height)
            return CURSOR_ERROR_MALFORMED;
        cursor_loaded = 1;
        return CURSOR_OK;
    }

    return CURSOR_ERROR_UNSUPPORTED_FORMAT;
}


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
    (void)cursor_load("/cursor/cursor_arrow.cur");

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

    /* Leave initialization replies for mouse_init() to consume. */
    if (!mouse_available)
        return;

    uint8_t value = io_inb(PS2_DATA_PORT);

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
        cursor_set_position(cursor_x + dx, cursor_y - dy);

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


int cursor_load_data(const unsigned char *data, unsigned long size)
{
    cursor_destroy();
    int result = cursor_parse(data, (size_t)size);
    if (result == CURSOR_OK) cursor_visible = 1;
    return result;
}

int cursor_load(const char *path)
{
    if (path == 0) return CURSOR_ERROR_INVALID_ARGUMENT;

    const char *expected = "/cursor/cursor_arrow.cur";
    int index = 0;
    while (path[index] != '\0' && path[index] == expected[index]) index++;
    if (path[index] != '\0' || expected[index] != '\0')
        return CURSOR_ERROR_MISSING;

    return cursor_load_data(ortos_cursor_resource,
        ortos_cursor_resource_size);
}

void cursor_destroy(void)
{
    cursor_restore();
    cursor_loaded = 0;
    cursor_visible = 0;
    cursor_width = 0;
    cursor_height = 0;
}

void cursor_set_position(int x, int y)
{
    int width = fb_width();
    int height = fb_height();
    if (width > 0) {
        if (x < 0) x = 0;
        if (x >= width) x = width - 1;
    }
    if (height > 0) {
        if (y < 0) y = 0;
        if (y >= height) y = height - 1;
    }
    cursor_x = x;
    cursor_y = y;
}

void cursor_begin_frame(void)
{
    cursor_rendered = 0;
}

void cursor_draw(void)
{
    if (!cursor_loaded || !cursor_visible || !fb_is_available()) return;
    if (cursor_rendered && (cursor_rendered_x != cursor_x ||
        cursor_rendered_y != cursor_y)) cursor_restore();
    if (cursor_rendered) return;

    int draw_x = cursor_x - cursor_hotspot_x;
    int draw_y = cursor_y - cursor_hotspot_y;
    for (int row = 0; row < cursor_height; row++) {
        for (int column = 0; column < cursor_width; column++) {
            int index = row * cursor_width + column;
            int px = draw_x + column;
            int py = draw_y + row;
            cursor_saved[index] = 0;
            if (px < 0 || py < 0 || px >= fb_width() || py >= fb_height()) continue;
            uint32_t source = cursor_pixels[index];
            if ((source >> 24) == 0) continue;
            cursor_background[index] = fb_get_pixel(px, py);
            cursor_saved[index] = 1;
            fb_put_pixel(px, py, cursor_blend(source, cursor_background[index]));
        }
    }
    cursor_rendered_x = draw_x;
    cursor_rendered_y = draw_y;
    cursor_rendered = 1;
}

void cursor_show(void)
{
    cursor_visible = 1;
    cursor_draw();
}

void cursor_hide(void)
{
    cursor_restore();
    cursor_visible = 0;
}

void mouse_draw_cursor(void)
{
    cursor_draw();
}