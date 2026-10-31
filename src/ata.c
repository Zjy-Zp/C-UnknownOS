#include "ata.h"
#include "ahci.h"

/* ATA PIO (LBA28) —— primary master = 0x1F0 */

#define ATA_DATA   0x1F0
#define ATA_ERROR  0x1F1
#define ATA_SECCNT 0x1F2
#define ATA_LBA0   0x1F3
#define ATA_LBA1   0x1F4
#define ATA_LBA2   0x1F5
#define ATA_DRIVE  0x1F6
#define ATA_STATUS 0x1F7
#define ATA_CMD    0x1F7
#define ATA_CTRL   0x3F6

#define ST_ERR  0x01
#define ST_DRQ  0x08
#define ST_DF   0x20
#define ST_RDY  0x40
#define ST_BSY  0x80

static int present = 0;
static u32 sectors = 0;
static int use_ahci = 0;

static void delay400(void)
{
    for (int i = 0; i < 4; i++) inb(ATA_STATUS);
}

static int wait_not_busy(void)
{
    for (u32 i = 0; i < 1000000; i++) {
        u8 st = inb(ATA_STATUS);
        if (st == 0) return 0;                 /* 无设备 */
        if (!(st & ST_BSY)) return 1;
    }
    return 0;
}

static int wait_drq(void)
{
    for (u32 i = 0; i < 2000000; i++) {
        u8 st = inb(ATA_STATUS);
        if (st & ST_ERR) return 0;
        if (st & ST_DF)  return 0;
        if (!(st & ST_BSY) && (st & ST_DRQ)) return 1;
    }
    return 0;
}

int ata_present(void) { return present; }
u32 ata_sector_count(void) { return sectors; }

static int ide_probe(void)
{
    present = 0;
    outb(ATA_CTRL, 0x02);          /* 关闭中断，纯 PIO 轮询 */
    outb(ATA_DRIVE, 0xA0);         /* master, LBA */
    delay400();

    /* IDENTIFY */
    outb(ATA_SECCNT, 0);
    outb(ATA_LBA0, 0);
    outb(ATA_LBA1, 0);
    outb(ATA_LBA2, 0);
    outb(ATA_CMD, 0xEC);
    delay400();

    u8 st = inb(ATA_STATUS);
    if (st == 0) return 0;         /* 没有设备 */
    if (!wait_not_busy()) return 0;

    /* 非 ATA 设备（如 ATAPI）会在 LBA1/LBA2 留魔数 */
    if (inb(ATA_LBA1) != 0 || inb(ATA_LBA2) != 0) return 0;

    if (!wait_drq()) return 0;

    u16 id[256];
    insw(ATA_DATA, id, 256);

    u32 lba48 = 0;
    if (id[83] & (1u << 10))
        lba48 = (u32)id[100] | ((u32)id[101] << 16);

    u32 lba28 = (u32)id[60] | ((u32)id[61] << 16);
    sectors = lba48 ? lba48 : lba28;
    present = (sectors > 0);
    return present;
}

int ata_is_ahci(void) { return use_ahci; }

int ata_init(void)
{
    use_ahci = 0;
    if (ide_probe()) return 1;

    /* 传统 IDE 兼容口不可用（AHCI / SATA 控制器）→ 走 AHCI */
    if (ahci_init()) {
        use_ahci = 1;
        present = 1;
        sectors = ahci_sector_count();
        return 1;
    }
    present = 0;
    sectors = 0;
    return 0;
}

static int setup(u32 lba, u32 count)
{
    if (!present) return 0;
    if (count == 0 || count > 256) return 0;

    if (!wait_not_busy()) return 0;

    outb(ATA_DRIVE, (u8)(0xE0 | ((lba >> 24) & 0x0F)));
    outb(ATA_SECCNT, (u8)count);
    outb(ATA_LBA0, (u8)(lba & 0xFF));
    outb(ATA_LBA1, (u8)((lba >> 8) & 0xFF));
    outb(ATA_LBA2, (u8)((lba >> 16) & 0xFF));
    return 1;
}

int ata_read_sectors(u32 lba, u32 count, void *buf)
{
    u8 *p = (u8 *)buf;
    if (use_ahci) return ahci_read_sectors(lba, count, buf);
    while (count) {
        u32 n = count > 256 ? 256 : count;
        if (!setup(lba, n)) return 0;
        outb(ATA_CMD, 0x20);                 /* READ SECTORS */
        delay400();

        for (u32 i = 0; i < n; i++) {
            if (!wait_drq()) return 0;
            insw(ATA_DATA, p + i * 512, 256);
            delay400();
        }
        lba += n;
        count -= n;
        p += n * 512;
    }
    return 1;
}

int ata_write_sectors(u32 lba, u32 count, const void *buf)
{
    const u8 *p = (const u8 *)buf;
    if (use_ahci) return ahci_write_sectors(lba, count, buf);
    while (count) {
        u32 n = count > 256 ? 256 : count;
        if (!setup(lba, n)) return 0;
        outb(ATA_CMD, 0x30);                 /* WRITE SECTORS */
        delay400();

        for (u32 i = 0; i < n; i++) {
            if (!wait_drq()) return 0;
            outsw(ATA_DATA, p + i * 512, 256);
            delay400();
        }
        /* FLUSH CACHE */
        outb(ATA_CMD, 0xE7);
        if (!wait_not_busy()) return 0;

        lba += n;
        count -= n;
        p += n * 512;
    }
    return 1;
}
