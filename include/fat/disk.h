#pragma once

#include "stdint.h"
#include "stdbool.h"

typedef struct 
{
    /* data */
    uint8_t id;
    uint16_t cylinders;
    uint16_t sectors;
    uint16_t heads;
} DISK;

bool Disk_Init(DISK* disk, uint8_t driveNum);
bool Disk_Read_Sector(DISK* disk, uint32_t lba, uint8_t sectors, void far* dataOut);
