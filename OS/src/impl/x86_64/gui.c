#include "gui.h"
#include "keyboard.h"
#include "imp.h"
#include "network.h"
#include <stdint.h>
#include <stddef.h>

#define GUI_WIDTH 320
#define GUI_HEIGHT 200
#define GUI_COLS 80
#define GUI_ROWS 25
#define GUI_TERMINAL_X 10
#define GUI_TERMINAL_Y 24
#define GUI_TERMINAL_W 300
#define GUI_TERMINAL_H 160

static uint32_t gui_backbuffer[GUI_WIDTH * GUI_HEIGHT];
struct framebuffer framebuffer = {
    .address = gui_backbuffer,
    .width = GUI_WIDTH,
    .height = GUI_HEIGHT,
    .pitch = GUI_WIDTH * 4
};

static int gui_active = 0;
static int gui_cursor_visible = 1;
static int gui_cursor_x = 0;
static int gui_cursor_y = 0;
static int gui_console_col = 0;
static int gui_console_row = 0;
static uint8_t gui_console_fg = 0x0F;
static uint8_t gui_console_bg = 0x00;

struct gui_cell {
    char character;
    uint8_t foreground;
    uint8_t background;
};

static struct gui_cell gui_console[GUI_COLS * GUI_ROWS];

struct gui_vga_char {
    uint8_t character;
    uint8_t color;
};

static struct gui_vga_char* gui_vga_buffer = (struct gui_vga_char*)0xB8000;

static void gui_vga_write_char(int x, int y, char character, uint8_t color)
{
    if (x < 0 || y < 0 || x >= 80 || y >= 25)
    {
        return;
    }

    int index = y * 80 + x;
    gui_vga_buffer[index].character = (uint8_t)character;
    gui_vga_buffer[index].color = color;
}

static void gui_vga_write_string(int x, int y, const char* text, uint8_t color)
{
    int offset = 0;
    while (text[offset] != '\0')
    {
        gui_vga_write_char(x + offset, y, text[offset], color);
        offset++;
    }
}

static void gui_render_vga_view(void)
{
    for (int i = 0; i < 80 * 25; i++)
    {
        gui_vga_buffer[i].character = ' ';
        gui_vga_buffer[i].color = 0x07;
    }

    for (int x = 0; x < 80; x++)
    {
        gui_vga_write_char(x, 0, '=', 0x0F);
        gui_vga_write_char(x, 24, '=', 0x0F);
    }

    for (int y = 0; y < 25; y++)
    {
        gui_vga_write_char(0, y, '|', 0x0F);
        gui_vga_write_char(79, y, '|', 0x0F);
    }

    gui_vga_write_string(3, 1, "Orange Tea OS GUI", 0x0E);
    gui_vga_write_string(3, 2, "Shell in GUI mode", 0x0A);
    gui_vga_write_string(3, 3, "--------------------", 0x07);

    for (int row = 0; row < GUI_ROWS; row++)
    {
        for (int col = 0; col < GUI_COLS; col++)
        {
            int index = row * GUI_COLS + col;
            struct gui_cell cell = gui_console[index];
            if (cell.character == '\0' || cell.character == ' ')
            {
                continue;
            }

            int x = 3 + col;
            int y = 5 + row;
            if (x >= 80 || y >= 25)
            {
                continue;
            }

            uint8_t color = 0x0F;
            if (cell.foreground == 0x08)
            {
                color = 0x08;
            }
            else if (cell.foreground != 0x0F)
            {
                color = 0x07;
            }

            gui_vga_write_char(x, y, cell.character, color);
        }
    }
}

