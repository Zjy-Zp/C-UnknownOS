#include "pci.h"

#define PCI_CONFIG_ADDR 0xCF8
#define PCI_CONFIG_DATA 0xCFC
#define PCI_MAX_DEV     128

static pci_dev devs[PCI_MAX_DEV];
static int     dev_count = 0;

static inline u32 cfg_addr(u8 bus, u8 slot, u8 func, u8 off)
{
    return 0x80000000u
         | ((u32)bus << 16)
         | ((u32)(slot & 0x1F) << 11)
         | ((u32)(func & 0x07) << 8)
         | (u32)(off & 0xFC);
}

u32 pci_read32(u8 bus, u8 slot, u8 func, u8 off)
{
    outl(PCI_CONFIG_ADDR, cfg_addr(bus, slot, func, off));
    return inl(PCI_CONFIG_DATA);
}

u16 pci_read16(u8 bus, u8 slot, u8 func, u8 off)
{
    u32 v = pci_read32(bus, slot, func, (u8)(off & 0xFC));
    return (u16)((v >> ((off & 2) * 8)) & 0xFFFF);
}

void pci_write32(u8 bus, u8 slot, u8 func, u8 off, u32 val)
{
    outl(PCI_CONFIG_ADDR, cfg_addr(bus, slot, func, off));
    outl(PCI_CONFIG_DATA, val);
}

void pci_scan(void)
{
    dev_count = 0;
    for (u32 bus = 0; bus < 256 && dev_count < PCI_MAX_DEV; bus++) {
        int bus_has_dev = 0;
        for (u32 slot = 0; slot < 32 && dev_count < PCI_MAX_DEV; slot++) {
            for (u32 func = 0; func < 8 && dev_count < PCI_MAX_DEV; func++) {
                u32 id = pci_read32((u8)bus, (u8)slot, (u8)func, 0);
                u16 vendor = (u16)(id & 0xFFFF);
                if (vendor == 0xFFFF) {
                    if (func == 0) break;   /* 单功能设备，跳到下一个 slot */
                    continue;
                }
                bus_has_dev = 1;
                pci_dev *d = &devs[dev_count++];
                d->bus = (u8)bus;
                d->slot = (u8)slot;
                d->func = (u8)func;
                d->vendor = vendor;
                d->device = (u16)(id >> 16);

                u32 cls = pci_read32((u8)bus, (u8)slot, (u8)func, 8);
                d->revision    = (u8)(cls & 0xFF);
                d->prog_if     = (u8)((cls >> 8) & 0xFF);
                d->subclass    = (u8)((cls >> 16) & 0xFF);
                d->class_code  = (u8)((cls >> 24) & 0xFF);

                for (int b = 0; b < 6; b++)
                    d->bar[b] = pci_read32((u8)bus, (u8)slot, (u8)func, (u8)(0x10 + b * 4));

                d->irq = (u8)(pci_read32((u8)bus, (u8)slot, (u8)func, 0x3C) & 0xFF);
            }
        }
        if (!bus_has_dev && bus > 0) continue;
    }
}

int pci_count(void) { return dev_count; }
const pci_dev *pci_at(int i) { return (i >= 0 && i < dev_count) ? &devs[i] : 0; }

int pci_gpu_count(void)
{
    int n = 0;
    for (int i = 0; i < dev_count; i++) if (devs[i].class_code == PCI_CLASS_DISPLAY) n++;
    return n;
}

const pci_dev *pci_gpu(int i)
{
    int n = 0;
    for (int k = 0; k < dev_count; k++) {
        if (devs[k].class_code == PCI_CLASS_DISPLAY) {
            if (n == i) return &devs[k];
            n++;
        }
    }
    return 0;
}

int pci_usb_count(void)
{
    int n = 0;
    for (int i = 0; i < dev_count; i++)
        if (devs[i].class_code == PCI_CLASS_SERIAL && devs[i].subclass == PCI_SUBCLASS_USB) n++;
    return n;
}

const pci_dev *pci_usb(int i)
{
    int n = 0;
    for (int k = 0; k < dev_count; k++) {
        if (devs[k].class_code == PCI_CLASS_SERIAL && devs[k].subclass == PCI_SUBCLASS_USB) {
            if (n == i) return &devs[k];
            n++;
        }
    }
    return 0;
}

int pci_ide_count(void)
{
    int n = 0;
    for (int i = 0; i < dev_count; i++)
        if (devs[i].class_code == 0x01 && devs[i].subclass == PCI_SUBCLASS_IDE) n++;
    return n;
}

const pci_dev *pci_ide(int i)
{
    int n = 0;
    for (int k = 0; k < dev_count; k++) {
        if (devs[k].class_code == 0x01 && devs[k].subclass == PCI_SUBCLASS_IDE) {
            if (n == i) return &devs[k];
            n++;
        }
    }
    return 0;
}

const char *pci_vendor_name(u16 vendor)
{
    switch (vendor) {
    case 0x1002: return "AMD";
    case 0x10DE: return "NVIDIA";
    case 0x8086: return "Intel";
    case 0x1234: return "QEMU/Bochs";
    case 0x1AF4: return "Virtio";
    case 0x15AD: return "VMware";
    case 0x1B36: return "Red Hat";
    case 0x106B: return "Apple";
    case 0x100C: return "Tseng";
    case 0x5333: return "S3";
    default:     return "Unknown";
    }
}

const char *pci_gpu_family(u16 vendor)
{
    switch (vendor) {
    case 0x1002: return "AMD Radeon (AMDGPU)";
    case 0x10DE: return "NVIDIA GeForce (nouveau)";
    case 0x8086: return "Intel Graphics (i915)";
    case 0x1234: return "Bochs VBE (QEMU std VGA)";
    default:     return "Generic VGA";
    }
}

const char *pci_class_name(u8 cls, u8 sub)
{
    if (cls == 0x03) {
        if (sub == 0x00) return "VGA compatible";
        if (sub == 0x02) return "3D controller";
        return "Display controller";
    }
    if (cls == 0x01) return "Storage controller";
    if (cls == 0x0C) return "Serial bus controller";
    if (cls == 0x02) return "Network controller";
    if (cls == 0x06) return "Bridge";
    return "Other";
}
