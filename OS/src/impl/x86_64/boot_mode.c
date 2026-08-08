#include "boot_mode.h"
#include "io.h"

/*
 * QEMU/SeaBIOS retains CMOS NVRAM across an i8042 CPU reset and GRUB does not
 * consume the high, implementation-defined CMOS bytes used below.  This is
 * therefore safe for the project's development target and survives the real
 * reset, unlike a C variable or the kernel's memory at and above 1 MiB.
 */
#define ORTOS_CMOS_INDEX_PORT 0x70u
#define ORTOS_CMOS_DATA_PORT  0x71u
#define ORTOS_CMOS_MAGIC_REG  0x5Au
#define ORTOS_CMOS_MODE_REG   0x5Bu
#define ORTOS_CMOS_CHECK_REG  0x5Cu
#define ORTOS_CMOS_MAGIC      0xA7u
#define ORTOS_CMOS_GUI        0x31u
#define ORTOS_CMOS_CHECK      (ORTOS_CMOS_MAGIC ^ ORTOS_CMOS_GUI ^ 0x5Du)

static uint8_t cmos_read(uint8_t reg)
{
    io_outb(ORTOS_CMOS_INDEX_PORT, (uint8_t)(0x80u | reg));
    return io_inb(ORTOS_CMOS_DATA_PORT);
}

static void cmos_write(uint8_t reg, uint8_t value)
{
    io_outb(ORTOS_CMOS_INDEX_PORT, (uint8_t)(0x80u | reg));
    io_outb(ORTOS_CMOS_DATA_PORT, value);
}

void ortos_boot_mode_set(ortos_boot_mode_t mode)
{
    if (mode == ORTOS_BOOT_GUI) {
        cmos_write(ORTOS_CMOS_MAGIC_REG, ORTOS_CMOS_MAGIC);
        cmos_write(ORTOS_CMOS_MODE_REG, ORTOS_CMOS_GUI);
        cmos_write(ORTOS_CMOS_CHECK_REG, ORTOS_CMOS_CHECK);
    } else {
        ortos_boot_mode_clear();
    }
}

void ortos_boot_mode_clear(void)
{
    cmos_write(ORTOS_CMOS_MAGIC_REG, 0u);
    cmos_write(ORTOS_CMOS_MODE_REG, 0u);
    cmos_write(ORTOS_CMOS_CHECK_REG, 0u);
}

ortos_boot_mode_t ortos_boot_mode_get(void)
{
    if (cmos_read(ORTOS_CMOS_MAGIC_REG) == ORTOS_CMOS_MAGIC &&
        cmos_read(ORTOS_CMOS_MODE_REG) == ORTOS_CMOS_GUI &&
        cmos_read(ORTOS_CMOS_CHECK_REG) == ORTOS_CMOS_CHECK) {
        return ORTOS_BOOT_GUI;
    }
    return ORTOS_BOOT_SHELL;
}

void ortos_reboot(void)
{
    asm volatile ("cli");

    /* Wait until the i8042 can accept the reset command. */
    for (uint32_t timeout = 0; timeout < 100000u; timeout++) {
        if ((io_inb(0x64u) & 0x02u) == 0u) {
            io_outb(0x64u, 0xFEu);
            break;
        }
    }

    for (;;)
        asm volatile ("hlt");
}
