#include "ahci.h"
#include "pci.h"

/* ---- AHCI (SATA) 最小驱动：单端口、命令槽 0、LBA48 DMA ---- */

#define HBA_CAP    0x00
#define HBA_GHC    0x04
#define HBA_IS     0x08
#define HBA_PI     0x0C
#define HBA_VS     0x10
#define PRT_BASE   0x100
#define PRT_SZ     0x80

#define P_CLB      0x00
#define P_CLBU     0x04
#define P_FB       0x08
#define P_FBU      0x0C
#define P_IS       0x10
#define P_IE       0x14
#define P_CMD      0x18
#define P_TFD      0x20
#define P_SIG      0x24
#define P_SSTS     0x28
#define P_SERR     0x30
#define P_CI       0x38

#define CMD_ST     (1u << 0)
#define CMD_SUD    (1u << 1)
#define CMD_POD    (1u << 2)
#define CMD_FRE    (1u << 4)
#define CMD_FR     (1u << 14)
#define CMD_CR     (1u << 15)

#define PXIS_TFES  (1u << 30)
#define PXIS_HBFS  (1u << 29)

#define SIG_ATA    0x00000101

static volatile u8 *abar;
static int  ready;
static int  px_port;
static u32  sec_total;
static u8  *cl_buf;      /* command list，1KB 对齐 */
static u8  *fis_buf;     /* FIS receive area，256B 对齐 */
static u8  *ct_buf;      /* command table，128B 对齐 */

static inline u32 rr(u32 off) { return *(volatile u32 *)(abar + off); }
static inline void wr(u32 off, u32 v) { *(volatile u32 *)(abar + off) = v; }

static void *aligned_mem(u32 size, u32 align)
{
    u8 *raw = (u8 *)kmalloc(size + align + 16);
    u32 a;
    if (!raw) return NULL;
    a = ((u32)raw + align - 1) & ~(align - 1);
    memset((void *)a, 0, size);
    return (void *)a;
}

int ahci_present(void) { return ready; }
u32 ahci_sector_count(void) { return sec_total; }
u32 ahci_port(void) { return (u32)px_port; }

static int port_wait_idle(u32 px)
{
    for (u32 i = 0; i < 20000000u; i++) {
        if (!(rr(px + P_CI) & 1u)) return 1;
        cpu_pause();
    }
    return 0;
}

/* 发一条 ATA 命令：op 命令码，write 标识，count==0 表示无数据（FLUSH） */
static int port_cmd(u8 op, int write, u32 lba, u32 count, void *buf)
{
    u32 px = PRT_BASE + (u32)px_port * PRT_SZ;
    u32 *hdr;
    u32 *prd;

    if (!ready) return 0;
    if (!port_wait_idle(px)) return 0;

    /* ---- FIS：Register H2D ---- */
    memset(ct_buf, 0, 256);
    ct_buf[0]  = 0x27;                 /* FIS type H2D */
    ct_buf[1]  = 0x80;                 /* C = 1 */
    ct_buf[2]  = op;
    ct_buf[3]  = 0;
    ct_buf[4]  = (u8)(lba & 0xFF);
    ct_buf[5]  = (u8)((lba >> 8) & 0xFF);
    ct_buf[6]  = (u8)((lba >> 16) & 0xFF);
    ct_buf[7]  = 0x40;                 /* device: LBA 模式 */
    ct_buf[8]  = (u8)((lba >> 24) & 0xFF);
    ct_buf[9]  = 0;
    ct_buf[10] = 0;
    ct_buf[11] = 0;
    ct_buf[12] = (u8)(count & 0xFF);
    ct_buf[13] = (u8)((count >> 8) & 0xFF);

    /* ---- 命令头（槽 0） ---- */
    hdr = (u32 *)cl_buf;
    hdr[0] = 5u | (write ? (1u << 6) : 0u) | (count ? (1u << 16) : 0u);
    hdr[1] = 0;
    hdr[2] = (u32)ct_buf;
    hdr[3] = 0;
    hdr[4] = 0; hdr[5] = 0; hdr[6] = 0; hdr[7] = 0;

    if (count) {
        prd = (u32 *)(ct_buf + 0x80);
        prd[0] = (u32)buf;
        prd[1] = 0;
        prd[2] = 0;
        prd[3] = count * 512u - 1u;
    }

    wr(px + P_IS, 0xFFFFFFFF);
    wr(px + P_SERR, 0xFFFFFFFF);
    wr(px + P_CI, 1u);

    for (u32 i = 0; i < 40000000u; i++) {
        if (!(rr(px + P_CI) & 1u)) {
            u32 isr = rr(px + P_IS);
            if (isr & (PXIS_TFES | PXIS_HBFS)) return 0;
            return 1;
        }
        cpu_pause();
    }
    return 0;
}

