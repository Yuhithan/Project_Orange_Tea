#include "gui.h"
#include "keyboard.h"
#include "imp.h"
#include <stdint.h>
#include <stddef.h>

#define GUI_WIDTH 80
#define GUI_HEIGHT 25

struct gui_state {
    int enabled;
    int window_x;
    int window_y;
    int window_w;
    int window_h;
};

static struct gui_state gui_state_instance = {
    .enabled = 0,
    .window_x = 8,
    .window_y = 3,
    .window_w = 48,
    .window_h = 12,
};

static volatile char* const video_buffer = (volatile char*)0xb8000;

static void gui_draw_cell(int x, int y, char ch, uint8_t color)
{
    if (x < 0 || y < 0 || x >= GUI_WIDTH || y >= GUI_HEIGHT)
    {
        return;
    }

    int index = (y * GUI_WIDTH + x) * 2;
    video_buffer[index] = (char)ch;
    video_buffer[index + 1] = (char)color;
}

static void gui_draw_rect(int x, int y, int w, int h, char ch, uint8_t color)
{
    for (int row = 0; row < h; row++)
    {
        for (int col = 0; col < w; col++)
        {
            gui_draw_cell(x + col, y + row, ch, color);
        }
    }
}

static void gui_draw_text(int x, int y, const char* text, uint8_t color)
{
    int i = 0;
    while (text[i] != '\0')
    {
        gui_draw_cell(x + i, y, text[i], color);
        i++;
    }
}

static void gui_draw_desktop(void)
{
    gui_draw_rect(0, 0, GUI_WIDTH, GUI_HEIGHT, ' ', PRINT_COLOR_LIGHT_BLUE + (PRINT_COLOR_BLUE << 4));
    gui_draw_rect(0, 23, GUI_WIDTH, 2, ' ', PRINT_COLOR_BLACK + (PRINT_COLOR_GREEN << 4));
    gui_draw_text(2, 0, "Orange Tea OS Desktop", PRINT_COLOR_WHITE + (PRINT_COLOR_BLUE << 4));
    gui_draw_text(2, 23, "[Start] Terminal  Ctrl+T  Help", PRINT_COLOR_WHITE + (PRINT_COLOR_GREEN << 4));
    gui_draw_text(2, 24, "Move: WASD  Quit: Q", PRINT_COLOR_WHITE + (PRINT_COLOR_GREEN << 4));

    gui_draw_rect(gui_state_instance.window_x, gui_state_instance.window_y,
                  gui_state_instance.window_w, gui_state_instance.window_h,
                  ' ', PRINT_COLOR_LIGHT_GRAY + (PRINT_COLOR_BLACK << 4));
    gui_draw_rect(gui_state_instance.window_x, gui_state_instance.window_y,
                  gui_state_instance.window_w, 1, '=', PRINT_COLOR_CYAN + (PRINT_COLOR_BLACK << 4));
    gui_draw_rect(gui_state_instance.window_x, gui_state_instance.window_y + gui_state_instance.window_h - 1,
                  gui_state_instance.window_w, 1, '-', PRINT_COLOR_CYAN + (PRINT_COLOR_BLACK << 4));
    for (int row = 1; row < gui_state_instance.window_h - 1; row++)
    {
        gui_draw_cell(gui_state_instance.window_x, gui_state_instance.window_y + row, '|', PRINT_COLOR_CYAN + (PRINT_COLOR_BLACK << 4));
        gui_draw_cell(gui_state_instance.window_x + gui_state_instance.window_w - 1, gui_state_instance.window_y + row, '|', PRINT_COLOR_CYAN + (PRINT_COLOR_BLACK << 4));
    }

    char title[32];
    int title_len = 0;
    const char* base = "ORT Terminal";
    while (base[title_len] != '\0' && title_len < 30)
    {
        title[title_len] = base[title_len];
        title_len++;
    }
    title[title_len] = '\0';
    gui_draw_text(gui_state_instance.window_x + 2, gui_state_instance.window_y, title, PRINT_COLOR_WHITE + (PRINT_COLOR_BLACK << 4));

    gui_draw_text(gui_state_instance.window_x + 2, gui_state_instance.window_y + 2, "Welcome to the ORT GUI", PRINT_COLOR_LIGHT_GREEN + (PRINT_COLOR_BLACK << 4));
    gui_draw_text(gui_state_instance.window_x + 2, gui_state_instance.window_y + 4, "Commands: help, date, time, clear, exit", PRINT_COLOR_LIGHT_CYAN + (PRINT_COLOR_BLACK << 4));
    gui_draw_text(gui_state_instance.window_x + 2, gui_state_instance.window_y + 6, "> ", PRINT_COLOR_LIGHT_GRAY + (PRINT_COLOR_BLACK << 4));
}

void enable_gui(void)
{
    gui_state_instance.enabled = 1;
}

