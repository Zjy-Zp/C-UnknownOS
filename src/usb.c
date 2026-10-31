#include "usb.h"
#include "pci.h"

#define USB_MAX_CTRL 8

/* UHCI 寄存器（IO 空间，相对 BAR0） */
#define UHCI_USBCMD     0x00
#define UHCI_USBSTS     0x02
#define UHCI_PORTSC1    0x10
#define UHCI_PORTSC2    0x12
#define UHCI_CMD_HCRESET 0x0002

/* EHCI 寄存器（MMIO） */
#define EHCI_CAPLENGTH  0x00
#define EHCI_HCSPARAMS  0x04
#define EHCI_USBCMD     0x00
#define EHCI_PORTSC     0x44
#define EHCI_CMD_RESET  0x0002

static usb_ctrl ctrls[USB_MAX_CTRL];
static int ctrl_count = 0;

const char *usb_kind_name(int kind)
{
    switch (kind) {
    case USB_UHCI: return "UHCI (USB 1.1)";
    case USB_OHCI: return "OHCI (USB 1.1)";
    case USB_EHCI: return "EHCI (USB 2.0)";
    case USB_XHCI: return "XHCI (USB 3.0)";
    default:       return "USB controller";
    }
}

int usb_count(void) { return ctrl_count; }
const usb_ctrl *usb_at(int i) { return (i >= 0 && i < ctrl_count) ? &ctrls[i] : 0; }

int usb_total_devices(void)
{
    int n = 0;
    for (int i = 0; i < ctrl_count; i++) n += ctrls[i].connected;
    return n;
}

/* ---- UHCI ---- */
static void uhci_probe(usb_ctrl *c)
{
    u16 io = (u16)(c->io_base & 0xFFE0);
    c->io_base = io;

    /* 全局复位 */
    outw((u16)(io + UHCI_USBCMD), UHCI_CMD_HCRESET);
    for (u32 i = 0; i < 100000; i++) {
        if (!(inw((u16)(io + UHCI_USBCMD)) & UHCI_CMD_HCRESET)) break;
        cpu_pause();
    }
    /* 启动控制器：USBCMD = 0x0001 (Run) */
    outw((u16)(io + UHCI_USBCMD), 0x0001);
    for (u32 i = 0; i < 10000; i++) cpu_pause();

    c->ports = 2;
    c->connected = 0;
    c->port_bits = 0;
    for (int p = 0; p < 2; p++) {
        u16 st = inw((u16)(io + UHCI_PORTSC1 + p * 2));
        if (st & 0x0001) {                 /* CCS：设备已连接 */
            c->connected++;
            c->port_bits |= (1u << p);
        }
    }
}

/* ---- EHCI ---- */
static void ehci_probe(usb_ctrl *c)
{
    u8 *mmio = (u8 *)c->mem_base;
    if (!mmio) { c->ports = 0; return; }

    u32 cap_len = *(volatile u8 *)(mmio + EHCI_CAPLENGTH);
    u32 hcs = *(volatile u32 *)(mmio + EHCI_HCSPARAMS);
    int nports = (int)(hcs & 0x0F);
    if (nports <= 0 || nports > 16) nports = 4;
    c->ports = nports;
    c->connected = 0;
    c->port_bits = 0;

    u8 *op = mmio + cap_len;

    /* 复位控制器后启动 */
    u32 cmd = *(volatile u32 *)(op + EHCI_USBCMD);
    cmd |= EHCI_CMD_RESET;
    *(volatile u32 *)(op + EHCI_USBCMD) = cmd;
    for (u32 i = 0; i < 100000; i++) {
        if (!(*(volatile u32 *)(op + EHCI_USBCMD) & EHCI_CMD_RESET)) break;
        cpu_pause();
    }
    *(volatile u32 *)(op + EHCI_USBCMD) = 0x0001;   /* Run */
    for (u32 i = 0; i < 10000; i++) cpu_pause();

    for (int p = 0; p < nports; p++) {
        u32 st = *(volatile u32 *)(op + EHCI_PORTSC + p * 4);
        if (st & 0x0001) {                 /* CCS */
            c->connected++;
            c->port_bits |= (1u << p);
        }
    }
}

/* ---- OHCI ---- */
static void ohci_probe(usb_ctrl *c)
{
    c->ports = 2;   /* 保守估计，OHCI 端口数在 HcRhDescriptorA 中 */
    c->connected = 0;
    c->port_bits = 0;
}

int usb_init(void)
{
    ctrl_count = 0;
    int n = pci_usb_count();
    for (int i = 0; i < n && ctrl_count < USB_MAX_CTRL; i++) {
        const pci_dev *d = pci_usb(i);
        if (!d) continue;
        usb_ctrl *c = &ctrls[ctrl_count];
        c->vendor = d->vendor;
        c->device = d->device;
        c->bus = d->bus;
        c->slot = d->slot;
        c->func = d->func;
        c->io_base = 0;
        c->mem_base = 0;
        c->ports = 0;
        c->connected = 0;
        c->port_bits = 0;

        if (d->prog_if == 0x00) {          /* UHCI */
            c->kind = USB_UHCI;
            c->io_base = d->bar[4] & 0xFFE0u;
            if (c->io_base) uhci_probe(c);
        } else if (d->prog_if == 0x10) {   /* OHCI */
            c->kind = USB_OHCI;
            c->mem_base = d->bar[0] & 0xFFFFFFF0u;
            ohci_probe(c);
        } else if (d->prog_if == 0x20) {   /* EHCI */
            c->kind = USB_EHCI;
            c->mem_base = d->bar[0] & 0xFFFFFFF0u;
            ehci_probe(c);
        } else if (d->prog_if == 0x30) {   /* XHCI */
            c->kind = USB_XHCI;
            c->mem_base = d->bar[0] & 0xFFFFFFF0u;
            c->ports = 4;
        } else {
            continue;
        }
        ctrl_count++;
    }
    return ctrl_count;
}
