#ifndef AHCI_H
#define AHCI_H

#include "common.h"

int  ahci_init(void);
int  ahci_present(void);
u32  ahci_sector_count(void);
u32  ahci_port(void);
int  ahci_read_sectors(u32 lba, u32 count, void *buf);
int  ahci_write_sectors(u32 lba, u32 count, const void *buf);

#endif
