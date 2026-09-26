#include "desktop_apps.h"

#include "framebuffer.h"
#include "process.h"
#include "vfs.h"
#include "timer.h"
#include "terminal.h"
#include "ui_animation.h"

#define TASK_LABEL_SIZE 24
#define TASK_COMMAND_SIZE 32
#define TASK_LIMIT 8
#define MENU_ANIMATION_MS 150u
#define NOTIFICATION_ANIMATION_MS 180u
#define HOVER_ANIMATION_MS 110u

typedef struct {
    int entry;
    char label[TASK_LABEL_SIZE];
    char command[TASK_COMMAND_SIZE];
} DesktopTask;

static DesktopTask desktop_tasks[TASK_LIMIT];
static int desktop_task_count;
static DesktopTask menu_tasks[TASK_LIMIT];
static int menu_task_count;
static DesktopTask panel_tasks[TASK_LIMIT];
static int panel_task_count;
static int menu_open;
static char notification_name[TASK_LABEL_SIZE];
static char notification_message[96];
static uint64_t notification_until;
static uint64_t notification_animation_started;
static uint64_t notification_hide_started;
static uint64_t menu_animation_started;
static uint64_t hover_started;
static int notification_entering;
static int notification_hiding;
static int menu_animating;
static int hover_target = -1;
static int pointer_x;
static int pointer_y;

static int point_in(int x, int y, int left, int top, int width, int height)
{
    return x >= left && y >= top && x < left + width && y < top + height;
}

static int text_equal(const char *left, const char *right)
{
    int index = 0;
    while (left[index] && right[index] && left[index] == right[index]) index++;
    return left[index] == right[index];
}

static int text_starts(const char *text, const char *prefix)
{
    int index = 0;
    while (prefix[index]) {
        if (text[index] != prefix[index]) return 0;
        index++;
    }
    return 1;
}

static void text_copy(char *out, const char *in, int size)
{
    int index = 0;
    if (size <= 0) return;
    while (in && in[index] && index + 1 < size) {
        out[index] = in[index];
        index++;
    }
    out[index] = '\0';
}

static void task_label(char *out, const char *name)
{
    text_copy(out, name, TASK_LABEL_SIZE);
    int length = 0;
    while (out[length]) length++;
    if (length > 5 && text_equal(out + length - 5, ".task")) out[length - 5] = '\0';
}

static void task_command(char *out, const char *content, const char *label)
{
    const char *cursor = content;
    out[0] = '\0';
    while (cursor && *cursor) {
        if (text_starts(cursor, "command=")) {
            text_copy(out, cursor + 8, TASK_COMMAND_SIZE);
            return;
        }
        while (*cursor && *cursor != '\n') cursor++;
        while (*cursor == '\n') cursor++;
    }
    text_copy(out, label, TASK_COMMAND_SIZE);
}

static int task_matches(const char *path, const char *prefix)
{
    int length = 0;
    while (path[length]) length++;
    return length > 5 && text_equal(path + length - 5, ".task") && text_starts(path, prefix);
}

static int collect_tasks(const char *prefix, DesktopTask *tasks)
{
    int count = 0;
    int prefix_length = 0;
    while (prefix[prefix_length]) prefix_length++;
    for (int index = 0; index < vfs_entry_count() && count < TASK_LIMIT; index++) {
        struct storage_entry_info info;
        if (vfs_readdir(index, &info) != STORAGE_OK || info.type != 'f' || !task_matches(info.path, prefix)) continue;
        int slash = prefix_length;
        while (info.path[slash] && info.path[slash] != '/') slash++;
        if (info.path[slash] == '/') continue;
        char content[256] = {0};
        int fd = vfs_open(info.path, 0);
        if (fd < 0) continue;
        int length = vfs_read(fd, content, sizeof(content) - 1);
        vfs_close(fd);
        if (length < 0) continue;
        content[length] = 0;
        tasks[count].entry = index;
        task_label(tasks[count].label, info.name);
        task_command(tasks[count].command, content, tasks[count].label);
        count++;
    }
    return count;
}