static int identify(void)
{
    u16 id[256];
    u32 lba28, lba48;

    if (!port_cmd(0xEC, 0, 0, 1, id)) return 0;
    if (id[0] == 0 || id[0] == 0xFFFF) return 0;

    lba28 = (u32)id[60] | ((u32)id[61] << 16);
    lba48 = 0;
    if (id[83] & (1u << 10)) {
        lba48 = (u32)id[100] | ((u32)id[101] << 16);
    }
    sec_total = lba48 ? lba48 : lba28;
    return sec_total > 0;
}

static int port_start(u32 px)
{
    u32 cmd = rr(px + P_CMD);

    if (cmd & CMD_ST) wr(px + P_CMD, cmd & ~CMD_ST);
    for (u32 i = 0; i < 2000000u; i++) {
        if (!(rr(px + P_CMD) & CMD_CR)) break;
        cpu_pause();
    }

    wr(px + P_CLB, 0); wr(px + P_CLBU, 0);
    wr(px + P_FB, 0);  wr(px + P_FBU, 0);
    for (u32 i = 0; i < 2000000u; i++) {
        if (!(rr(px + P_CMD) & CMD_FR)) break;
        cpu_pause();
    }

    wr(px + P_CLB, (u32)cl_buf);
    wr(px + P_CLBU, 0);
    wr(px + P_FB, (u32)fis_buf);
    wr(px + P_FBU, 0);
    wr(px + P_IE, 0);
    wr(px + P_IS, 0xFFFFFFFF);
    wr(px + P_SERR, 0xFFFFFFFF);
    wr(px + P_CMD, CMD_FRE | CMD_ST | CMD_SUD | CMD_POD);
    return 1;
}

int ahci_init(void)
{
    int i, port;
    u32 pi;

    ready = 0;
    sec_total = 0;
    abar = NULL;

    for (i = 0; i < pci_count(); i++) {
        const pci_dev *d = pci_at(i);
        u32 bar;
        if (!d) continue;
        if (d->class_code != 0x01 || d->subclass != 0x06) continue;
        bar = d->bar[5] & 0xFFFFFFF0u;
        if (!bar || bar == 0xFFFFFFF0u) continue;
        abar = (volatile u8 *)bar;
        break;
    }
    if (!abar) return 0;

    /* 打开 AHCI 模式 */
    wr(HBA_GHC, rr(HBA_GHC) | (1u << 31));

    cl_buf  = (u8 *)aligned_mem(1024, 1024);
    fis_buf = (u8 *)aligned_mem(256, 256);
    ct_buf  = (u8 *)aligned_mem(256, 128);
    if (!cl_buf || !fis_buf || !ct_buf) return 0;

    pi = rr(HBA_PI);
    for (port = 0; port < 32; port++) {
        u32 px;
        if (!(pi & (1u << port))) continue;
        px = PRT_BASE + (u32)port * PRT_SZ;
        if ((rr(px + P_SSTS) & 0x0F) != 3) continue;   /* DET: 已建立连接 */
        if (rr(px + P_SIG) != SIG_ATA) continue;       /* 只取 ATA 硬盘 */
        px_port = port;
        ready = 1;
        port_start(px);
        if (identify()) return 1;
        ready = 0;
    }
    return 0;
}

int ahci_read_sectors(u32 lba, u32 count, void *buf)
{
    u8 *p = (u8 *)buf;
    while (count) {
        u32 n = count > 120u ? 120u : count;
        if (!port_cmd(0x25, 0, lba, n, p)) return 0;
        lba += n; count -= n; p += n * 512u;
    }
    return 1;
}

int ahci_write_sectors(u32 lba, u32 count, const void *buf)
{
    const u8 *p = (const u8 *)buf;
    while (count) {
        u32 n = count > 120u ? 120u : count;
        if (!port_cmd(0x35, 1, lba, n, (void *)p)) return 0;
        lba += n; count -= n; p += n * 512u;
    }
    return port_cmd(0xEA, 0, 0, 0, NULL);
}
