#include "terminal.h"
#include "ORgui.h"
#include "imp.h"
#include "shell.h"

#define TERMINAL_MAX_LINES 128
#define TERMINAL_LINE_SIZE 128
#define TERMINAL_CHAR_WIDTH 6
#define TERMINAL_LINE_HEIGHT 8
#define TERMINAL_PROMPT "ORTOS> "

static ORWindow *terminal_window;
static char terminal_lines[TERMINAL_MAX_LINES][TERMINAL_LINE_SIZE];
static int terminal_line_count;
static char terminal_input[TERMINAL_LINE_SIZE];
static int terminal_input_length;

static void terminal_new_line(void)
{
    if (terminal_line_count < TERMINAL_MAX_LINES)
    {
        terminal_lines[terminal_line_count++][0] = '\0';
        return;
    }
    for (int line = 1; line < TERMINAL_MAX_LINES; line++)
        for (int column = 0; column < TERMINAL_LINE_SIZE; column++)
            terminal_lines[line - 1][column] = terminal_lines[line][column];
    terminal_lines[TERMINAL_MAX_LINES - 1][0] = '\0';
}

static void terminal_put_char(char character)
{
    if (character == '\n') { terminal_new_line(); return; }
    if (terminal_line_count == 0) terminal_new_line();
    int length = 0;
    while (terminal_lines[terminal_line_count - 1][length] != '\0') length++;
    if (character == '\b')
    {
        if (length > 0) terminal_lines[terminal_line_count - 1][length - 1] = '\0';
        return;
    }
    if (length < TERMINAL_LINE_SIZE - 1)
    {
        terminal_lines[terminal_line_count - 1][length] = character;
        terminal_lines[terminal_line_count - 1][length + 1] = '\0';
    }
}

static void terminal_put_text(const char *text)
{
    for (int index = 0; text[index] != '\0'; index++)
        terminal_put_char(text[index]);
}

static void terminal_clear(void)
{
    terminal_line_count = 0;
    terminal_new_line();
}

static int terminal_backend_active(void)
{
    return terminal_window != 0 && terminal_window->visible;
}

static void terminal_backend_set_color(uint8_t foreground, uint8_t background)
{
    (void)foreground;
    (void)background;
}

static const imp_backend_t terminal_backend = {
    terminal_backend_active, terminal_clear, terminal_put_char,
    terminal_backend_set_color
};

static void terminal_draw(ORWindow *window)
{
    int left = window->x + 8;
    int top = window->y + 28;
    int width = window->width - 16;
    int height = window->height - 36;
    int columns = width / TERMINAL_CHAR_WIDTH;
    int rows = height / TERMINAL_LINE_HEIGHT;
    int first_line = terminal_line_count - rows;
    if (first_line < 0) first_line = 0;

    ORgui_draw_panel(window->x + 4, window->y + 25, window->width - 8,
                     window->height - 29, 0x080808);
    int y = top;
    for (int line = first_line; line < terminal_line_count && y < top + height; line++)
    {
        char visible[TERMINAL_LINE_SIZE];
        int column = 0;
        while (terminal_lines[line][column] != '\0' && column < columns &&
               column < TERMINAL_LINE_SIZE - 1)
            visible[column] = terminal_lines[line][column++];
        visible[column] = '\0';
        ORgui_draw_text(left, y, visible, OR_COLOR_TEXT);
        y += TERMINAL_LINE_HEIGHT;
    }

    int input_y = top + (terminal_line_count - first_line) * TERMINAL_LINE_HEIGHT;
    if (input_y >= top + height) input_y = top + height - TERMINAL_LINE_HEIGHT;
    ORgui_draw_text(left, input_y, TERMINAL_PROMPT, OR_COLOR_FIRE_YELLOW);
    ORgui_draw_text(left + 7 * TERMINAL_CHAR_WIDTH, input_y,
                    terminal_input, OR_COLOR_FIRE_YELLOW);
    ORgui_draw_text(left + (7 + terminal_input_length) * TERMINAL_CHAR_WIDTH,
                    input_y, "_", OR_COLOR_FIRE_YELLOW);
}

static void terminal_event(ORWindow *window, const OREvent *event)
{
    (void)window;
    if (event->type != OR_EVENT_KEY_DOWN) return;
    if (event->key == '\b')
    {
        if (terminal_input_length > 0)
            terminal_input[--terminal_input_length] = '\0';
        return;
    }
    if (event->key == '\n')
    {
        terminal_put_text(TERMINAL_PROMPT);
        terminal_put_text(terminal_input);
        terminal_put_char('\n');
        shell_execute_line(terminal_input);
        terminal_input_length = 0;
        terminal_input[0] = '\0';
        return;
    }
    if (event->key >= 32 && event->key < 127 &&
        terminal_input_length < TERMINAL_LINE_SIZE - 1)
    {
        terminal_input[terminal_input_length++] = (char)event->key;
        terminal_input[terminal_input_length] = '\0';
    }
}

void terminal_init(void)
{
    terminal_window = ORgui_create_window(40, 48, 640, 400, "ORTOS TERMINAL");
    if (terminal_window == 0) return;
    terminal_window->on_draw = terminal_draw;
    terminal_window->on_event = terminal_event;
    terminal_input_length = 0;
    terminal_input[0] = '\0';
    terminal_clear();
    imp_set_backend(&terminal_backend);
    shell_init();
}