static void gui_draw_char(int x, int y, char character, uint32_t color)
{
    if (character == ' ')
    {
        return;
    }

    if (character >= 'a' && character <= 'z')
    {
        character = (char)('A' + (character - 'a'));
    }

    switch (character)
    {
        case 'A':
            draw_rect(x + 1, y, 3, 1, color);
            draw_rect(x, y + 1, 1, 6, color);
            draw_rect(x + 4, y + 1, 1, 6, color);
            draw_rect(x + 1, y + 3, 3, 1, color);
            break;
        case 'B':
            draw_rect(x, y, 1, 7, color);
            draw_rect(x + 1, y, 3, 1, color);
            draw_rect(x + 1, y + 3, 3, 1, color);
            draw_rect(x + 1, y + 6, 3, 1, color);
            draw_rect(x + 3, y + 1, 1, 2, color);
            draw_rect(x + 3, y + 4, 1, 2, color);
            break;
        case 'C':
            draw_rect(x + 1, y, 3, 1, color);
            draw_rect(x, y + 1, 1, 5, color);
            draw_rect(x + 1, y + 6, 3, 1, color);
            break;
        case 'D':
            draw_rect(x, y, 1, 7, color);
            draw_rect(x + 1, y, 3, 1, color);
            draw_rect(x + 4, y + 1, 1, 5, color);
            draw_rect(x + 1, y + 6, 3, 1, color);
            break;
        case 'E':
            draw_rect(x, y, 1, 7, color);
            draw_rect(x + 1, y, 4, 1, color);
            draw_rect(x + 1, y + 3, 3, 1, color);
            draw_rect(x + 1, y + 6, 4, 1, color);
            break;
        case 'F':
            draw_rect(x, y, 1, 7, color);
            draw_rect(x + 1, y, 4, 1, color);
            draw_rect(x + 1, y + 3, 3, 1, color);
            break;
        case 'G':
            draw_rect(x + 1, y, 3, 1, color);
            draw_rect(x, y + 1, 1, 5, color);
            draw_rect(x + 1, y + 6, 3, 1, color);
            draw_rect(x + 4, y + 3, 1, 2, color);
            draw_rect(x + 2, y + 3, 2, 1, color);
            break;
        case 'H':
            draw_rect(x, y, 1, 7, color);
            draw_rect(x + 4, y, 1, 7, color);
            draw_rect(x + 1, y + 3, 3, 1, color);
            break;
        case 'I':
            draw_rect(x, y, 5, 1, color);
            draw_rect(x + 2, y + 1, 1, 5, color);
            draw_rect(x, y + 6, 5, 1, color);
            break;
        case 'J':
            draw_rect(x + 2, y, 3, 1, color);
            draw_rect(x + 4, y + 1, 1, 5, color);
            draw_rect(x, y + 6, 4, 1, color);
            break;
        case 'K':
            draw_rect(x, y, 1, 7, color);
            draw_rect(x + 2, y + 3, 2, 1, color);
            draw_rect(x + 4, y, 1, 3, color);
            draw_rect(x + 4, y + 4, 1, 3, color);
            break;
        case 'L':
            draw_rect(x, y, 1, 7, color);
            draw_rect(x + 1, y + 6, 4, 1, color);
            break;
        case 'M':
            draw_rect(x, y, 1, 7, color);
            draw_rect(x + 4, y, 1, 7, color);
            draw_rect(x + 1, y + 1, 1, 2, color);
            draw_rect(x + 3, y + 1, 1, 2, color);
            draw_rect(x + 2, y, 1, 1, color);
            break;
        case 'N':
            draw_rect(x, y, 1, 7, color);
            draw_rect(x + 4, y, 1, 7, color);
            draw_rect(x + 1, y + 1, 1, 1, color);
            draw_rect(x + 2, y + 2, 1, 1, color);
            draw_rect(x + 3, y + 3, 1, 1, color);
            break;
        case 'O':
            draw_rect(x + 1, y, 3, 1, color);
            draw_rect(x, y + 1, 1, 5, color);
            draw_rect(x + 4, y + 1, 1, 5, color);
            draw_rect(x + 1, y + 6, 3, 1, color);
            break;
        case 'P':
            draw_rect(x, y, 1, 7, color);
            draw_rect(x + 1, y, 3, 1, color);
            draw_rect(x + 4, y + 1, 1, 2, color);
            draw_rect(x + 1, y + 3, 3, 1, color);
            break;
        case 'Q':
            draw_rect(x + 1, y, 3, 1, color);
            draw_rect(x, y + 1, 1, 5, color);
            draw_rect(x + 4, y + 1, 1, 5, color);
            draw_rect(x + 1, y + 6, 3, 1, color);
            draw_rect(x + 3, y + 4, 1, 2, color);
            break;
        case 'R':
            draw_rect(x, y, 1, 7, color);
            draw_rect(x + 1, y, 3, 1, color);
            draw_rect(x + 4, y + 1, 1, 2, color);
            draw_rect(x + 1, y + 3, 3, 1, color);
            draw_rect(x + 3, y + 4, 1, 3, color);
            break;
        case 'S':
            draw_rect(x + 1, y, 3, 1, color);
            draw_rect(x, y + 1, 1, 2, color);
            draw_rect(x + 1, y + 3, 3, 1, color);
            draw_rect(x + 4, y + 4, 1, 2, color);
            draw_rect(x + 1, y + 6, 3, 1, color);
            break;
        case 'T':
            draw_rect(x, y, 5, 1, color);
            draw_rect(x + 2, y + 1, 1, 6, color);
            break;
        case 'U':
            draw_rect(x, y + 1, 1, 6, color);
            draw_rect(x + 4, y + 1, 1, 6, color);
            draw_rect(x + 1, y + 6, 3, 1, color);
            break;
        case 'V':
            draw_rect(x, y, 1, 5, color);
            draw_rect(x + 4, y, 1, 5, color);
            draw_rect(x + 1, y + 5, 1, 2, color);
            draw_rect(x + 3, y + 5, 1, 2, color);
            break;
        case 'W':
            draw_rect(x, y, 1, 7, color);
            draw_rect(x + 4, y, 1, 7, color);
            draw_rect(x + 2, y + 5, 1, 2, color);
            break;
        case 'X':
            draw_rect(x, y, 1, 2, color);
            draw_rect(x + 4, y, 1, 2, color);
            draw_rect(x + 1, y + 2, 1, 3, color);
            draw_rect(x + 3, y + 2, 1, 3, color);
            draw_rect(x, y + 5, 1, 2, color);
            draw_rect(x + 4, y + 5, 1, 2, color);
            break;
        case 'Y':
            draw_rect(x, y, 1, 3, color);
            draw_rect(x + 4, y, 1, 3, color);
            draw_rect(x + 1, y + 3, 3, 1, color);
            draw_rect(x + 2, y + 4, 1, 3, color);
            break;
        case 'Z':
            draw_rect(x, y, 5, 1, color);
            draw_rect(x + 4, y + 1, 1, 5, color);
            draw_rect(x, y + 6, 5, 1, color);
            draw_rect(x + 1, y + 4, 3, 1, color);
            break;
        case '0':
            draw_rect(x + 1, y, 3, 1, color);
            draw_rect(x, y + 1, 1, 5, color);
            draw_rect(x + 4, y + 1, 1, 5, color);
            draw_rect(x + 1, y + 6, 3, 1, color);
            draw_rect(x + 2, y + 3, 1, 1, color);
            break;
        case '1':
            draw_rect(x + 2, y, 1, 1, color);
            draw_rect(x + 1, y + 1, 1, 6, color);
            draw_rect(x, y + 6, 3, 1, color);
            break;
        case '2':
            draw_rect(x + 1, y, 3, 1, color);
            draw_rect(x + 4, y + 1, 1, 2, color);
            draw_rect(x + 1, y + 3, 3, 1, color);
            draw_rect(x, y + 4, 1, 2, color);
            draw_rect(x + 1, y + 6, 3, 1, color);
            break;
        case '3':
            draw_rect(x + 1, y, 3, 1, color);
            draw_rect(x + 4, y + 1, 1, 5, color);
            draw_rect(x + 1, y + 3, 3, 1, color);
            draw_rect(x + 1, y + 6, 3, 1, color);
            break;
        case '4':
            draw_rect(x + 3, y, 1, 7, color);
            draw_rect(x, y + 3, 4, 1, color);
            draw_rect(x + 1, y, 1, 3, color);
            break;
        case '5':
            draw_rect(x, y, 4, 1, color);
            draw_rect(x, y + 1, 1, 3, color);
            draw_rect(x + 1, y + 3, 3, 1, color);
            draw_rect(x + 4, y + 4, 1, 2, color);
            draw_rect(x + 1, y + 6, 3, 1, color);
            break;
        case '6':
            draw_rect(x + 1, y, 3, 1, color);
            draw_rect(x, y + 1, 1, 6, color);
            draw_rect(x + 1, y + 3, 3, 1, color);
            draw_rect(x + 4, y + 4, 1, 2, color);
            draw_rect(x + 1, y + 6, 3, 1, color);
            break;
        case '7':
            draw_rect(x, y, 5, 1, color);
            draw_rect(x + 4, y + 1, 1, 6, color);
            break;
        case '8':
            draw_rect(x + 1, y, 3, 1, color);
            draw_rect(x, y + 1, 1, 5, color);
            draw_rect(x + 4, y + 1, 1, 5, color);
            draw_rect(x + 1, y + 3, 3, 1, color);
            draw_rect(x + 1, y + 6, 3, 1, color);
            break;
        case '9':
            draw_rect(x + 1, y, 3, 1, color);
            draw_rect(x + 4, y + 1, 1, 5, color);
            draw_rect(x, y + 1, 1, 2, color);
            draw_rect(x + 1, y + 3, 3, 1, color);
            draw_rect(x + 1, y + 6, 3, 1, color);
            break;
        case '.':
            draw_rect(x + 2, y + 6, 1, 1, color);
            break;
        case ':':
            draw_rect(x + 2, y + 2, 1, 1, color);
            draw_rect(x + 2, y + 5, 1, 1, color);
            break;
        case '-':
            draw_rect(x + 1, y + 3, 3, 1, color);
            break;
        case '_':
            draw_rect(x, y + 6, 5, 1, color);
            break;
        case '+':
            draw_rect(x + 2, y + 1, 1, 5, color);
            draw_rect(x + 1, y + 3, 3, 1, color);
            break;
        case '/':
            draw_rect(x + 4, y, 1, 2, color);
            draw_rect(x + 3, y + 2, 1, 2, color);
            draw_rect(x + 2, y + 4, 1, 2, color);
            draw_rect(x + 1, y + 6, 1, 1, color);
            break;
        case '\\':
            draw_rect(x, y, 1, 2, color);
            draw_rect(x + 1, y + 2, 1, 2, color);
            draw_rect(x + 2, y + 4, 1, 2, color);
            draw_rect(x + 3, y + 6, 1, 1, color);
            break;
        case '=':
            draw_rect(x + 1, y + 2, 3, 1, color);
            draw_rect(x + 1, y + 4, 3, 1, color);
            break;
        case '!':
            draw_rect(x + 2, y, 1, 5, color);
            draw_rect(x + 2, y + 6, 1, 1, color);
            break;
        case '?':
            draw_rect(x + 1, y, 3, 1, color);
            draw_rect(x + 4, y + 1, 1, 2, color);
            draw_rect(x + 2, y + 3, 1, 1, color);
            draw_rect(x + 2, y + 6, 1, 1, color);
            break;
        case '(':
            draw_rect(x + 2, y, 1, 2, color);
            draw_rect(x + 1, y + 2, 1, 3, color);
            draw_rect(x + 2, y + 5, 1, 2, color);
            break;
        case ')':
            draw_rect(x + 2, y, 1, 2, color);
            draw_rect(x + 3, y + 2, 1, 3, color);
            draw_rect(x + 2, y + 5, 1, 2, color);
            break;
        case '[':
            draw_rect(x + 1, y, 2, 1, color);
            draw_rect(x, y + 1, 1, 5, color);
            draw_rect(x + 1, y + 6, 2, 1, color);
            break;
        case ']':
            draw_rect(x + 2, y, 2, 1, color);
            draw_rect(x + 3, y + 1, 1, 5, color);
            draw_rect(x + 2, y + 6, 2, 1, color);
            break;
        case '{':
            draw_rect(x + 2, y, 2, 1, color);
            draw_rect(x + 1, y + 1, 1, 2, color);
            draw_rect(x + 2, y + 3, 2, 1, color);
            draw_rect(x + 1, y + 4, 1, 2, color);
            draw_rect(x + 2, y + 6, 2, 1, color);
            break;
        case '}':
            draw_rect(x + 1, y, 2, 1, color);
            draw_rect(x + 3, y + 1, 1, 2, color);
            draw_rect(x + 1, y + 3, 2, 1, color);
            draw_rect(x + 3, y + 4, 1, 2, color);
            draw_rect(x + 1, y + 6, 2, 1, color);
            break;
        case '<':
            draw_rect(x + 3, y + 1, 1, 2, color);
            draw_rect(x + 2, y + 3, 1, 1, color);
            draw_rect(x + 1, y + 4, 1, 2, color);
            break;
        case '>':
            draw_rect(x + 1, y + 1, 1, 2, color);
            draw_rect(x + 2, y + 3, 1, 1, color);
            draw_rect(x + 3, y + 4, 1, 2, color);
            break;
        case '|':
            draw_rect(x + 2, y, 1, 7, color);
            break;
        case '#':
            draw_rect(x + 1, y + 1, 3, 1, color);
            draw_rect(x + 1, y + 3, 3, 1, color);
            draw_rect(x + 2, y, 1, 7, color);
            break;
        case '$':
            draw_rect(x + 1, y, 3, 1, color);
            draw_rect(x + 4, y + 1, 1, 2, color);
            draw_rect(x + 1, y + 3, 3, 1, color);
            draw_rect(x, y + 4, 1, 2, color);
            draw_rect(x + 1, y + 6, 3, 1, color);
            break;
        case '@':
            draw_rect(x + 1, y, 3, 1, color);
            draw_rect(x, y + 1, 1, 5, color);
            draw_rect(x + 4, y + 1, 1, 5, color);
            draw_rect(x + 1, y + 6, 3, 1, color);
            draw_rect(x + 2, y + 3, 1, 1, color);
            break;
        case '*':
            draw_rect(x + 2, y, 1, 7, color);
            draw_rect(x, y + 2, 5, 1, color);
            break;
        case '%':
            draw_rect(x, y, 1, 2, color);
            draw_rect(x + 3, y, 1, 2, color);
            draw_rect(x + 1, y + 2, 3, 1, color);
            draw_rect(x + 1, y + 4, 3, 1, color);
            draw_rect(x, y + 5, 1, 2, color);
            draw_rect(x + 3, y + 5, 1, 2, color);
            break;
        case '&':
            draw_rect(x + 1, y, 3, 1, color);
            draw_rect(x, y + 1, 1, 2, color);
            draw_rect(x + 1, y + 3, 3, 1, color);
            draw_rect(x + 4, y + 4, 1, 2, color);
            draw_rect(x + 1, y + 6, 3, 1, color);
            break;
        default:
            draw_rect(x, y, 5, 7, color);
            break;
    }
}

