#pragma once

#include "block_device.h"

int ata_primary_master_init(void);
const struct block_device *ata_primary_master_device(void);
