#include "imp.h"
#include "test.h"
#include "boot_mode.h"

char test_boot_text[] =
    "Boot mode test\n"
    "If you see this message, the boot mode test has passed.\n";

void test_boot(void)
{
    /* Set boot mode to SHELL */
    ortos_boot_mode_set(ORTOS_BOOT_MODE_SHELL);

    /* Read the mode back and verify it */
    if (ortos_boot_mode_get() == ORTOS_BOOT_MODE_SHELL) {
        imp_text("BOOT MODE TEST: PASS\n");
        imp_text("Boot mode is SHELL.\n");
    } else {
        imp_text("BOOT MODE TEST: FAIL\n");
        imp_text("Boot mode was not set correctly.\n");
    }
}