static void gui_render_console(void)
{
    int cell_w = 6;
    int cell_h = 8;
    for (int row = 0; row < GUI_ROWS; row++)
    {
        for (int col = 0; col < GUI_COLS; col++)
        {
            int index = row * GUI_COLS + col;
            struct gui_cell cell = gui_console[index];
            if (cell.character == '\0')
            {
                continue;
            }

            int x = GUI_TERMINAL_X + col * cell_w + 4;
            int y = GUI_TERMINAL_Y + row * cell_h + 4;
            uint32_t color = 0x00FFFFFF;
            if (cell.foreground == 0x0F)
            {
                color = 0x00FFFFFF;
            }
            else if (cell.foreground == 0x08)
            {
                color = 0x00999999;
            }
            else
            {
                color = 0x00C0C0C0;
            }
            gui_draw_char(x, y, cell.character, color);
        }
    }
}

static void gui_render_frame(void)
{
    fill_rect(0, 0, GUI_WIDTH, GUI_HEIGHT, 0xFF1F2D3D);
    draw_rect(0, 0, GUI_WIDTH, GUI_HEIGHT, 0xFF0B1020);
    fill_rect(0, GUI_HEIGHT - 24, GUI_WIDTH, 24, 0xFF263547);
    draw_rect(6, GUI_HEIGHT - 20, 48, 12, 0xFF4A90E2);
    draw_string(16, GUI_HEIGHT - 18, "ORT", 0x00FFFFFF);
    draw_string(70, GUI_HEIGHT - 18, "Orange Tea OS", 0x00DDEEFF);
    draw_rect(6, 6, GUI_WIDTH - 12, GUI_HEIGHT - 36, 0xFF102030);
    draw_rect_outline(6, 6, GUI_WIDTH - 12, GUI_HEIGHT - 36, 0xFF5A6A7A);
    draw_rect(GUI_TERMINAL_X, GUI_TERMINAL_Y, GUI_TERMINAL_W, GUI_TERMINAL_H, 0xFF000000);
    draw_rect_outline(GUI_TERMINAL_X, GUI_TERMINAL_Y, GUI_TERMINAL_W, GUI_TERMINAL_H, 0xFF4A90E2);
    draw_string(GUI_TERMINAL_X + 6, GUI_TERMINAL_Y + 4, "Terminal", 0x00FFFFFF);
    gui_render_console();
    gui_render_vga_view();

    if (gui_cursor_visible)
    {
        int cursor_x = GUI_TERMINAL_X + 8 + gui_console_col * 6;
        int cursor_y = GUI_TERMINAL_Y + 20 + gui_console_row * 8;
        fill_rect(cursor_x, cursor_y, 4, 8, 0x00FFFFFF);
    }
}

