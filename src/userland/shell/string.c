
#include "userland/shell/shell.h"
#include "custom/text.h"
#include "fat/string.h"

int string_read_line(char *buffer, size_t capacity)
{
    size_t length = 0;
    size_t overflow_length = 0;
    int c;

    while ((c = getchar()) != '\n') {
        if (c == '\r')
            continue;

        if (c == '\b') {
            if (overflow_length > 0) {
                overflow_length--;
                printf_("\b");
            } else if (length > 0) {
                length--;
                buffer[length] = '\0';
                printf_("\b");
            }
            continue;
        }

        if (capacity > 0 && length < capacity - 1) {
            buffer[length++] = (char)c;
        } else {
            overflow_length++;
        }
    }

    if (capacity > 0)
        buffer[length] = '\0';

    return capacity > 0 && overflow_length == 0;
}
