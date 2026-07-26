#include "gui.h"
#include "keyboard.h"
#include "imp.h"
#include "network.h"
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
    gui_draw_text(2, 0, "ORT-Desktop", PRINT_COLOR_WHITE + (PRINT_COLOR_BLUE << 4));
    gui_draw_text(2, 23, "[Start] ORT-Shell  Ctrl+T  Help", PRINT_COLOR_WHITE + (PRINT_COLOR_GREEN << 4));
    gui_draw_text(2, 24, "Move: WASD  Quit: Q(cursor coming soon)", PRINT_COLOR_WHITE + (PRINT_COLOR_GREEN << 4));

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
    const char* base = "ORT-Shell";
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
                else if (input[0] == 'w' && input[1] == 'i' && input[2] == 'f' && input[3] == 'i')
                {
                    if (input[4] == ' ')
                    {
                        const char* text = input + 5;
                        if (text[0] == 'c' && text[1] == 'o' && text[2] == 'n' && text[3] == 'n' && text[4] == 'e' && text[5] == 'c' && text[6] == 't' && text[7] == ' ')
                        {
                            const char* ssid = text + 8;
                            if (ssid[0] != '\0')
                            {
                                network_connect_wifi(ssid);
                                gui_draw_rect(gui_state_instance.window_x + 2, gui_state_instance.window_y + 8, gui_state_instance.window_w - 4, 4, ' ', PRINT_COLOR_LIGHT_GRAY + (PRINT_COLOR_BLACK << 4));
                                gui_draw_text(gui_state_instance.window_x + 2, gui_state_instance.window_y + 8, "Wi-Fi connected to ", PRINT_COLOR_LIGHT_GREEN + (PRINT_COLOR_BLACK << 4));
                                gui_draw_text(gui_state_instance.window_x + 26, gui_state_instance.window_y + 8, ssid, PRINT_COLOR_LIGHT_GREEN + (PRINT_COLOR_BLACK << 4));
                            }
                            else
                            {
                                gui_draw_rect(gui_state_instance.window_x + 2, gui_state_instance.window_y + 8, gui_state_instance.window_w - 4, 4, ' ', PRINT_COLOR_LIGHT_GRAY + (PRINT_COLOR_BLACK << 4));
                                gui_draw_text(gui_state_instance.window_x + 2, gui_state_instance.window_y + 8, "Usage: wifi connect <ssid>", PRINT_COLOR_LIGHT_CYAN + (PRINT_COLOR_BLACK << 4));
                            }
                        }
                        else if (text[0] == 'd' && text[1] == 'i' && text[2] == 's' && text[3] == 'c' && text[4] == 'o' && text[5] == 'n' && text[6] == 'n' && text[7] == 'e' && text[8] == 'c' && text[9] == 't' && text[10] == '\0')
                        {
                            network_disconnect_wifi();
                            gui_draw_rect(gui_state_instance.window_x + 2, gui_state_instance.window_y + 8, gui_state_instance.window_w - 4, 4, ' ', PRINT_COLOR_LIGHT_GRAY + (PRINT_COLOR_BLACK << 4));
                            gui_draw_text(gui_state_instance.window_x + 2, gui_state_instance.window_y + 8, "Wi-Fi disconnected", PRINT_COLOR_LIGHT_GREEN + (PRINT_COLOR_BLACK << 4));
                        }
                        else if (text[0] == 's' && text[1] == 't' && text[2] == 'a' && text[3] == 't' && text[4] == 'u' && text[5] == 's' && text[6] == '\0')
                        {
                            gui_draw_rect(gui_state_instance.window_x + 2, gui_state_instance.window_y + 8, gui_state_instance.window_w - 4, 4, ' ', PRINT_COLOR_LIGHT_GRAY + (PRINT_COLOR_BLACK << 4));
                            if (network_is_wifi_connected())
                            {
                                gui_draw_text(gui_state_instance.window_x + 2, gui_state_instance.window_y + 8, "Wi-Fi connected to ", PRINT_COLOR_LIGHT_GREEN + (PRINT_COLOR_BLACK << 4));
                                gui_draw_text(gui_state_instance.window_x + 26, gui_state_instance.window_y + 8, network_get_wifi_ssid(), PRINT_COLOR_LIGHT_GREEN + (PRINT_COLOR_BLACK << 4));
                            }
                            else
                            {
                                gui_draw_text(gui_state_instance.window_x + 2, gui_state_instance.window_y + 8, "Wi-Fi disconnected", PRINT_COLOR_LIGHT_GREEN + (PRINT_COLOR_BLACK << 4));
                            }
                        }
                        else
                        {
                            gui_draw_rect(gui_state_instance.window_x + 2, gui_state_instance.window_y + 8, gui_state_instance.window_w - 4, 4, ' ', PRINT_COLOR_LIGHT_GRAY + (PRINT_COLOR_BLACK << 4));
                            gui_draw_text(gui_state_instance.window_x + 2, gui_state_instance.window_y + 8, "Usage: wifi connect <ssid> | disconnect | status", PRINT_COLOR_LIGHT_CYAN + (PRINT_COLOR_BLACK << 4));
                        }
                    }
                    else
                    {
                        gui_draw_rect(gui_state_instance.window_x + 2, gui_state_instance.window_y + 8, gui_state_instance.window_w - 4, 4, ' ', PRINT_COLOR_LIGHT_GRAY + (PRINT_COLOR_BLACK << 4));
                        gui_draw_text(gui_state_instance.window_x + 2, gui_state_instance.window_y + 8, "Usage: wifi connect <ssid> | disconnect | status", PRINT_COLOR_LIGHT_CYAN + (PRINT_COLOR_BLACK << 4));
                    }
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