static void gui_scroll_console(void)
{
    for (int row = 1; row < GUI_ROWS; row++)
    {
        for (int col = 0; col < GUI_COLS; col++)
        {
            int dst = (row - 1) * GUI_COLS + col;
            int src = row * GUI_COLS + col;
            gui_console[dst] = gui_console[src];
        }
    }

    for (int col = 0; col < GUI_COLS; col++)
    {
        gui_console[(GUI_ROWS - 1) * GUI_COLS + col].character = ' ';
        gui_console[(GUI_ROWS - 1) * GUI_COLS + col].foreground = gui_console_fg;
        gui_console[(GUI_ROWS - 1) * GUI_COLS + col].background = gui_console_bg;
    }
}

void put_pixel(int x, int y, uint32_t color)
{
    if (x < 0 || y < 0 || x >= framebuffer.width || y >= framebuffer.height)
    {
        return;
    }

    framebuffer.address[y * framebuffer.width + x] = color;
}

void draw_pixel(int x, int y, uint32_t color)
{
    put_pixel(x, y, color);
}

void draw_line(int x0, int y0, int x1, int y1, uint32_t color)
{
    int dx = x1 > x0 ? x1 - x0 : x0 - x1;
    int sx = x0 < x1 ? 1 : -1;
    int dy = y1 > y0 ? y1 - y0 : y0 - y1;
    int sy = y0 < y1 ? 1 : -1;
    int err = (dx > dy ? dx : -dy) / 2;
    int e2;

    while (1)
    {
        put_pixel(x0, y0, color);
        if (x0 == x1 && y0 == y1)
        {
            break;
        }
        e2 = err;
        if (e2 > -dx)
        {
            err -= dy;
            x0 += sx;
        }
        if (e2 < dy)
        {
            err += dx;
            y0 += sy;
        }
    }
}

