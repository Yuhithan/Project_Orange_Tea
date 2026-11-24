#include "userland/shell/shell.h"
#include "custom/text.h"
#include "drivers/printf.h"

#define SHELL_PROMPT "Shell$ "
#define SHELL_INPUT_SIZE 64

static int shell_command_equals(const char *command, const char *expected)
{
    while (*command && *command == *expected) {
        command++;
        expected++;
    }

    return *command == '\0' && *expected == '\0';
}

static void shell_handle_keyboard(struct KeyboardEvent event)
{
    if (event.type == KEYBOARD_EVENT_TYPE_MAKE &&
        event.code == KEY_CODE_ENTER) {
        console_input_push('\n');
        printf_(NEWLINE);
    } else if (event.type == KEYBOARD_EVENT_TYPE_MAKE &&
               event.code == KEY_CODE_DELETE) {
        console_input_push('\b');
    } else {
        en_us_handle_input(event);
    }
}

void shell_loop(void)
{
    keyboard_set_handler(shell_handle_keyboard);
    printf_(BLUE SHELL_PROMPT RESET);

    for (;;) {
        char command[SHELL_INPUT_SIZE];

        if (!string_read_line(command, sizeof(command))) {
            printf_("Command is too long.\n");
        } else if (command[0] != '\0') {
            if (shell_command_equals(command, "help")) {
                printf_("===============\n");
                printf_("help\n");
                printf_("fastfetch\n");
                printf_("echo\n");
                printf_("hello\n");
                printf_("readDisk\n");
                printf_("===============\n");
            }
            else if (shell_command_equals(command, "fastfetch")) {

                    
                        const char *logo[] = {
                            "          ####",
                            "          ##@@##",
                            "          ##@@@@##",
                            "          ##@@**@@##",
                            "            ##@@@@*@##",
                            "              ##@@@@##    ##########",
                            "            ######@@##  ##@@@@@@@@@@##",
                            "        ####::::::######@@@@@@@@####",
                            "      ##::::    ::##############",
                            "    ##::::  ::::::::##::##",
                            "    ##::  ::::::::::::::::##",
                            "    ##::  ::::::::::::::::##",
                            "    ##::::::::::::::::::::##",
                            "    ##::::::::::::::::::::##",
                            "      ##::::::::::::::::##",
                            "        ##::::::::::::##",
                            "          ############"
                        }; 
                        
                        
                        int rows = sizeof(logo) / sizeof(logo[0]);

                        for (int i = 0; i < rows; i++)
                        {
                            printf_(YELLOW "%s\n" RESET, logo[i]);
                        }
                    printf_(YELLOW "Welcome to Orangetea OS\n" RESET);

            }
            else if (shell_command_equals(command, "cls")) {
                console_clear();
            }
            else if (shell_command_equals(command, "panic")) {
                cli();
            }
            else if (shell_command_equals (command, "echo"))
            {
                printf_(NEWLINE);
            }
            else if (command[0]== 'e' && command[1]== 'c' && command[2]== 'h' && command[3]== 'o' && command[4]== ' ') {
                printf_("%s\n", command + 5);
            }
            else if (shell_command_equals(command, "hello")) {
                printf_("Hello, USER!!\n");
            }
            else if (shell_command_equals(command, "calc")) {
                printf_("use calc for\n use +-*/\n");
            }
            else if (command[0] == 'c' && command[1] == 'a' && command[2] == 'l' && command[3] == 'c' && command[4] == ' ') {
                printf_("test\n");
            }
            /*else if (shell_command_equals(command, "readDisk")) {
                readDisk();
            }*/
            else if (shell_command_equals(command, "asmexe")) {
                printf_(GREEN "to use Assembly x86_64 sysntax to execute" RESET NEWLINE);
                printf_(RED "Can break computer if wrong syntax executed" RESET NEWLINE);
            }
            else if (command[0] == 'a' && command[1] == 's' && command[2] == 'm' && command[3] == 'e' && command[4] == 'x' && command[5] == 'e' && command[6] == ' ') {
                /*__asm__ volatile ("just wan to see");*/
                printf_(YELLOW"not supported yet" RESET NEWLINE);
            }
            /*else if (shell_command_equals(command, "Dump")) {
                hexDump();
            }*/
            else if (shell_command_equals(command, "panic")) {
                cli();
            }
            else {
                printf_("Unknown command. Try 'help'.\n");
            }
        }

        printf_(BLUE SHELL_PROMPT RESET);
    }
}
