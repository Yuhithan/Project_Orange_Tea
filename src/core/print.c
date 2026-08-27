#include "imp.h"
#include "imp_b.h"

#define NUM_COLS 80
#define NUM_ROWS 25
#define SCROLLBACK_LINES 200

static const imp_backend_t *backend = NULL;



struct Char {
    uint8_t character;
    uint8_t color;
};

struct Char* buffer = (struct Char*) 0xb8000;
size_t col = 0;
size_t row = 0;
uint8_t color = PRINT_COLOR_BLACK | PRINT_COLOR_WHITE << 4;
static uint8_t scrollback[SCROLLBACK_LINES][80];
static struct Char scroll_view[NUM_ROWS][NUM_COLS];
static size_t scrollback_count = 0;
static size_t scroll_offset = 0;
static int scroll_view_active = 0;

static void render_scrollback(void)
{
    size_t start = scrollback_count - scroll_offset;

    for (size_t screen_row = 0; screen_row < NUM_ROWS; screen_row++) {
        size_t line = start + screen_row;
        for (size_t screen_col = 0; screen_col < NUM_COLS; screen_col++) {
            uint8_t character = ' ';
            if (line < scrollback_count) {
                character = scrollback[line][screen_col];
            } else if (line - scrollback_count < NUM_ROWS) {
                if (scroll_view_active) {
                    character = scroll_view[line - scrollback_count][screen_col].character;
                } else {
                    character = buffer[screen_col + NUM_COLS * (line - scrollback_count)].character;
                }
            }
            buffer[screen_col + NUM_COLS * screen_row].character = character;
        }
    }
}

static void return_to_bottom(void)
{
    if (scroll_view_active) {
        scroll_offset = 0;
        for (size_t screen_row = 0; screen_row < NUM_ROWS; screen_row++) {
            for (size_t screen_col = 0; screen_col < NUM_COLS; screen_col++) {
                buffer[screen_col + NUM_COLS * screen_row] = scroll_view[screen_row][screen_col];
            }
        }
        scroll_view_active = 0;
    }
}

void imp_set_backend(const imp_backend_t *b)
{
    backend = b;
}

void clear_row(size_t row) {
    struct Char empty = (struct Char) {
        character: ' ',
        color: color,
    };

    for (size_t col = 0; col < NUM_COLS; col++) {
        buffer[col + NUM_COLS * row] = empty;
    }
}

void imp_cls() {
    if (backend && backend->active()) {
    backend->clear();
    return;
    }

    col = 0;
    row = 0;
    scrollback_count = 0;
    scroll_offset = 0;
    scroll_view_active = 0;

    for (size_t i = 0; i < NUM_ROWS; i++) {
        clear_row(i);
    }
}

void imp_Nl() {
    col = 0;

    if (row < NUM_ROWS - 1) {
        row++;
        return;
    }

    if (scrollback_count < SCROLLBACK_LINES) {
        for (size_t col = 0; col < NUM_COLS; col++) {
            scrollback[scrollback_count][col] = buffer[col].character;
        }
        scrollback_count++;
    } else {
        for (size_t history_row = 1; history_row < SCROLLBACK_LINES; history_row++) {
            for (size_t col = 0; col < NUM_COLS; col++) {
                scrollback[history_row - 1][col] = scrollback[history_row][col];
            }
        }
        for (size_t col = 0; col < NUM_COLS; col++) {
            scrollback[SCROLLBACK_LINES - 1][col] = buffer[col].character;
        }
    }

    for (size_t screen_row = 1; screen_row < NUM_ROWS; screen_row++) {
        for (size_t col = 0; col < NUM_COLS; col++) {
            buffer[col + NUM_COLS * (screen_row - 1)] = buffer[col + NUM_COLS * screen_row];
        }
    }
    clear_row(NUM_ROWS - 1);
}

void imp_scroll_up(size_t lines)
{
    if (backend && backend->active()) return;
    if (!scroll_view_active) {
        for (size_t screen_row = 0; screen_row < NUM_ROWS; screen_row++) {
            for (size_t screen_col = 0; screen_col < NUM_COLS; screen_col++) {
                scroll_view[screen_row][screen_col] = buffer[screen_col + NUM_COLS * screen_row];
            }
        }
        scroll_view_active = 1;
    }
    if (lines > scrollback_count - scroll_offset) lines = scrollback_count - scroll_offset;
    scroll_offset += lines;
    render_scrollback();
}

void imp_scroll_down(size_t lines)
{
    if (backend && backend->active()) return;
    if (lines > scroll_offset) lines = scroll_offset;
    scroll_offset -= lines;
    if (scroll_offset == 0) {
        return_to_bottom();
    } else {
        render_scrollback();
    }
}

void imp_char(char character) {
    if (backend && backend->active()) {
    backend->put_char(character);
    return;
    }

    return_to_bottom();

    if (character == '\n') {
        imp_Nl();
        return;
    }

    if (character == '\b') {
        if (col > 0) {
            col--;
        } else if (row > 0) {
            row--;
            col = NUM_COLS - 1;
        }

        buffer[col + NUM_COLS * row] = (struct Char) {
            character: ' ',
            color: color,
        };
        return;
    }

    if (character == '\t') {
        for (int i = 0; i < 4; i++) {
            imp_char(' ');
        }
        return;
    }

    if (col >= NUM_COLS) {
        imp_Nl();
    }

    buffer[col + NUM_COLS * row] = (struct Char) {
        character: (uint8_t) character,
        color: color,
    };

    col++;
}

void imp_text(const char* str) {
    for (size_t i = 0; 1; i++) {
        char character = (uint8_t) str[i];

        if (character == '\0') {
            return;
        }

        imp_char(character);
    }
}

void imp_uint64_dec(uint64_t value) {
    char digits[20];
    size_t count = 0;

    do {
        digits[count++] = (char)('0' + (value % 10));
        value /= 10;
    } while (value != 0);

    while (count != 0) {
        imp_char(digits[--count]);
    }
}

void imp_uint64_hex(uint64_t value) {
    static const char digits[] = "0123456789abcdef";
    char output[16];
    size_t count = 0;

    do {
        output[count++] = digits[value & 0x0f];
        value >>= 4;
    } while (value != 0);

    while (count != 0) {
        imp_char(output[--count]);
    }
}

void imp_color(uint8_t foreground, uint8_t background) {
    color = foreground + (background << 4);
    if (backend && backend->active()) {
    backend->set_color(foreground, background);
    }
}