void draw_rect(int x, int y, int width, int height, uint32_t color)
{
    for (int iy = 0; iy < height; iy++)
    {
        for (int ix = 0; ix < width; ix++)
        {
            put_pixel(x + ix, y + iy, color);
        }
    }
}

void draw_rect_outline(int x, int y, int width, int height, uint32_t color)
{
    for (int i = 0; i < width; i++)
    {
        put_pixel(x + i, y, color);
        put_pixel(x + i, y + height - 1, color);
    }

    for (int i = 0; i < height; i++)
    {
        put_pixel(x, y + i, color);
        put_pixel(x + width - 1, y + i, color);
    }
}

void fill_rect(int x, int y, int width, int height, uint32_t color)
{
    draw_rect(x, y, width, height, color);
}

void draw_circle(int x, int y, int radius, uint32_t color)
{
    for (int iy = -radius; iy <= radius; iy++)
    {
        for (int ix = -radius; ix <= radius; ix++)
        {
            if ((ix * ix) + (iy * iy) <= radius * radius)
            {
                put_pixel(x + ix, y + iy, color);
            }
        }
    }
}

void draw_string(int x, int y, const char* text, uint32_t color)
{
    int offset = 0;
    while (text[offset] != '\0')
    {
        gui_draw_char(x + offset * 6, y, text[offset], color);
        offset++;
    }
}

