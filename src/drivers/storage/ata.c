#include "ata.h"
#include "io.h"

#define ATA_DATA 0x1f0
#define ATA_SECTOR_COUNT 0x1f2
#define ATA_LBA_LOW 0x1f3
#define ATA_LBA_MID 0x1f4
#define ATA_LBA_HIGH 0x1f5
#define ATA_DRIVE 0x1f6
#define ATA_STATUS 0x1f7
#define ATA_COMMAND 0x1f7
#define ATA_CONTROL 0x3f6
#define ATA_CMD_IDENTIFY 0xec
#define ATA_CMD_READ 0x20
#define ATA_CMD_WRITE 0x30
#define ATA_CMD_FLUSH_CACHE 0xe7
#define ATA_STATUS_BUSY 0x80
#define ATA_STATUS_READY 0x40
#define ATA_STATUS_ERROR 0x01

static struct block_device primary_master;
static uint16_t identify_data[256];
static int ata_ready;

static int ata_wait(int require_drq)
{
    uint8_t status;
    for (unsigned int attempt = 0; attempt < 1000000u; attempt++) {
        status = io_inb(ATA_STATUS);
        if (status & ATA_STATUS_ERROR) return -1;
        if (!(status & ATA_STATUS_BUSY) && (!require_drq || (status & 8))) return 0;
    }
    return -1;
}

static void ata_select(uint32_t lba)
{
    io_outb(ATA_DRIVE, (uint8_t)(0xe0 | ((lba >> 24) & 0x0f)));
}

static int ata_read_sector(void *context, uint64_t sector, void *buffer)
{
    uint32_t lba = (uint32_t)sector;
    uint16_t *words = buffer;
    (void)context;
    if (!buffer || !ata_ready || sector >= primary_master.sector_count || sector > 0x0fffffffu) return -1;
    ata_select(lba);
    io_outb(ATA_SECTOR_COUNT, 1);
    io_outb(ATA_LBA_LOW, (uint8_t)lba);
    io_outb(ATA_LBA_MID, (uint8_t)(lba >> 8));
    io_outb(ATA_LBA_HIGH, (uint8_t)(lba >> 16));
    io_outb(ATA_COMMAND, ATA_CMD_READ);
    if (ata_wait(1) != 0) return -1;
    for (unsigned int i = 0; i < 256; i++) words[i] = io_inw(ATA_DATA);
    return 0;
}

static int ata_write_sector(void *context, uint64_t sector, const void *buffer)
{
    uint32_t lba = (uint32_t)sector;
    const uint16_t *words = buffer;
    (void)context;
    if (!buffer || !ata_ready || sector >= primary_master.sector_count || sector > 0x0fffffffu) return -1;
    ata_select(lba);
    io_outb(ATA_SECTOR_COUNT, 1);
    io_outb(ATA_LBA_LOW, (uint8_t)lba);
    io_outb(ATA_LBA_MID, (uint8_t)(lba >> 8));
    io_outb(ATA_LBA_HIGH, (uint8_t)(lba >> 16));
    io_outb(ATA_COMMAND, ATA_CMD_WRITE);
    if (ata_wait(1) != 0) return -1;
    for (unsigned int i = 0; i < 256; i++) io_outw(ATA_DATA, words[i]);
    io_inb(ATA_STATUS);
    return ata_wait(0);
}

static int ata_flush(void *context)
{
    (void)context;
    if (!ata_ready) return -1;
    ata_select(0);
    io_outb(ATA_COMMAND, ATA_CMD_FLUSH_CACHE);
    return ata_wait(0);
}

int ata_primary_master_init(void)
{
    uint8_t status;
    ata_ready = 0;
    io_outb(ATA_CONTROL, 2);
    ata_select(0);
    io_outb(ATA_SECTOR_COUNT, 0);
    io_outb(ATA_LBA_LOW, 0);
    io_outb(ATA_LBA_MID, 0);
    io_outb(ATA_LBA_HIGH, 0);
    io_outb(ATA_COMMAND, ATA_CMD_IDENTIFY);
    status = io_inb(ATA_STATUS);
    if (!status || ata_wait(1) != 0) return -1;
    for (unsigned int i = 0; i < 256; i++) identify_data[i] = io_inw(ATA_DATA);
    primary_master.context = 0;
    primary_master.sector_count = ((uint64_t)identify_data[61] << 16) | identify_data[60];
    primary_master.name = "disk0";
    primary_master.type = "ATA/IDE";
    primary_master.read_sector = ata_read_sector;
    primary_master.write_sector = ata_write_sector;
    primary_master.flush = ata_flush;
    ata_ready = primary_master.sector_count != 0;
    return ata_ready ? 0 : -1;
}

const struct block_device *ata_primary_master_device(void)
{
    return ata_ready ? &primary_master : 0;
}
