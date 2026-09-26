#include "update.h"

#include <stdio.h>

int main(int argc, char **argv)
{
    char error[128]; int result;
    if (argc != 4 || (argv[1][0] != 'c' && argv[1][0] != 'a')) {
        fprintf(stderr, "usage: %s check|apply MANIFEST INSTALL_ROOT\n", argv[0]);
        return 2;
    }
    result = argv[1][0] == 'c' ? ortos_update_check(argv[2], argv[3], error, sizeof(error)) : ortos_update_apply(argv[2], argv[3], error, sizeof(error));
    if (result) { fprintf(stderr, "update: %s\n", error); return 1; }
    puts(argv[1][0] == 'c' ? "update is compatible and verified" : "update installed; previous kernel preserved as kernel.bin.previous");
    return 0;
}