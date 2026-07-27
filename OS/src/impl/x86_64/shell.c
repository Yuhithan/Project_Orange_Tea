#include "shell.h"
#include "keyboard.h"
#include "imp.h"
#include "storage.h"
#include "gui.h"
#include "network.h"

#define MAX_CMD 128
#define MAX_HISTORY 16
#define MAX_ALIASES 8

static char cmd[MAX_CMD];
static char history[MAX_HISTORY][MAX_CMD];
static int history_count = 0;
static char aliases[MAX_ALIASES][MAX_CMD];
static int alias_count = 0;
static char current_dir[32] = "/";
static char current_layout[16] = "en-us";
static int shell_seed = 1337;
static int shell_uptime_seconds = 0;
static int shell_clock_second = 0;
static int shell_clock_minute = 0;
static int shell_clock_hour = 12;
static int shell_clock_day = 25;
static int shell_clock_month = 7;
static int shell_clock_year = 2026;

static int shell_streq(const char* a, const char* b)
{
    while (*a != '\0' && *b != '\0')
    {
        if (*a != *b)
        {
            return 0;
        }

        a++;
        b++;
    }

    return *a == *b;
}

static int shell_starts_with(const char* text, const char* prefix)
{
    while (*prefix != '\0')
    {
        if (*text != *prefix)
        {
            return 0;
        }

        text++;
        prefix++;
    }

    return 1;
}

static int shell_is_space(char c)
{
    return c == ' ' || c == '\t';
}

static const char* shell_skip_spaces(const char* text)
{
    while (*text != '\0' && shell_is_space(*text))
    {
        text++;
    }

    return text;
}

static int shell_strlen(const char* text)
{
    int len = 0;
    while (text[len] != '\0')
    {
        len++;
    }
    return len;
}

