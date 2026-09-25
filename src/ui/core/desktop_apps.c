#include "desktop_apps.h"

#include "framebuffer.h"
#include "process.h"
#include "storage.h"
#include "timer.h"

#define TASK_LABEL_SIZE 24
#define TASK_COMMAND_SIZE 32
#define TASK_LIMIT 8

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
    for (int index = 0; index < storage_get_entry_count() && count < TASK_LIMIT; index++) {
        const char *path = storage_get_entry_path(index);
        if (storage_get_entry_type(index) != 'f' || !task_matches(path, prefix)) continue;
        int slash = prefix_length;
        while (path[slash] && path[slash] != '/') slash++;
        if (path[slash] == '/') continue;
        tasks[count].entry = index;
        task_label(tasks[count].label, storage_get_entry_name(index));
        task_command(tasks[count].command, storage_get_entry_content(index), tasks[count].label);
        count++;
    }
    return count;
}

static void ensure_directory(const char *path)
{
    if (storage_find_entry(path) < 0) storage_mkdir(path);
}

static void ensure_task(const char *path, const char *content)
{
    if (storage_find_entry(path) < 0) storage_create_entry(path, 'f', content);
}

static void ensure_default_tasks(void)
{
    ensure_directory("/C:");
    ensure_directory("/C:/menu");
    ensure_directory("/C:/menu/taskbar");
    ensure_directory("/C:/user");
    ensure_directory("/C:/user/admin");
    ensure_directory("/C:/user/admin/desktop");
    ensure_task("/C:/menu/OExplorer.task", "name=OExplorer\ncommand=OExplorer\ncategory=System");
    ensure_task("/C:/menu/setting.task", "name=setting\ncommand=setting\ncategory=System");
    ensure_task("/C:/menu/browser.task", "name=browser\ncommand=browser\ncategory=Internet");
    ensure_task("/C:/menu/terminal.task", "name=Terminal\ncommand=terminal\ncategory=Utilities");
    ensure_task("/C:/menu/taskbar/OExplorer.task", "name=OExplorer\ncommand=OExplorer");
    ensure_task("/C:/menu/taskbar/setting.task", "name=setting\ncommand=setting");
    ensure_task("/C:/menu/taskbar/browser.task", "name=browser\ncommand=browser");
    ensure_task("/C:/user/admin/desktop/OExplorer.task", "name=OExplorer\ncommand=OExplorer");
    ensure_task("/C:/user/admin/desktop/setting.task", "name=setting\ncommand=setting");
    ensure_task("/C:/user/admin/desktop/browser.task", "name=browser\ncommand=browser");
}

static void explorer_draw(ORWindow *window)
{
    ORgui_draw_text(window->x + 10, window->y + 32, "Location: /", OR_COLOR_FIRE_RED);
    int row = window->y + 52;
    for (int index = 0; index < storage_get_entry_count() && row < window->y + window->height - 12; index++) {
        const char *path = storage_get_entry_path(index);
        if (path[0] == '/' && path[1] && path[2] == '\0') {
            ORgui_draw_text(window->x + 14, row,
                            storage_get_entry_type(index) == 'd' ? "[DIR]" : "[FILE]",
                            OR_COLOR_FIRE_ORANGE);
            ORgui_draw_text(window->x + 62, row, storage_get_entry_name(index), OR_COLOR_FIRE_RED);
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
    } else {
        window = ORgui_create_window(width / 2 - 220, height / 2 - 110, 440, 220, task->label);
        if (window) window->on_draw = generic_draw;
    }
    if (window) process_create(1);
    return window;
}

void desktop_apps_init(void)
{
    ensure_default_tasks();
    desktop_task_count = collect_tasks("/C:/user/admin/desktop/", desktop_tasks);
    menu_task_count = collect_tasks("/C:/menu/", menu_tasks);
    panel_task_count = collect_tasks("/C:/menu/taskbar/", panel_tasks);
    menu_open = 0;
    notification_until = 0;
}

int desktop_apps_draw_taskbar(int x, int y, int available_width)
{
    for (int index = 0; index < panel_task_count; index++) {
        if (x + 110 > available_width) break;
        fb_fill_rect(x, y, 110, 18, OR_COLOR_WINDOW);
        fb_draw_rect(x, y, 110, 18, OR_COLOR_BORDER);
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
        fb_fill_rect(x, y, 56, 38, OR_COLOR_PANEL);
        fb_draw_rect(x, y, 56, 38, OR_COLOR_FIRE_ORANGE);
        ORgui_draw_text(x + 9, y + 14, "APP", OR_COLOR_FIRE_YELLOW);
        ORgui_draw_text(x - 2, y + 52, desktop_tasks[index].label, OR_COLOR_TEXT);
    }
    if (menu_open) {
        int menu_height = 42 + menu_task_count * 26;
        int menu_y = height - 30 - menu_height;
        if (menu_y < 38) menu_y = 38;
        fb_fill_rect(6, menu_y, 260, menu_height, OR_COLOR_WINDOW);
        fb_draw_rect(6, menu_y, 260, menu_height, OR_COLOR_FIRE_ORANGE);
        ORgui_draw_text(18, menu_y + 18, "Applications", OR_COLOR_FIRE_RED);
        for (int index = 0; index < menu_task_count; index++)
            ORgui_draw_text(20, menu_y + 44 + index * 26, menu_tasks[index].label, OR_COLOR_FIRE_RED);
    }
    if (notification_until > timer_get_ticks()) {
        int x = width - 310;
        int y = 48;
        fb_fill_rect(x, y, 294, 74, OR_COLOR_WINDOW);
        fb_draw_rect(x, y, 294, 74, OR_COLOR_FIRE_ORANGE);
        ORgui_draw_text(x + 12, y + 18, notification_name, OR_COLOR_FIRE_RED);
        ORgui_draw_text(x + 12, y + 42, notification_message, OR_COLOR_FIRE_RED);
    }
}

int desktop_apps_handle_event(const OREvent *event)
{
    if (!event || event->type != OR_EVENT_MOUSE_DOWN || event->button != 1) return 0;
    int height = fb_height();
    if (event->y >= height - 28 && event->x >= 7 && event->x < 65) {
        menu_open = !menu_open;
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
            return 1;
        }
        menu_open = 0;
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
}
