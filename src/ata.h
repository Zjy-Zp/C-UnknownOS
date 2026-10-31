#ifndef ATA_H
#define ATA_H

#include "common.h"

int  ata_init(void);
int  ata_read_sectors(u32 lba, u32 count, void *buf);
int  ata_write_sectors(u32 lba, u32 count, const void *buf);
int  ata_present(void);
u32  ata_sector_count(void);
int  ata_is_ahci(void);

#endif
