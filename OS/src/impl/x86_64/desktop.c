#include "desktop.h"
#include "gui.h"

#define SCREEN_W 320
#define SCREEN_H 100


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
    /*Fond d'écran*/
    fill_rect(0,0,SCREEN_W,SCREEN_H,0xFF2D5EA8);

    /*simple gradient*/
    fill_rect(0,0,SCREEN_W,60,0xFF4A90E2);

    /*icon de Desktop*/
    fill_rect(20,20,16,16,0xFFFFFF00);
    draw_rect_outline(20,20,16,16,0xFFFFFFFF);
    draw_string(18,40,"TERM",0xFFFFFFFF);

    taskbar_render();

    mouse_render();

}

void taskbar_render(void)
{
    fill_rect(0,180,SCREEN_W,20,0xFF202020);

    /* Start button */
    fill_rect(4,183,42,14,0xFF0078D7);
    draw_rect_outline(4,183,42,14,0xFFFFFFFF);
    draw_string(10,186,"START",0xFFFFFFFF);

    draw_string(250,186,"OrangeTeaOS",0xFFFFFFFF);
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
    desktop_render();
}