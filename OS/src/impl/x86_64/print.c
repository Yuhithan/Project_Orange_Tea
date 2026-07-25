#include "imp.h"

const static size_t NUM_COLS = 80;
const static size_t NUM_ROWS = 25;

struct Char {
    uint8_t character;
    uint8_t color;
};

struct Char* buffer = (struct Char*) 0xb8000;
size_t col = 0;
size_t row = 0;
uint8_t color = PRINT_COLOR_BLACK | PRINT_COLOR_WHITE << 4;

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
    col = 0;
    row = 0;

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

    for (size_t row = 1; row < NUM_ROWS; row++) {
        for (size_t col = 0; col < NUM_COLS; col++) {
            struct Char character = buffer[col + NUM_COLS * row];
            buffer[col + NUM_COLS * (row - 1)] = character;
        }
    }

    clear_row(NUM_ROWS - 1);
}

void imp_char(char character) {
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

void imp_text(char* str) {
    for (size_t i = 0; 1; i++) {
        char character = (uint8_t) str[i];

        if (character == '\0') {
            return;
        }

        imp_char(character);
    }
}

void imp_color(uint8_t foreground, uint8_t background) {
    color = foreground + (background << 4);
}