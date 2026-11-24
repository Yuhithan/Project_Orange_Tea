#include "fat/disk.h"
#include "x86_64/port.h"

#define ATA_PRIMARY_IO 0x1F0
#define ATA_PRIMARY_CONTROL 0x3F6
#define ATA_DATA (ATA_PRIMARY_IO + 0)
#define ATA_SECTOR_COUNT (ATA_PRIMARY_IO + 2)
#define ATA_LBA_LOW (ATA_PRIMARY_IO + 3)
#define ATA_LBA_MID (ATA_PRIMARY_IO + 4)
#define ATA_LBA_HIGH (ATA_PRIMARY_IO + 5)
#define ATA_DRIVE (ATA_PRIMARY_IO + 6)
#define ATA_STATUS (ATA_PRIMARY_IO + 7)
#define ATA_COMMAND (ATA_PRIMARY_IO + 7)

#define ATA_STATUS_ERROR 0x01
#define ATA_STATUS_DEVICE_FAULT 0x20
#define ATA_STATUS_DATA_REQUEST 0x08
#define ATA_STATUS_BUSY 0x80
#define ATA_COMMAND_READ_SECTORS 0x20
#define ATA_LBA28_MAX 0x0FFFFFFF
#define ATA_POLL_LIMIT 1000000U

static void ata_delay_400ns(void)
{
    for (unsigned int i = 0; i < 4; i++) {
        port_inb(ATA_PRIMARY_CONTROL);
    }
}

static bool ata_wait_not_busy(void)
{
    for (unsigned int i = 0; i < ATA_POLL_LIMIT; i++) {
        uint8_t status = port_inb(ATA_STATUS);

        if (status == 0 || status == 0xFF) {
            return false;
        }
        if ((status & ATA_STATUS_BUSY) == 0) {
            return true;
        }
    }

    return false;
}

static bool ata_wait_for_data(void)
{
    for (unsigned int i = 0; i < ATA_POLL_LIMIT; i++) {
        uint8_t status = port_inb(ATA_STATUS);

        if (status == 0 || status == 0xFF ||
            (status & (ATA_STATUS_ERROR | ATA_STATUS_DEVICE_FAULT)) != 0) {
            return false;
        }
        if ((status & ATA_STATUS_BUSY) == 0 &&
            (status & ATA_STATUS_DATA_REQUEST) != 0) {
            return true;
        }
    }

    return false;
}

bool disk_read_sector(uint32_t lba, uint8_t *buffer)
{
    if (buffer == 0 || lba > ATA_LBA28_MAX) {
        return false;
    }

    port_outb(ATA_PRIMARY_CONTROL, 0x02);

    if (!ata_wait_not_busy()) {
        return false;
    }

    port_outb(ATA_DRIVE, (uint8_t)(0xE0 | ((lba >> 24) & 0x0F)));
    ata_delay_400ns();

    if (!ata_wait_not_busy()) {
        return false;
    }

    port_outb(ATA_SECTOR_COUNT, 1);
    port_outb(ATA_LBA_LOW, (uint8_t)(lba & 0xFF));
    port_outb(ATA_LBA_MID, (uint8_t)((lba >> 8) & 0xFF));
    port_outb(ATA_LBA_HIGH, (uint8_t)((lba >> 16) & 0xFF));
    port_outb(ATA_COMMAND, ATA_COMMAND_READ_SECTORS);

    if (!ata_wait_for_data()) {
        return false;
    }

    for (unsigned int i = 0; i < 256; i++) {
        uint16_t word = port_inw(ATA_DATA);
        buffer[i * 2] = (uint8_t)(word & 0xFF);
        buffer[i * 2 + 1] = (uint8_t)(word >> 8);
    }

    return true;
}
