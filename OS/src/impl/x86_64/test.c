#include "imp.h"
#include "test.h"
#include "boot_mode.h"
#include "timer.h"

char test_boot_text[] =
    "Boot mode test\n"
    "Starting boot mode test...\n";

void test_boot(void)
{
    /* Set boot mode to SHELL */
    ortos_boot_mode_set(ORTOS_BOOT_MODE_SHELL);

    /* Verify the mode */
    if (ortos_boot_mode_get() != ORTOS_BOOT_MODE_SHELL) {
        imp_text("BOOT MODE TEST: FAIL\n");
        return;
    }

    imp_text("BOOT MODE TEST: PASS\n");
    imp_text("Boot mode: SHELL\n");

    /* 5 second countdown */
    imp_text("Returning to shell in:\n");

    for (int i = 5; i > 0; i--) {
        imp_text("  ");
        
        if (i == 5)
            imp_text("5");
        else if (i == 4)
            imp_text("4");
        else if (i == 3)
            imp_text("3");
        else if (i == 2)
            imp_text("2");
        else if (i == 1)
            imp_text("1");

        imp_text("...\n");

        timer_sleep(1000);
    }

    imp_text("Starting shell!\n");
}