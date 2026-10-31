#ifndef PCI_H
#define PCI_H

#include "common.h"

typedef struct {
    u8  bus, slot, func;
    u16 vendor, device;
    u8  class_code, subclass, prog_if, revision;
    u32 bar[6];
    u8  irq;
} pci_dev;

u32  pci_read32(u8 bus, u8 slot, u8 func, u8 off);
u16  pci_read16(u8 bus, u8 slot, u8 func, u8 off);
void pci_write32(u8 bus, u8 slot, u8 func, u8 off, u32 val);

void pci_scan(void);
int  pci_count(void);
const pci_dev *pci_at(int i);

int  pci_gpu_count(void);
const pci_dev *pci_gpu(int i);
int  pci_usb_count(void);
const pci_dev *pci_usb(int i);
int  pci_ide_count(void);
const pci_dev *pci_ide(int i);

const char *pci_vendor_name(u16 vendor);
const char *pci_gpu_family(u16 vendor);
const char *pci_class_name(u8 cls, u8 sub);

#define PCI_CLASS_DISPLAY   0x03
#define PCI_CLASS_SERIAL    0x0C
#define PCI_SUBCLASS_USB    0x03
#define PCI_SUBCLASS_IDE    0x01

#endif