static void explorer_draw(ORWindow *window)
{
    ORgui_draw_text(window->x + 10, window->y + 32, "Location: C:/", OR_COLOR_FIRE_RED);
    int row = window->y + 52;
    for (int index = 0; index < vfs_entry_count() && row < window->y + window->height - 12; index++) {
        struct storage_entry_info info;
        if (vfs_readdir(index, &info) != STORAGE_OK) continue;
        const char *path = info.path;
        if (text_starts(path, "/C:/")) {
            const char *child = path + 4;
            int direct_child = 1;
            for (const char *cursor = child; *cursor; cursor++)
                if (*cursor == '/') direct_child = 0;
            if (!direct_child) continue;
            ORgui_draw_text(window->x + 14, row,
                            info.type == 'd' ? "[DIR]" : "[FILE]",
                            OR_COLOR_FIRE_ORANGE);
            ORgui_draw_text(window->x + 62, row, info.name, OR_COLOR_FIRE_RED);
            row += 16;
        }
    }
}

static void settings_draw(ORWindow *window)
{
    ORgui_draw_text(window->x + 14, window->y + 36, "System settings", OR_COLOR_FIRE_RED);
    ORgui_draw_text(window->x + 14, window->y + 62, "Display", OR_COLOR_FIRE_ORANGE);
    ORgui_draw_text(window->x + 112, window->y + 62, "Framebuffer / ORgui", OR_COLOR_FIRE_RED);
    ORgui_draw_text(window->x + 14, window->y + 86, "Input", OR_COLOR_FIRE_ORANGE);
    ORgui_draw_text(window->x + 112, window->y + 86, "Keyboard + PS/2 mouse", OR_COLOR_FIRE_RED);
    ORgui_draw_text(window->x + 14, window->y + 110, "Storage", OR_COLOR_FIRE_ORANGE);
    ORgui_draw_text(window->x + 112, window->y + 110, "Mounted VFS", OR_COLOR_FIRE_RED);
}

static void generic_draw(ORWindow *window)
{
    ORgui_draw_text(window->x + 14, window->y + 42, "Task launched from the ORT desktop.", OR_COLOR_FIRE_RED);
    ORgui_draw_text(window->x + 14, window->y + 62, "Managed by ORgui.", OR_COLOR_BORDER);
}

static ORWindow *launch_task(const DesktopTask *task)
{
    int width = fb_width();
    int height = fb_height();
    ORWindow *window;
    if (text_equal(task->command, "OExplorer")) {
        window = ORgui_create_window(width / 2 - 270, height / 2 - 190, 540, 360, "OExplorer");
        if (window) window->on_draw = explorer_draw;
    } else if (text_equal(task->command, "setting")) {
        window = ORgui_create_window(width / 2 - 210, height / 2 - 145, 420, 290, "setting");
        if (window) window->on_draw = settings_draw;
    } else if (text_equal(task->command, "browser")) {
        window = ORgui_create_window(width / 2 - 330, height / 2 - 220, 660, 440, "browser");
        if (window) browser_window_open(window);
    } else if (text_equal(task->command, "terminal")) {
        window = terminal_open();
    } else {
        window = ORgui_create_window(width / 2 - 220, height / 2 - 110, 440, 220, task->label);
        if (window) window->on_draw = generic_draw;
    }
    if (window) process_create(1);
    return window;
}

void desktop_apps_init(void)
{
    (void)desktop_storage_init();
    desktop_task_count = collect_tasks("/C:/Users/admin/Desktop/", desktop_tasks);
    menu_task_count = collect_tasks("/C:/ProgramData/ORTos/Menu/", menu_tasks);
    panel_task_count = collect_tasks("/C:/ProgramData/ORTos/Taskbar/", panel_tasks);
    menu_open = 0;
    notification_until = 0;
    notification_entering = notification_hiding = menu_animating = 0;
    hover_target = -1;
    pointer_x = pointer_y = 0;
}

int desktop_apps_draw_taskbar(int x, int y, int available_width)
{
    for (int index = 0; index < panel_task_count; index++) {
        if (x + 110 > available_width) break;
        int hovered = hover_target == 1000 + index;
        int progress = hovered ? ui_animation_progress(hover_started, HOVER_ANIMATION_MS,
                                                       UI_ANIMATION_EASE_OUT) : 0;
        uint32_t fill = ui_animation_mix_color(OR_COLOR_WINDOW, OR_COLOR_FIRE_YELLOW, progress / 4);
        fb_fill_rect(x, y, 110, 18, fill);
        fb_draw_rect(x, y, 110, 18, hovered ? OR_COLOR_FIRE_ORANGE : OR_COLOR_BORDER);
        ORgui_draw_text(x + 8, y + 7, panel_tasks[index].label, OR_COLOR_FIRE_RED);
        x += 114;
    }
    return x;
}