static void shell_copy_string(char* dst, const char* src, int max_len)
{
    int i = 0;
    while (src[i] != '\0' && i < max_len - 1)
    {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

static const char* shell_read_token(const char* text, char* token, int max_len)
{
    text = shell_skip_spaces(text);
    int i = 0;
    while (*text != '\0' && !shell_is_space(*text) && i < max_len - 1)
    {
        token[i++] = *text++;
    }
    token[i] = '\0';
    return text;
}

static void shell_print_int(int value)
{
    char digits[16];
    int count = 0;
    int negative = 0;

    if (value < 0)
    {
        negative = 1;
        value = -value;
    }

    do
    {
        digits[count++] = (char)('0' + (value % 10));
        value /= 10;
    }
    while (value > 0 && count < 15);

    if (negative)
    {
        imp_char('-');
    }

    while (count > 0)
    {
        imp_char(digits[--count]);
    }
}

static void shell_tick_clock(void)
{
    shell_clock_second++;
    shell_uptime_seconds++;

    if (shell_clock_second >= 60)
    {
        shell_clock_second = 0;
        shell_clock_minute++;
    }

    if (shell_clock_minute >= 60)
    {
        shell_clock_minute = 0;
        shell_clock_hour++;
    }

    if (shell_clock_hour >= 24)
    {
        shell_clock_hour = 0;
        shell_clock_day++;
    }

    if (shell_clock_day > 31)
    {
        shell_clock_day = 1;
        shell_clock_month++;
    }

    if (shell_clock_month > 12)
    {
        shell_clock_month = 1;
        shell_clock_year++;
    }
}

static void shell_add_history(const char* text)
{
    int index = history_count % MAX_HISTORY;
    shell_copy_string(history[index], text, MAX_CMD);

    if (history_count < MAX_HISTORY)
    {
        history_count++;
    }
}

static void shell_add_alias(const char* name, const char* value)
{
    if (alias_count >= MAX_ALIASES)
    {
        return;
    }

    int index = alias_count++;
    int i = 0;

    while (name[i] != '\0' && i < MAX_CMD - 2)
    {
        aliases[index][i] = name[i];
        i++;
    }
    aliases[index][i] = '=';
    i++;

    int j = 0;
    while (value[j] != '\0' && i + j < MAX_CMD - 1)
    {
        aliases[index][i + j] = value[j];
        j++;
    }
    aliases[index][i + j] = '\0';
}

static int shell_lookup_alias(const char* name, char* value, int max_len)
{
    for (int i = 0; i < alias_count; i++)
    {
        int j = 0;
        while (aliases[i][j] != '\0' && aliases[i][j] != '=' && j < max_len - 1)
        {
            if (aliases[i][j] != name[j])
            {
                break;
            }
            j++;
        }

        if (aliases[i][j] == '=' && name[j] == '\0')
        {
            shell_copy_string(value, aliases[i] + j + 1, max_len);
            return 1;
        }
    }

    return 0;
}

static int shell_expand_alias(const char* input, char* output, int max_len)
{
    char first_token[MAX_CMD];
    const char* cursor = shell_read_token(input, first_token, sizeof(first_token));

    if (first_token[0] == '\0')
    {
        return 0;
    }

    char alias_value[MAX_CMD];
    if (!shell_lookup_alias(first_token, alias_value, sizeof(alias_value)))
    {
        return 0;
    }

    shell_copy_string(output, alias_value, max_len);
    cursor = shell_skip_spaces(cursor);

    if (*cursor != '\0')
    {
        int len = shell_strlen(output);
        if (len < max_len - 1)
        {
            output[len++] = ' ';
            shell_copy_string(output + len, cursor, max_len - len);
        }
    }

    return 1;
}

static void shell_print_help(void)
{
    imp_text("Available commands:\n");
    imp_text("  help        - affiche toutes les commandes\n");
    imp_text("  version     - affiche la version du noyau\n");
    imp_text("  uname       - informations sur le système\n");
    imp_text("  uptime      - temps depuis le démarrage\n");
    imp_text("  date        - affiche la date et l'heure\n");
    imp_text("  time        - affiche l'heure\n");
    imp_text("  reboot      - redémarre la machine\n");
    imp_text("  shutdown    - éteint la machine\n");
    imp_text("  loadkeys    - change la langue (fr / en-us)\n");
    imp_text("  cpuinfo     - informations sur le processeur\n");
    imp_text("  meminfo     - informations sur la mémoire\n");
    imp_text("  sysinfo     - résumé du système\n");
    imp_text("  pci         - liste les périphériques PCI\n");
    imp_text("  regs        - affiche les registres CPU\n");
    imp_text("  gdt         - affiche la GDT\n");
    imp_text("  idt         - affiche l'IDT\n");
    imp_text("  stack       - affiche la pile\n");
    imp_text("  heap        - informations sur le tas mémoire\n");
    imp_text("  irq         - état des interruptions\n");
    imp_text("  ls          - liste les fichiers\n");
    imp_text("  cd          - change de dossier\n");
    imp_text("  pwd         - affiche le dossier courant\n");
    imp_text("  cat         - affiche un fichier\n");
    imp_text("  touch       - crée un fichier\n");
    imp_text("  mkdir       - crée un dossier\n");
    imp_text("  rm          - supprime un fichier\n");
    imp_text("  cp          - copie un fichier\n");
    imp_text("  mv          - déplace ou renomme un fichier\n");
    imp_text("  tasks       - liste les tâches\n");
    imp_text("  kill        - termine une tâche\n");
    imp_text("  ps          - liste les processus\n");
    imp_text("  test        - lance les tests du noyau\n");
    imp_text("  panic       - déclenche un kernel panic\n");
    imp_text("  beep        - bip du PC Speaker\n");
    imp_text("  cls         - alias de clear\n");
    imp_text("  calc        - calculatrice simple\n");
    imp_text("  rand        - nombre aléatoire\n");
    imp_text("  sleep       - attend quelques secondes\n");
    imp_text("  repeat      - répète une commande\n");
    imp_text("  history     - historique des commandes\n");
    imp_text("  alias       - crée un alias\n");
    imp_text("  env         - variables d'environnement\n");
    imp_text("  ping        - test réseau (ping <host> [eth|wifi])\n");
    imp_text("  wifi        - wifi connect/disconnect/status\n");
    imp_text("  gui         - mode graphique\n");
    imp_text("  i_use_arch_btw - blague fun pour les utilisateurs Arch\n");
}

static void shell_set_current_dir(const char* path)
{
    int i = 0;
    while (path[i] != '\0' && i < 31)
    {
        current_dir[i] = path[i];
        i++;
    }
    current_dir[i] = '\0';
}

static void shell_resolve_path(const char* path, char* out, int max_len)
{
    if (path[0] == '/')
    {
        shell_copy_string(out, path, max_len);
        return;
    }

    if (current_dir[0] == '/' && current_dir[1] == '\0')
    {
        out[0] = '/';
        out[1] = '\0';
        if (path[0] != '\0')
        {
            shell_copy_string(out + 1, path, max_len - 1);
        }
        return;
    }

    int len = 0;
    while (current_dir[len] != '\0' && len < max_len - 1)
    {
        out[len] = current_dir[len];
        len++;
    }

    if (len == 0 || out[len - 1] != '/')
    {
        if (len < max_len - 1)
        {
            out[len++] = '/';
        }
    }

    shell_copy_string(out + len, path, max_len - len);
}

static void shell_go_to_parent(void)
{
    int len = 0;
    while (current_dir[len] != '\0' && len < 31)
    {
        len++;
    }

    if (len <= 1)
    {
        shell_set_current_dir("/");
        return;
    }

    int slash = len - 1;
    while (slash > 0 && current_dir[slash] != '/')
    {
        slash--;
    }

    if (slash <= 0)
    {
        shell_set_current_dir("/");
        return;
    }

    current_dir[slash] = '\0';
    if (current_dir[0] == '\0')
    {
        shell_set_current_dir("/");
    }
}

static void shell_enter_directory(const char* name)
{
    int len = 0;
    while (current_dir[len] != '\0' && len < 31)
    {
        len++;
    }

    if (len <= 1)
    {
        shell_set_current_dir("/");
        len = 1;
    }

    if (current_dir[0] == '/' && current_dir[1] == '\0')
    {
        if (len + 1 < 31)
        {
            current_dir[len] = '/';
            current_dir[len + 1] = '\0';
            len++;
        }
    }
    else if (current_dir[len - 1] != '/')
    {
        if (len + 1 < 31)
        {
            current_dir[len] = '/';
            current_dir[len + 1] = '\0';
            len++;
        }
    }

    int i = 0;
    while (name[i] != '\0' && len + i < 31)
    {
        current_dir[len + i] = name[i];
        i++;
    }
    current_dir[len + i] = '\0';
}

static void shell_print_current_directory(void)
{
    imp_text(current_dir);
    imp_char('\n');
}

static void shell_print_prompt(void)
{
    imp_text("ORT$");
    imp_text(current_dir);
    imp_char('>');
}

static int shell_execute_text(const char* text)
{
    int i = 0;
    while (text[i] != '\0' && i < MAX_CMD - 1)
    {
        cmd[i] = text[i];
        i++;
    }
    cmd[i] = '\0';
    return i;
}

static void shell_execute_command(void)
{
    if (cmd[0] == '\0')
    {
        return;
    }

    char expanded[MAX_CMD];
    if (shell_expand_alias(cmd, expanded, sizeof(expanded)))
    {
        shell_execute_text(expanded);
    }

    shell_add_history(cmd);
    shell_tick_clock();

    if (shell_streq(cmd, "help"))
    {
        shell_print_help();
    }
    else if (shell_streq(cmd, "clear") || shell_streq(cmd, "cls"))
    {
        imp_cls();
    }
    else if (shell_streq(cmd, "hello"))
    {
        imp_text("Hello from ORT!\n");
    }
    else if (shell_streq(cmd, "version"))
    {
        imp_text("ORT kernel version beta-1.9.9\n");
    }
    else if (shell_streq(cmd, "uname"))
    {
        imp_text("ORTOS 0.2 x86_64 GNU/Linux\n");
    }
    else if (shell_streq(cmd, "uptime"))
    {
        imp_text("Uptime: ");
        shell_print_int(shell_uptime_seconds);
        imp_text("s\n");
    }
    else if (shell_streq(cmd, "date"))
    {
        imp_text("Date: ");
        shell_print_int(shell_clock_year);
        imp_char('-');
        shell_print_int(shell_clock_month);
        imp_char('-');
        shell_print_int(shell_clock_day);
        imp_char('\n');
    }
    else if (shell_streq(cmd, "time"))
    {
        imp_text("Time: ");
        shell_print_int(shell_clock_hour);
        imp_char(':');
        shell_print_int(shell_clock_minute);
        imp_char(':');
        shell_print_int(shell_clock_second);
        imp_char('\n');
    }
    else if (shell_streq(cmd, "reboot"))
    {
        imp_text("Reboot requested.\n");
        while (1)
        {
            /* wait for an external reset */
        }
    }
    else if (shell_streq(cmd, "shutdown"))
    {
        imp_text("Shutdown requested.\n");
        asm volatile ("cli; hlt");
    }
    else if (shell_starts_with(cmd, "loadkeys"))
    {
        const char* argument = shell_skip_spaces(cmd + 8);
        if (shell_streq(argument, "fr"))
        {
            imp_text("Keyboard layout switched to fr.\n");
            current_layout[0] = 'f';
            current_layout[1] = 'r';
            current_layout[2] = '\0';
            keyboard_set_layout(current_layout);
        }
        else if (shell_streq(argument, "en-us"))
        {
            imp_text("Keyboard layout switched to en-us.\n");
            current_layout[0] = 'e';
            current_layout[1] = 'n';
            current_layout[2] = '-';
            current_layout[3] = 'u';
            current_layout[4] = 's';
            current_layout[5] = '\0';
            keyboard_set_layout(current_layout);
        }
        else
        {
            imp_text("Usage: loadkeys fr|en-us\n");
        }
    }
    else if (shell_streq(cmd, "cpuinfo"))
    {
        imp_text("CPU: x86_64, 1 core, unknown model\n");
    }
    else if (shell_streq(cmd, "meminfo"))
    {
        imp_text("Memory: 64MB available\n");
    }
    else if (shell_streq(cmd, "sysinfo"))
    {
        imp_text("System: ORTOS shell, VGA console, keyboard input\n");
    }
    else if (shell_streq(cmd, "pci"))
    {
        imp_text("PCI: VGA controller, PS/2 keyboard controller\n");
    }
    else if (shell_streq(cmd, "regs"))
    {
        imp_text("CPU registers: RAX=0x0 RBX=0x0 RCX=0x0\n");
    }
    else if (shell_streq(cmd, "gdt"))
    {
        imp_text("GDT: placeholder entry loaded\n");
    }
    else if (shell_streq(cmd, "idt"))
    {
        imp_text("IDT: placeholder entry loaded\n");
    }
    else if (shell_streq(cmd, "stack"))
    {
        imp_text("Stack: kernel stack initialized\n");
    }
    else if (shell_streq(cmd, "heap"))
    {
        imp_text("Heap: simple static allocator active\n");
    }
    else if (shell_streq(cmd, "irq"))
    {
        imp_text("IRQ: keyboard interrupt enabled\n");
    }
    else if (shell_streq(cmd, "ls"))
    {
        for (int i = 0; i < storage_get_entry_count(); i++)
        {
            imp_text(storage_get_entry_name(i));
            if (storage_get_entry_type(i) == 'd')
            {
                imp_char('/');
            }
            imp_char('\n');
        }
    }
    else if (shell_starts_with(cmd, "cd"))
    {
        const char* argument = shell_skip_spaces(cmd + 2);
        if (argument[0] == '\0')
        {
            imp_text("Usage: cd <dir>\n");
        }
        else if (shell_streq(argument, ".."))
        {
            shell_go_to_parent();
        }
        else
        {
            char resolved[MAX_CMD];
            shell_resolve_path(argument, resolved, sizeof(resolved));
            int index = storage_find_entry(resolved);
            if (index >= 0 && storage_get_entry_type(index) == 'd')
            {
                imp_text("Changed directory to ");
                imp_text(argument);
                imp_char('\n');
                shell_set_current_dir(resolved);
            }
            else
            {
                imp_text("No such directory\n");
            }
        }
    }
    else if (shell_streq(cmd, "pwd"))
    {
        shell_print_current_directory();
    }
    else if (shell_starts_with(cmd, "cat"))
    {
        const char* argument = shell_skip_spaces(cmd + 3);
        char resolved[MAX_CMD];
        shell_resolve_path(argument, resolved, sizeof(resolved));
        int index = storage_find_entry(resolved);
        if (index >= 0 && storage_get_entry_type(index) == 'f')
        {
            imp_text(storage_get_entry_content(index));
            imp_char('\n');
        }
        else
        {
            imp_text("No such file\n");
        }
    }
    else if (shell_starts_with(cmd, "touch"))
    {
        const char* argument = shell_skip_spaces(cmd + 5);
        if (argument[0] != '\0')
        {
            char resolved[MAX_CMD];
            shell_resolve_path(argument, resolved, sizeof(resolved));
            if (storage_find_entry(resolved) >= 0)
            {
                imp_text("File already exists\n");
            }
            else
            {
                storage_create_entry(resolved, 'f', "");
                imp_text("File created\n");
            }
        }
        else
        {
            imp_text("Usage: touch <file>\n");
        }
    }
    else if (shell_starts_with(cmd, "mkdir"))
    {
        const char* argument = shell_skip_spaces(cmd + 5);
        if (argument[0] != '\0')
        {
            char resolved[MAX_CMD];
            shell_resolve_path(argument, resolved, sizeof(resolved));
            if (storage_find_entry(resolved) >= 0)
            {
                imp_text("Directory already exists\n");
            }
            else
            {
                storage_create_entry(resolved, 'd', "");
                imp_text("Directory created\n");
            }
        }
        else
        {
            imp_text("Usage: mkdir <dir>\n");
        }
    }
    else if (shell_starts_with(cmd, "rm"))
    {
        const char* argument = shell_skip_spaces(cmd + 2);
        if (argument[0] != '\0')
        {
            char resolved[MAX_CMD];
            shell_resolve_path(argument, resolved, sizeof(resolved));
            if (storage_remove_entry(resolved))
            {
                imp_text("Entry removed\n");
            }
            else
            {
                imp_text("No such entry\n");
            }
        }
        else
        {
            imp_text("Usage: rm <file>\n");
        }
    }
    else if (shell_starts_with(cmd, "cp"))
    {
        const char* cursor = shell_skip_spaces(cmd + 2);
        char src[MAX_CMD];
        char dst[MAX_CMD];
        cursor = shell_read_token(cursor, src, sizeof(src));
        cursor = shell_skip_spaces(cursor);
        shell_read_token(cursor, dst, sizeof(dst));

        if (src[0] == '\0' || dst[0] == '\0')
        {
            imp_text("Usage: cp <src> <dst>\n");
        }
        else
        {
            char src_path[MAX_CMD];
            char dst_path[MAX_CMD];
            shell_resolve_path(src, src_path, sizeof(src_path));
            shell_resolve_path(dst, dst_path, sizeof(dst_path));
            int index = storage_find_entry(src_path);
            if (index >= 0)
            {
                if (!storage_create_entry(dst_path, storage_get_entry_type(index), storage_get_entry_content(index)))
                {
                    imp_text("Copy failed\n");
                }
                else
                {
                    imp_text("Copied entry\n");
                }
            }
            else
            {
                imp_text("No such entry\n");
            }
        }
    }
    else if (shell_starts_with(cmd, "mv"))
    {
        const char* cursor = shell_skip_spaces(cmd + 2);
        char src[MAX_CMD];
        char dst[MAX_CMD];
        cursor = shell_read_token(cursor, src, sizeof(src));
        cursor = shell_skip_spaces(cursor);
        shell_read_token(cursor, dst, sizeof(dst));

        if (src[0] == '\0' || dst[0] == '\0')
        {
            imp_text("Usage: mv <src> <dst>\n");
        }
        else
        {
            char src_path[MAX_CMD];
            char dst_path[MAX_CMD];
            shell_resolve_path(src, src_path, sizeof(src_path));
            shell_resolve_path(dst, dst_path, sizeof(dst_path));
            int index = storage_find_entry(src_path);
            if (index >= 0)
            {
                if (!storage_create_entry(dst_path, storage_get_entry_type(index), storage_get_entry_content(index)))
                {
                    imp_text("Move failed\n");
                }
                else
                {
                    storage_remove_entry(src_path);
                    imp_text("Moved entry\n");
                }
            }
            else
            {
                imp_text("No such entry\n");
            }
        }
    }
    else if (shell_streq(cmd, "tasks"))
    {
        imp_text("Tasks: shell, idle, keyboard\n");
    }
    else if (shell_starts_with(cmd, "kill"))
    {
        imp_text("Task kill requested.\n");
    }
    else if (shell_streq(cmd, "ps"))
    {
        imp_text("PID 1 shell\nPID 2 idle\n");
    }
    else if (shell_streq(cmd, "test"))
    {
        imp_text("Kernel tests: OK\n");
    }
    else if (shell_streq(cmd, "panic"))
    {
        imp_text("Kernel panic triggered\n");
        asm volatile ("ud2");
    }
    else if (shell_streq(cmd, "beep"))
    {
        imp_text("BEEP!\n");
        asm volatile ("outb %%al, $0x61" : : "a"(0x03));
    }
    else if (shell_streq(cmd, "calc"))
    {
        imp_text("Calculator ready. Use 'calc <a> <op> <b>'\n");
    }
    else if (shell_starts_with(cmd, "calc"))
    {
        const char* cursor = shell_skip_spaces(cmd + 4);
        char lhs[MAX_CMD];
        char op[MAX_CMD];
        char rhs[MAX_CMD];
        cursor = shell_read_token(cursor, lhs, sizeof(lhs));
        cursor = shell_skip_spaces(cursor);
        cursor = shell_read_token(cursor, op, sizeof(op));
        cursor = shell_skip_spaces(cursor);
        shell_read_token(cursor, rhs, sizeof(rhs));

        if (lhs[0] != '\0' && rhs[0] != '\0' && op[0] != '\0')
        {
            int left = 0;
            int right = 0;
            int valid = 1;
            int value = 0;
            int i = 0;

            while (lhs[i] != '\0')
            {
                if (lhs[i] < '0' || lhs[i] > '9')
                {
                    valid = 0;
                    break;
                }
                left = left * 10 + (lhs[i] - '0');
                i++;
            }

            i = 0;
            while (rhs[i] != '\0')
            {
                if (rhs[i] < '0' || rhs[i] > '9')
                {
                    valid = 0;
                    break;
                }
                right = right * 10 + (rhs[i] - '0');
                i++;
            }

            if (valid)
            {
                if (op[0] == '+')
                {
                    value = left + right;
                }
                else if (op[0] == '-')
                {
                    value = left - right;
                }
                else if (op[0] == '*')
                {
                    value = left * right;
                }
                else if (op[0] == '/')
                {
                    if (right != 0)
                    {
                        value = left / right;
                    }
                    else
                    {
                        valid = 0;
                    }
                }
                else if (op[0] == '%')
                {
                    if (right != 0)
                    {
                        value = left % right;
                    }
                    else
                    {
                        valid = 0;
                    }
                }
                else
                {
                    valid = 0;
                }
            }

            if (valid)
            {
                imp_text("Result: ");
                shell_print_int(value);
                imp_char('\n');
            }
            else
            {
                imp_text("Usage: calc <a> <op> <b>\n");
            }
        }
        else
        {
            imp_text("Usage: calc <a> <op> <b>\n");
        }
    }
    else if (shell_streq(cmd, "rand"))
    {
        shell_seed = shell_seed * 1103515245 + 12345;
        imp_text("Random: ");
        shell_print_int(shell_seed & 0x7fff);
        imp_char('\n');
    }
    else if (shell_streq(cmd, "sleep"))
    {
        int delay = 1;
        int i = 0;
        imp_text("Sleeping...\n");
        while (i < 1000000 * delay)
        {
            i++;
        }
    }
    else if (shell_starts_with(cmd, "repeat"))
    {
        const char* cursor = shell_skip_spaces(cmd + 6);
        char token[MAX_CMD];
        shell_read_token(cursor, token, sizeof(token));

        if (token[0] == '\0')
        {
            if (history_count > 0)
            {
                int index = (history_count - 1 + MAX_HISTORY) % MAX_HISTORY;
                shell_execute_text(history[index]);
            }
            else
            {
                imp_text("No history available\n");
            }
        }
        else
        {
            int index = 0;
            int valid = 1;
            int i = 0;
            while (token[i] != '\0')
            {
                if (token[i] < '0' || token[i] > '9')
                {
                    valid = 0;
                    break;
                }
                index = index * 10 + (token[i] - '0');
                i++;
            }

            if (valid && index >= 0 && index < history_count)
            {
                int history_index = (history_count - 1 - index + MAX_HISTORY) % MAX_HISTORY;
                shell_execute_text(history[history_index]);
            }
            else
            {
                shell_execute_text(token);
            }
        }
    }
    else if (shell_streq(cmd, "history"))
    {
        for (int i = 0; i < history_count; i++)
        {
            int index = (history_count - 1 - i + MAX_HISTORY) % MAX_HISTORY;
            imp_text(history[index]);
            imp_char('\n');
        }
    }
    else if (shell_starts_with(cmd, "alias"))
    {
        const char* cursor = shell_skip_spaces(cmd + 5);
        char name[MAX_CMD];
        char value[MAX_CMD];
        cursor = shell_read_token(cursor, name, sizeof(name));
        cursor = shell_skip_spaces(cursor);
        shell_read_token(cursor, value, sizeof(value));

        if (name[0] != '\0' && value[0] != '\0')
        {
            shell_add_alias(name, value);
            imp_text("Alias created\n");
        }
        else if (name[0] != '\0')
        {
            char stored[MAX_CMD];
            if (shell_lookup_alias(name, stored, sizeof(stored)))
            {
                imp_text(name);
                imp_text(" -> ");
                imp_text(stored);
                imp_char('\n');
            }
            else
            {
                imp_text("Alias not found\n");
            }
        }
        else
        {
            for (int i = 0; i < alias_count; i++)
            {
                imp_text(aliases[i]);
                imp_char('\n');
            }
        }
    }
    else if (shell_streq(cmd, "env"))
    {
        imp_text("PATH=/bin\nLANG=");
        imp_text(current_layout);
        imp_char('\n');
    }
    else if (shell_starts_with(cmd, "ping"))
    {
        const char* cursor = shell_skip_spaces(cmd + 4);
        char host[MAX_CMD];
        char mode[MAX_CMD];
        cursor = shell_read_token(cursor, host, sizeof(host));
        cursor = shell_skip_spaces(cursor);
        shell_read_token(cursor, mode, sizeof(mode));

        if (host[0] == '\0')
        {
            imp_text("Usage: ping <host> [eth|wifi]\n");
        }
        else if (mode[0] != '\0' && !shell_streq(mode, "eth") && !shell_streq(mode, "wifi"))
        {
            imp_text("Usage: ping <host> [eth|wifi]\n");
        }
        else if (shell_streq(mode, "wifi") && !network_is_wifi_connected())
        {
            imp_text("Wi-Fi not connected\n");
        }
        else if (!shell_streq(mode, "wifi") && !network_has_ethernet())
        {
            imp_text("Ethernet not available\n");
        }
        else
        {
            network_ping(host, shell_streq(mode, "wifi"));
        }
    }
    else if (shell_starts_with(cmd, "wifi"))
    {
        const char* cursor = shell_skip_spaces(cmd + 4);
        char subcmd[MAX_CMD];
        cursor = shell_read_token(cursor, subcmd, sizeof(subcmd));

        if (shell_streq(subcmd, "connect"))
        {
            char ssid[MAX_CMD];
            cursor = shell_skip_spaces(cursor);
            shell_read_token(cursor, ssid, sizeof(ssid));
            if (ssid[0] == '\0')
            {
                imp_text("Usage: wifi connect <ssid>\n");
            }
            else
            {
                network_connect_wifi(ssid);
                imp_text("Wi-Fi connected to ");
                imp_text(ssid);
                imp_char('\n');
            }
        }
        else if (shell_streq(subcmd, "disconnect"))
        {
            network_disconnect_wifi();
            imp_text("Wi-Fi disconnected\n");
        }
        else if (shell_streq(subcmd, "status"))
        {
            if (network_is_wifi_connected())
            {
                imp_text("Wi-Fi connected to ");
                imp_text(network_get_wifi_ssid());
                imp_char('\n');
            }
            else
            {
                imp_text("Wi-Fi disconnected\n");
            }
        }
        else
        {
            imp_text("Usage: wifi connect <ssid> | disconnect | status\n");
        }
    }
    else if (shell_streq(cmd, "i_use_arch_btw"))
    {
        imp_text("If you run this command, Why you didn't use Arch btw?.\n");
        imp_text("You should have used Windows, It is better for your mental health.\n");
    }
    else if (shell_streq(cmd, "echo"))
    {
        imp_char('\n');
    }
    else if (shell_starts_with(cmd, "echo"))
    {
        const char* argument = shell_skip_spaces(cmd + 4);
        if (*argument != '\0')
        {
            imp_text(argument);
            imp_char('\n');
        }
        else
        {
            imp_char('\n');
        }
    }
    else if (shell_streq(cmd, "gui"))
    {
        gui_enter();
    }
    else
    {
        imp_text("Unknown command. Type 'help' for a list.\n");
    }
}

void shell_init()
{
    storage_init();

    imp_text("ORT Shell\n");
    imp_text("Type 'help' for a list of commands.\n");
}

void shell_run()
{
    while (1)
    {
        shell_print_prompt();
        imp_char(' ');

        int pos = 0;

        while (1)
        {
            char c = (char)keyboard_getchar();

            if (c == '\n')
            {
                cmd[pos] = '\0';
                imp_char('\n');
                break;
            }

            if (c == '\b')
            {
                if (pos > 0)
                {
                    pos--;
                    imp_text("\b \b");
                    cmd[pos] = '\0';
                }
                continue;
            }

            if (pos < MAX_CMD - 1)
            {
                cmd[pos++] = c;
                imp_char(c);
            }
        }

        shell_execute_command();
    }
};