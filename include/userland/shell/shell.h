#pragma once

#pragma once
#include "drivers/printf.h"
#include <stddef.h>

void shell_loop(void);
int string_read_line(char *buffer, size_t capacity);
void readDisk(void);
//void shell_do_ouput();