void desktop_apps_draw(void)
{
    int height = fb_height();
    int width = fb_width();
    for (int index = 0; index < desktop_task_count; index++) {
        int x = 18 + (index % 4) * 100;
        int y = 58 + (index / 4) * 72;
        int hovered = hover_target == index;
        int progress = hovered ? ui_animation_progress(hover_started, HOVER_ANIMATION_MS,
                                                       UI_ANIMATION_EASE_OUT) : 0;
        uint32_t icon_fill = ui_animation_mix_color(OR_COLOR_PANEL, OR_COLOR_TITLEBAR, progress / 3);
        fb_fill_rect(x, y, 56, 38, icon_fill);
        fb_draw_rect(x, y, 56, 38, hovered ? OR_COLOR_FIRE_YELLOW : OR_COLOR_FIRE_ORANGE);
        ORgui_draw_text(x + 9, y + 14, "APP", OR_COLOR_FIRE_YELLOW);
        ORgui_draw_text(x - 2, y + 52, desktop_tasks[index].label, OR_COLOR_TEXT);
    }
    if (menu_open || menu_animating) {
        int menu_height = 42 + menu_task_count * 26;
        int menu_y = height - 30 - menu_height;
        if (menu_y < 38) menu_y = 38;
        int progress = menu_open ? ui_animation_progress(menu_animation_started,
                         MENU_ANIMATION_MS, UI_ANIMATION_EASE_OUT) :
                         1000 - ui_animation_progress(menu_animation_started,
                         MENU_ANIMATION_MS, UI_ANIMATION_EASE_OUT);
        menu_y += (1000 - progress) * 10 / 1000;
        fb_fill_rect(6, menu_y, 260, menu_height, OR_COLOR_WINDOW);
        fb_draw_rect(6, menu_y, 260, menu_height, OR_COLOR_FIRE_ORANGE);
        ORgui_draw_text(18, menu_y + 18, "Applications", OR_COLOR_FIRE_RED);
        for (int index = 0; index < menu_task_count; index++) {
            int row_y = menu_y + 32 + index * 26;
            if (menu_open && point_in(pointer_x, pointer_y, 12, row_y - 7, 248, 22))
                fb_fill_rect(12, row_y - 7, 248, 22, OR_COLOR_FIRE_YELLOW);
            ORgui_draw_text(20, menu_y + 44 + index * 26, menu_tasks[index].label, OR_COLOR_FIRE_RED);
        }
    }
    if (notification_until > 0) {
        int x = width - 310;
        int progress = 1000;
        if (notification_entering)
            progress = ui_animation_progress(notification_animation_started,
                         NOTIFICATION_ANIMATION_MS, UI_ANIMATION_EASE_OUT);
        else if (notification_hiding)
            progress = 1000 - ui_animation_progress(notification_hide_started,
                         NOTIFICATION_ANIMATION_MS, UI_ANIMATION_EASE_IN);
        int y = 48 - (1000 - progress) * 12 / 1000;
        uint32_t fill = ui_animation_mix_color(OR_COLOR_BACKGROUND, OR_COLOR_WINDOW, progress);
        uint32_t title = ui_animation_mix_color(OR_COLOR_BACKGROUND, OR_COLOR_FIRE_RED, progress);
        fb_fill_rect(x, y, 294, 74, fill);
        fb_draw_rect(x, y, 294, 74,
                     ui_animation_mix_color(OR_COLOR_BACKGROUND, OR_COLOR_FIRE_ORANGE, progress));
        ORgui_draw_text(x + 12, y + 18, notification_name, title);
        ORgui_draw_text(x + 12, y + 42, notification_message, title);
    }
}

void desktop_apps_update_animations(void)
{
    uint64_t now = timer_get_ticks();
    if (menu_animating && !ui_animation_running(menu_animation_started, MENU_ANIMATION_MS))
        menu_animating = 0;
    if (notification_entering &&
        !ui_animation_running(notification_animation_started, NOTIFICATION_ANIMATION_MS))
        notification_entering = 0;
    if (notification_until && now >= notification_until && !notification_hiding) {
        notification_hiding = 1;
        notification_hide_started = now;
    }
    if (notification_hiding &&
        !ui_animation_running(notification_hide_started, NOTIFICATION_ANIMATION_MS)) {
        notification_until = 0;
        notification_hiding = 0;
    }
}

