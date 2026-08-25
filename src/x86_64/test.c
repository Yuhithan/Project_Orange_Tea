#include "imp.h"
#include "test.h"
#include "boot_mode.h"
#include "timer.h"

void test_boot(void)
{
    ortos_boot_mode_set(ORTOS_BOOT_MODE_SHELL);

    if (ortos_boot_mode_get() != ORTOS_BOOT_MODE_SHELL) {
        imp_text("BOOT MODE TEST: FAIL\n");
        return;
    }

    imp_text("BOOT MODE TEST: PASS\n");
    imp_text("Returning to shell in:\n");

    for (int i = 5; i > 0; i--) {
        if (i == 5)
            imp_text("5...\n");
        else if (i == 4)
            imp_text("4...\n");
        else if (i == 3)
            imp_text("3...\n");
        else if (i == 2)
            imp_text("2...\n");
        else if (i == 1)
            imp_text("1...\n");

        timer_sleep(1000);
    }

    imp_text("Starting shell!\n");
}