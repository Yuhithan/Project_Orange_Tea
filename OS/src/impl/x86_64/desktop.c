#include "desktop.h"
#include "gui.h"

#define SCREEN_W 320
#define SCREEN_H 200


static int mouse_x = 160;
static int mouse_y = 100;

static int left_button = 0;
static int right_button = 0;
static int mouse_wheel = 0;

void desktop_init(void)
{
    mouse_x = SCREEN_W / 2;
    mouse_y = SCREEN_H / 2;
}

void desktop_render(void)
{
    fill_rect(0, 0, SCREEN_W, SCREEN_H, 0xFF7FA8D7);
    fill_rect(0, 0, SCREEN_W, 40, 0xFF0F60B6);

    fill_rect(20, 24, 16, 16, 0xFFFFFF00);
    draw_rect_outline(20, 24, 16, 16, 0xFFFFFFFF);
    draw_string(18, 44, "TERM", 0xFFFFFFFF);

    fill_rect(20, 72, 16, 16, 0xFFB3D5FF);
    draw_rect_outline(20, 72, 16, 16, 0xFF0F60B6);
    draw_string(14, 92, "FILES", 0xFFFFFFFF);

    fill_rect(80, 24, 70, 58, 0xFFFAFAFA);
    draw_rect_outline(80, 24, 70, 58, 0xFF808080);
    fill_rect(81, 25, 68, 16, 0xFF0F60B6);
    draw_string(88, 29, "OrangeTeaOS", 0x00FFFFFF);
    draw_string(88, 46, "Welcome", 0x00000000);
    draw_string(88, 58, "to the desktop", 0x00000000);

    taskbar_render();
    mouse_render();
}

void taskbar_render(void)
{
    fill_rect(0, 176, SCREEN_W, 24, 0xFFC0C0C0);
    draw_rect_outline(0, 176, SCREEN_W, 24, 0xFF808080);

    fill_rect(4, 180, 44, 14, 0xFF004E98);
    draw_rect_outline(4, 180, 44, 14, 0xFFFFFFFF);
    draw_string(10, 183, "START", 0x00FFFFFF);

    draw_string(240, 183, "OrangeTeaOS", 0x00000000);
}

void mouse_render(void)
{
    /* White arrow */

    put_pixel(mouse_x,mouse_y,0xFFFFFFFF);

    put_pixel(mouse_x,mouse_y+1,0xFFFFFFFF);
    put_pixel(mouse_x+1,mouse_y+1,0xFFFFFFFF);

    put_pixel(mouse_x,mouse_y+2,0xFFFFFFFF);
    put_pixel(mouse_x+1,mouse_y+2,0xFFFFFFFF);
    put_pixel(mouse_x+2,mouse_y+2,0xFFFFFFFF);

    put_pixel(mouse_x,mouse_y+3,0xFFFFFFFF);
    put_pixel(mouse_x+1,mouse_y+3,0xFFFFFFFF);

    put_pixel(mouse_x,mouse_y+4,0xFFFFFFFF);

    /* Black outline */

    put_pixel(mouse_x+1,mouse_y,0xFF000000);
    put_pixel(mouse_x+2,mouse_y+1,0xFF000000);
    put_pixel(mouse_x+3,mouse_y+2,0xFF000000);
}

void mouse_update_position(int x,int y)
{
    mouse_x += x;
    mouse_y += y;

    if(mouse_x < 0)
        mouse_x = 0;

    if(mouse_y < 0)
        mouse_y = 0;

    if(mouse_x >= SCREEN_W)
        mouse_x = SCREEN_W-1;

    if(mouse_y >= SCREEN_H)
        mouse_y = SCREEN_H-1;
}

void mouse_update_button_state(int button,int pressed)
{
    if(button == 0)
        left_button = pressed;

    if(button == 1)
        right_button = pressed;
}

void mouse_update_wheel(int delta)
{
    mouse_wheel += delta;
}

void desktop_enter(void)
{
    enable_gui();
    gui_set_desktop_mode(1);
    desktop_render();
}