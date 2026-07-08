#include "shell.h"
#include "keyboard.h"
#include "imp.h"

#define MAX_CMD 128

static char cmd[MAX_CMD];

void shell_init()
{
    imp_text("ORT Shell\n");
}

void shell_run()
{
    while (1)
    {
        imp_text("> ");

        int pos = 0;

        while (1)
        {
            char c = keyboard_getchar();

            if (c == '\n')
            {
                cmd[pos] = 0;
                imp_char('\n');
                break;
            }

            if (c == '\b')
            {
                if (pos > 0)
                {
                    pos--;
                    // You can later add code to erase the character on screen.
                }
                continue;
            }

            if (pos < MAX_CMD - 1)
            {
                cmd[pos++] = c;
                imp_char(c);
            }
        }

        if (cmd[0] == 'h')
        {
            imp_text("Hello!\n");
        }
        else
        {
            imp_text("Unknown command\n");
        }
    }
}