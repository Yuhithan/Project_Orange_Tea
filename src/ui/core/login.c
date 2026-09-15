#include "login.h"
#include "ORgui.h"
#include "framebuffer.h"
#include "keyboard.h"

#define LOGIN_FIELD_LENGTH 31

static char login_user[LOGIN_FIELD_LENGTH + 1];
static char login_password[LOGIN_FIELD_LENGTH + 1];
static int login_user_length;
static int login_password_length;
static int login_field;
static int login_message;

static int login_equals(const char *value, const char *expected)
{
    int index = 0;
    while (value[index] != '\0' && expected[index] != '\0') {
        if (value[index] != expected[index]) return 0;
        index++;
    }
    return value[index] == '\0' && expected[index] == '\0';
}

static void login_backspace(void)
{
    char *value = login_field == 0 ? login_user : login_password;
    int *length = login_field == 0 ? &login_user_length : &login_password_length;
    if (*length == 0) return;
    (*length)--;
    value[*length] = '\0';
}

static void login_append(int character)
{
    char *value = login_field == 0 ? login_user : login_password;
    int *length = login_field == 0 ? &login_user_length : &login_password_length;
    if (character < 32 || character > 126 || *length >= LOGIN_FIELD_LENGTH) return;
    value[*length] = (char)character;
    (*length)++;
    value[*length] = '\0';
}

static void login_draw_field(int x, int y, const char *label, const char *value,
                             int length, int active, int password)
{
    char display[LOGIN_FIELD_LENGTH + 1];
    for (int index = 0; index < length; index++)
        display[index] = password ? '*' : value[index];
    display[length] = '\0';

    ORgui_draw_text(x, y, label, OR_COLOR_FIRE_YELLOW);
    fb_fill_rect(x, y + 12, 240, 24, OR_COLOR_WINDOW);
    fb_draw_rect(x, y + 12, 240, 24,
                 active ? OR_COLOR_FIRE_YELLOW : OR_COLOR_BORDER);
    ORgui_draw_text(x + 8, y + 21, display, OR_COLOR_FIRE_RED);
}

static void login_draw(void)
{
    int width = fb_width();
    int height = fb_height();
    int panel_width = 330;
    int panel_height = 270;
    int panel_x = (width - panel_width) / 2;
    int panel_y = (height - panel_height) / 2;
    if (panel_x < 8) panel_x = 8;
    if (panel_y < 8) panel_y = 8;

    fb_clear(OR_COLOR_BACKGROUND);
    fb_fill_rect(0, 0, width, 34, OR_COLOR_PANEL);
    fb_draw_line(0, 33, width - 1, 33, OR_COLOR_FIRE_RED);
    ORgui_draw_text(14, 14, "ORTOS DESKTOP - LOGIN", OR_COLOR_FIRE_YELLOW);

    ORgui_draw_panel(panel_x, panel_y, panel_width, panel_height, OR_COLOR_WINDOW);
    ORgui_draw_text(panel_x + 45, panel_y + 25, "WELCOME TO ORANGE TEA", OR_COLOR_FIRE_RED);
    ORgui_draw_text(panel_x + 45, panel_y + 43, "SIGN IN TO CONTINUE", OR_COLOR_BORDER);
    login_draw_field(panel_x + 45, panel_y + 70, "USERNAME", login_user,
                     login_user_length, login_field == 0, 0);
    login_draw_field(panel_x + 45, panel_y + 125, "PASSWORD", login_password,
                     login_password_length, login_field == 1, 1);
    ORgui_draw_button(panel_x + 45, panel_y + 190, 240, 28, "LOGIN", 0);
    if (login_message)
        ORgui_draw_text(panel_x + 45, panel_y + 235, "INVALID LOGIN", OR_COLOR_FIRE_RED);
    else
        ORgui_draw_text(panel_x + 45, panel_y + 235, "ENTER TO SUBMIT", OR_COLOR_BORDER);
    fb_flush();
}

int login_run(void)
{
    login_user[0] = '\0';
    login_password[0] = '\0';
    login_user_length = 0;
    login_password_length = 0;
    login_field = 0;
    login_message = 0;
    login_draw();

    for (;;) {
        int key;
        while (keyboard_try_getchar(&key)) {
            if (key == 27) return 0;
            if (key == '\b') {
                login_backspace();
                login_message = 0;
            } else if (key == '\n' || key == '\r') {
                if (login_field == 0) {
                    login_field = 1;
                } else if (login_equals(login_user, "s") &&
                           login_equals(login_password, "s")) {
                    return 1;
                } else {
                    login_message = 1;
                    login_password[0] = '\0';
                    login_password_length = 0;
                    login_field = 1;
                }
            } else {
                login_append(key);
                login_message = 0;
            }
            login_draw();
        }
    }
}
