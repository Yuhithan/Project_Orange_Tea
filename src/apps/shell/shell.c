#include "apps/shell/shell.h"
#include "drivers/printf.h"

int shell_loop(){
    char *line;
    char **args;
    int status;
    
    do {
        print_str("> ");
        line = lsh_split_line();
        args = lsh_split_line(line);
        status = lsh_execute(args);

        free(line);
        free(args);
    } while (status);
    
    //print_str("Test shell....");
}