void gui_console_clear(void)
{
    if (!gui_active)
    {
        return;
    }

    for (int i = 0; i < GUI_COLS * GUI_ROWS; i++)
    {
        gui_console[i].character = ' ';
        gui_console[i].foreground = gui_console_fg;
        gui_console[i].background = gui_console_bg;
    }

    gui_console_col = 0;
    gui_console_row = 0;
    gui_render_frame();
}

void gui_console_write_char(char character)
{
    if (!gui_active)
    {
        return;
    }

    if (character == '\n')
    {
        gui_console_col = 0;
        gui_console_row++;
    }
    else if (character == '\b')
    {
        if (gui_console_col > 0)
        {
            gui_console_col--;
        }
        else if (gui_console_row > 0)
        {
            gui_console_row--;
            gui_console_col = GUI_COLS - 1;
        }

        int index = gui_console_row * GUI_COLS + gui_console_col;
        gui_console[index].character = ' ';
        gui_console[index].foreground = gui_console_fg;
        gui_console[index].background = gui_console_bg;
    }
    else if (character == '\t')
    {
        for (int i = 0; i < 4; i++)
        {
            gui_console_write_char(' ');
        }
        return;
    }
    else
    {
        int index = gui_console_row * GUI_COLS + gui_console_col;
        gui_console[index].character = character;
        gui_console[index].foreground = gui_console_fg;
        gui_console[index].background = gui_console_bg;
        gui_console_col++;
    }

    if (gui_console_col >= GUI_COLS)
    {
        gui_console_col = 0;
        gui_console_row++;
    }

    while (gui_console_row >= GUI_ROWS)
    {
        gui_scroll_console();
        gui_console_row = GUI_ROWS - 1;
    }

    gui_render_frame();
}

void gui_set_console_color(uint8_t foreground, uint8_t background)
{
    gui_console_fg = foreground;
    gui_console_bg = background;
}

int gui_is_active(void)
{
    return gui_active;
}

void enable_gui(void)
{
    gui_active = 1;
    gui_cursor_visible = 1;
    gui_console_clear();
}

void enable_cursor(void)
{
    gui_cursor_visible = 1;
    gui_render_frame();
}

void gui_enter(void)
{
    gui_active = 1;
    gui_cursor_visible = 1;
    gui_console_clear();
}
