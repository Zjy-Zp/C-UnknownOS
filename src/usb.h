#ifndef USB_H
#define USB_H

#include "common.h"

enum { USB_UHCI = 0, USB_OHCI = 1, USB_EHCI = 2, USB_XHCI = 3 };

typedef struct {
    int   kind;
    u16   vendor, device;
    u8    bus, slot, func;
    u32   io_base;      /* UHCI / OHCI 的 IO BAR */
    u32   mem_base;     /* EHCI / XHCI 的 MMIO BAR */
    int   ports;        /* 根端口数 */
    int   connected;    /* 有设备插入的根端口数 */
    u32   port_bits;    /* 已连接端口位图（bit0 = port1） */
} usb_ctrl;

/* 枚举 PCI 上的 USB 控制器，初始化并探测根端口，返回控制器数量 */
int  usb_init(void);
int  usb_count(void);
const usb_ctrl *usb_at(int i);
const char    *usb_kind_name(int kind);
int  usb_total_devices(void);

#endif
