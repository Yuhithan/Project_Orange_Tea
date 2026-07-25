#include "shell.h"
#include "keyboard.h"
#include "imp.h"

#define MAX_CMD 128

static char cmd[MAX_CMD];

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

static void shell_execute_command(void)
{
    if (cmd[0] == '\0')
    {
        return;
    }

    if (shell_streq(cmd, "help"))
    {
        imp_text("Available commands:\n");
        imp_text("  help      - show this help\n");
        imp_text("  clear     - clear the screen\n");
        imp_text("  echo      - print text after the command\n");
        imp_text("  hello     - greet the user\n");
        imp_text("  about     - show OS info\n");
        imp_text("  reboot    - restart the shell\n");
    }
    else if (shell_streq(cmd, "clear"))
    {
        imp_cls();
    }
    else if (shell_streq(cmd, "hello"))
    {
        imp_text("Hello from ORT!\n");
    }
    else if (shell_streq(cmd, "about"))
    {
        imp_text("ORT - The new rival of Windows\n");
        imp_text("Kernel shell v1.0\n");
    }
    else if (shell_streq(cmd, "reboot"))
    {
        imp_text("Shell reboot requested.\n");
    }
    else if (shell_starts_with(cmd, "echo"))
    {
        const char* argument = cmd + 4;

        while (*argument == ' ')
        {
            argument++;
        }

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
    else
    {
        imp_text("Unknown command. Type 'help' for a list.\n");
    }
}

void shell_init()
{
    imp_text("ORT Shell\n");
    imp_text("Type 'help' for a list of commands.\n");
}

void shell_run()
{
    while (1)
    {
        imp_text("ORT> ");

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