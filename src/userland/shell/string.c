#include "userland/shell/shell.h"
#include "custom/text.h"
#include <limits.h>

int string_read_input(void)
{
    for (;;) {
        unsigned int magnitude = 0;
        unsigned int limit;
        int negative = 0;
        int has_digits = 0;
        int trailing_whitespace = 0;
        int invalid = 0;
        int c;

        do {
            c = getchar();
        } while (c == ' ' || c == '\t' || c == '\r' ||
                 c == '\v' || c == '\f');

        if (c == '+' || c == '-') {
            negative = c == '-';
            c = getchar();
            if (c == ' ' || c == '\t' || c == '\r' ||
                c == '\v' || c == '\f') {
                invalid = 1;
            }
        }

        limit = (unsigned int)INT_MAX + (negative ? 1U : 0U);

        while (c != '\n') {
            if (c >= '0' && c <= '9' && !trailing_whitespace) {
                unsigned int digit = (unsigned int)(c - '0');

                has_digits = 1;
                if (magnitude > (limit - digit) / 10U) {
                    invalid = 1;
                } else if (!invalid) {
                    magnitude = magnitude * 10U + digit;
                }
            } else if (c == ' ' || c == '\t' || c == '\r' ||
                       c == '\v' || c == '\f') {
                if (has_digits) {
                    trailing_whitespace = 1;
                }
            } else {
                invalid = 1;
            }

            c = getchar();
        }

        if (!has_digits || invalid) {
            printf_("Invalid integer input. Please try again.\n");
            continue;
        }

        if (negative) {
            if (magnitude == (unsigned int)INT_MAX + 1U) {
                return INT_MIN;
            }
            return -(int)magnitude;
        }

        return (int)magnitude;
    }
}