void gui_enter(void)
{
    enable_gui();

    char input[48];
    int input_len = 0;
    input[0] = '\0';

    while (1)
    {
        gui_draw_desktop();

        if (input_len > 0)
        {
            gui_draw_text(gui_state_instance.window_x + 4, gui_state_instance.window_y + 6, input, PRINT_COLOR_WHITE + (PRINT_COLOR_BLACK << 4));
        }

        int ch = keyboard_getchar();
        if (ch == '\n')
        {
            if (input_len > 0)
            {
                if (input[0] == 'c' && input[1] == 'l' && input[2] == 'e' && input[3] == 'a' && input[4] == 'r' && input[5] == '\0')
                {
                    input_len = 0;
                    input[0] = '\0';
                    gui_draw_rect(gui_state_instance.window_x + 2, gui_state_instance.window_y + 8, gui_state_instance.window_w - 4, 3, ' ', PRINT_COLOR_LIGHT_GRAY + (PRINT_COLOR_BLACK << 4));
                    gui_draw_text(gui_state_instance.window_x + 2, gui_state_instance.window_y + 8, "Screen cleared", PRINT_COLOR_LIGHT_GREEN + (PRINT_COLOR_BLACK << 4));
                }
                else if (input[0] == 'h' && input[1] == 'e' && input[2] == 'l' && input[3] == 'p' && input[4] == '\0')
                {
                    gui_draw_rect(gui_state_instance.window_x + 2, gui_state_instance.window_y + 8, gui_state_instance.window_w - 4, 4, ' ', PRINT_COLOR_LIGHT_GRAY + (PRINT_COLOR_BLACK << 4));
                    gui_draw_text(gui_state_instance.window_x + 2, gui_state_instance.window_y + 8, "help, date, time, clear, exit", PRINT_COLOR_LIGHT_CYAN + (PRINT_COLOR_BLACK << 4));
                }
                else if (input[0] == 'd' && input[1] == 'a' && input[2] == 't' && input[3] == 'e' && input[4] == '\0')
                {
                    gui_draw_rect(gui_state_instance.window_x + 2, gui_state_instance.window_y + 8, gui_state_instance.window_w - 4, 4, ' ', PRINT_COLOR_LIGHT_GRAY + (PRINT_COLOR_BLACK << 4));
                    gui_draw_text(gui_state_instance.window_x + 2, gui_state_instance.window_y + 8, "2026-07-26", PRINT_COLOR_LIGHT_GREEN + (PRINT_COLOR_BLACK << 4));
                }
                else if (input[0] == 't' && input[1] == 'i' && input[2] == 'm' && input[3] == 'e' && input[4] == '\0')
                {
                    gui_draw_rect(gui_state_instance.window_x + 2, gui_state_instance.window_y + 8, gui_state_instance.window_w - 4, 4, ' ', PRINT_COLOR_LIGHT_GRAY + (PRINT_COLOR_BLACK << 4));
                    gui_draw_text(gui_state_instance.window_x + 2, gui_state_instance.window_y + 8, "12:00:00", PRINT_COLOR_LIGHT_GREEN + (PRINT_COLOR_BLACK << 4));
                }
                else if (input[0] == 'e' && input[1] == 'x' && input[2] == 'i' && input[3] == 't' && input[4] == '\0')
                {
                    break;
                }
                else
                {
                    gui_draw_rect(gui_state_instance.window_x + 2, gui_state_instance.window_y + 8, gui_state_instance.window_w - 4, 4, ' ', PRINT_COLOR_LIGHT_GRAY + (PRINT_COLOR_BLACK << 4));
                    gui_draw_text(gui_state_instance.window_x + 2, gui_state_instance.window_y + 8, "Command executed", PRINT_COLOR_YELLOW + (PRINT_COLOR_BLACK << 4));
                }
            }
            input_len = 0;
            input[0] = '\0';
            continue;
        }

        if (ch == '\b')
        {
            if (input_len > 0)
            {
                input_len--;
                input[input_len] = '\0';
            }
            continue;
        }

        if (ch == 'q' || ch == 'Q')
        {
            break;
        }

        if (ch == 'w' || ch == 'W')
        {
            if (gui_state_instance.window_y > 2)
            {
                gui_state_instance.window_y--;
            }
            continue;
        }

        if (ch == 's' || ch == 'S')
        {
            if (gui_state_instance.window_y + gui_state_instance.window_h < GUI_HEIGHT - 1)
            {
                gui_state_instance.window_y++;
            }
            continue;
        }

        if (ch == 'a' || ch == 'A')
        {
            if (gui_state_instance.window_x > 1)
            {
                gui_state_instance.window_x--;
            }
            continue;
        }

        if (ch == 'd' || ch == 'D')
        {
            if (gui_state_instance.window_x + gui_state_instance.window_w < GUI_WIDTH - 1)
            {
                gui_state_instance.window_x++;
            }
            continue;
        }

        if (ch >= 32 && ch <= 126 && input_len < 47)
        {
            input[input_len++] = (char)ch;
            input[input_len] = '\0';
        }
    }

    imp_cls();
    imp_color(PRINT_COLOR_BLACK, PRINT_COLOR_WHITE);
    imp_text("Returned to shell.\n");
}