int desktop_apps_animations_active(void)
{
    return menu_animating || notification_entering || notification_hiding ||
           (hover_target >= 0 && ui_animation_running(hover_started, HOVER_ANIMATION_MS));
}

int desktop_apps_handle_event(const OREvent *event)
{
    if (!event) return 0;
    int height = fb_height();
    if (event->type == OR_EVENT_MOUSE_MOVE) {
        pointer_x = event->x;
        pointer_y = event->y;
        int target = -1;
        for (int index = 0; index < desktop_task_count; index++) {
            int x = 18 + (index % 4) * 100;
            int y = 58 + (index / 4) * 72;
            if (point_in(event->x, event->y, x, y, 56, 62)) target = index;
        }
        if (event->y >= height - 28 && event->x >= 74) {
            for (int index = 0; index < panel_task_count; index++)
                if (point_in(event->x, event->y, 74 + index * 114, height - 28, 110, 28))
                    target = 1000 + index;
            int window_x = desktop_apps_draw_taskbar(74, height - 23, fb_width());
            for (int index = 0; index < ORGUI_MAX_WINDOWS; index++) {
                ORWindow *window = ORgui_window_at(index);
                if (!window || !window->visible) continue;
                if (point_in(event->x, event->y, window_x, height - 28, 110, 28))
                    target = 2000 + index;
                window_x += 114;
            }
        }
        if (menu_open) {
            int menu_height = 42 + menu_task_count * 26;
            int menu_y = height - 30 - menu_height;
            if (menu_y < 38) menu_y = 38;
            for (int index = 0; index < menu_task_count; index++)
                if (point_in(event->x, event->y, 12, menu_y + 25 + index * 26, 248, 22))
                    target = 3000 + index;
        }
        if (target != hover_target) {
            hover_target = target;
            hover_started = timer_get_ticks();
            return 2;
        }
        return 0;
    }
    if (event->type != OR_EVENT_MOUSE_DOWN || event->button != 1) return 0;
    if (event->y >= height - 28 && event->x >= 7 && event->x < 65) {
        menu_open = !menu_open;
        menu_animating = 1;
        menu_animation_started = timer_get_ticks();
        return 1;
    }
    if (menu_open) {
        int menu_height = 42 + menu_task_count * 26;
        int menu_y = height - 30 - menu_height;
        if (menu_y < 38) menu_y = 38;
        if (event->x >= 6 && event->x < 266 && event->y >= menu_y + 28 && event->y < menu_y + menu_height) {
            int index = (event->y - menu_y - 28) / 26;
            if (index >= 0 && index < menu_task_count) launch_task(&menu_tasks[index]);
            menu_open = 0;
            menu_animating = 1;
            menu_animation_started = timer_get_ticks();
            return 1;
        }
        menu_open = 0;
        menu_animating = 1;
        menu_animation_started = timer_get_ticks();
    }
    for (int index = 0; index < desktop_task_count; index++) {
        int x = 18 + (index % 4) * 100;
        int y = 58 + (index / 4) * 72;
        if (event->x >= x && event->x < x + 56 && event->y >= y && event->y < y + 62) {
            launch_task(&desktop_tasks[index]);
            return 1;
        }
    }
    int x = desktop_apps_draw_taskbar(74, height - 23, fb_width());
    if (event->y >= height - 28 && event->x >= 74 && event->x < x) {
        int index = (event->x - 74) / 114;
        if (index >= 0 && index < panel_task_count) launch_task(&panel_tasks[index]);
        return 1;
    }
    for (int index = 0; index < ORGUI_MAX_WINDOWS; index++) {
        ORWindow *window = ORgui_window_at(index);
        if (!window || !window->visible) continue;
        if (event->x >= x && event->x < x + 110) {
            ORgui_set_active(window);
            return 1;
        }
        x += 114;
    }
    return 0;
}

void desktop_notify(const char *name, const char *message)
{
    text_copy(notification_name, name, sizeof(notification_name));
    text_copy(notification_message, message, sizeof(notification_message));
    notification_until = timer_get_ticks() + 5000;
    notification_animation_started = timer_get_ticks();
    notification_entering = 1;
    notification_hiding = 0;
}
