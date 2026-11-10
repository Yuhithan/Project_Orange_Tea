#include "drivers/printf.h"
#include "core/irq.h"
#include <stdint.h>

void irq_exception(uint64_t vector, uint64_t error_code)
{
    printf_("EXCEPTION vector=%llu error=0x%llx\nSystem halted.\n",
            (unsigned long long)vector,
            (unsigned long long)error_code);
    __asm__ volatile ("cli");
    for (;;) __asm__ volatile ("hlt");
}
