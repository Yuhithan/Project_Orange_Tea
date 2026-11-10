#include "apps/shell/shell.h"
#include "drivers/printf.h"

#define BLUE "\033[1;34m"
#define RESET "\033[0m"

int shell_loop(void) {
    printf_(BLUE "TestShell> " RESET);
    return 0;
}