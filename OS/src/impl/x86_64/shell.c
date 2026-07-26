#include "shell.h"
#include "keyboard.h"
#include "imp.h"
#include "storage.h"

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

static void shell_add_history(const char* text)
{
    int index = history_count % MAX_HISTORY;
    for (int i = 0; text[i] != '\0' && i < MAX_CMD - 1; i++)
    {
        history[index][i] = text[i];
    }
    history[index][MAX_CMD - 1] = '\0';

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

    while (name[i] != '\0' && i < 15)
    {
        aliases[index][i] = name[i];
        i++;
    }
    aliases[index][i] = '\0';

    int j = 0;
    while (value[j] != '\0' && j < MAX_CMD - 1)
    {
        aliases[index][i + 1 + j] = value[j];
        j++;
    }
    aliases[index][i + 1 + j] = '\0';
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

static void shell_execute_command(void)
{
    if (cmd[0] == '\0')
    {
        return;
    }

    shell_add_history(cmd);

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
        imp_text("ORT kernel version 0.1\n");
    }
    else if (shell_streq(cmd, "uname"))
    {
        imp_text("ORTOS 0.1 x86_64 GNU/Linux\n");
    }
    else if (shell_streq(cmd, "uptime"))
    {
        imp_text("Uptime: 00:00:00\n");
    }
    else if (shell_streq(cmd, "date"))
    {
        imp_text("Date: 2026-07-25\n");
    }
    else if (shell_streq(cmd, "time"))
    {
        imp_text("Time: 12:00:00\n");
    }
    else if (shell_streq(cmd, "reboot"))
    {
        imp_text("Reboot requested.\n");
    }
    else if (shell_streq(cmd, "shutdown"))
    {
        imp_text("Shutdown requested.\n");
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
        imp_text("CPU registers: not implemented in this build\n");
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
            int index = storage_find_entry(argument);
            if (index >= 0 && storage_get_entry_type(index) == 'd')
            {
                imp_text("Changed directory to ");
                imp_text(argument);
                imp_char('\n');
                shell_enter_directory(argument);
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
        int index = storage_find_entry(argument);
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
            storage_create_entry(argument, 'f', "");
            imp_text("File created\n");
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
            storage_create_entry(argument, 'd', "");
            imp_text("Directory created\n");
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
            storage_remove_entry(argument);
            imp_text("Entry removed\n");
        }
        else
        {
            imp_text("Usage: rm <file>\n");
        }
    }
    else if (shell_starts_with(cmd, "cp"))
    {
        const char* argument = shell_skip_spaces(cmd + 2);
        if (argument[0] != '\0')
        {
            imp_text("Copy operation is simulated.\n");
        }
        else
        {
            imp_text("Usage: cp <src> <dst>\n");
        }
    }
    else if (shell_starts_with(cmd, "mv"))
    {
        const char* argument = shell_skip_spaces(cmd + 2);
        if (argument[0] != '\0')
        {
            imp_text("Move operation is simulated.\n");
        }
        else
        {
            imp_text("Usage: mv <src> <dst>\n");
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
    }
    else if (shell_streq(cmd, "beep"))
    {
        imp_text("BEEP!\n");
    }
    else if (shell_streq(cmd, "calc"))
    {
        imp_text("Calculator ready. Use 'calc <a> <op> <b>'\n");
    }
    else if (shell_streq(cmd, "rand"))
    {
        shell_seed = shell_seed * 1103515245 + 12345;
        imp_text("Random: ");
        imp_text("0");
        imp_char('\n');
    }
    else if (shell_streq(cmd, "sleep"))
    {
        imp_text("Sleeping...\n");
    }
    else if (shell_starts_with(cmd, "repeat"))
    {
        imp_text("Repeat command is ready.\n");
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
        const char* argument = shell_skip_spaces(cmd + 5);
        if (argument[0] != '\0')
        {
            imp_text("Alias support enabled.\n");
        }
        else
        {
            imp_text("Usage: alias <name> <command>\n");
        }
    }
    else if (shell_streq(cmd, "env"))
    {
        imp_text("PATH=/bin\nLANG=");
        imp_text(current_layout);
        imp_char('\n');
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
            imp_text((char*)argument);
            imp_char('\n');
        }
        else
        {
            imp_char('\n');
        }
    }
    else if (shell_streq(cmd, "gui")){
        imp_text("GUI mode is not implemented in this build in this version.\n");
        impl_text("please wait for the next version of ORTOS, it will be implemented soon.\n");
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
}