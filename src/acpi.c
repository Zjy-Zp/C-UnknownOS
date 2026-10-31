#include "acpi.h"

/* ACPI 关机：RSDP -> RSDT -> FADT -> DSDT 中 \_S5_ 的 SLP_TYP -> 写 PM1a_CNT */

static u8  *fadt = NULL;
static u16  pm1a_cnt = 0;
static u16  pm1b_cnt = 0;
static u8   smi_cmd = 0;
static u8   acpi_enable_val = 0;
static u32  dsdt_addr = 0;
static char oem_id[7];

static int sig_eq(const u8 *p, const char *sig)
{
    for (int i = 0; i < 4; i++)
        if (p[i] != (u8)sig[i]) return 0;
    return 1;
}

static u8 checksum_ok(const u8 *p, u32 len)
{
    u8 sum = 0;
    for (u32 i = 0; i < len; i++) sum = (u8)(sum + p[i]);
    return sum == 0;
}

static u8 *scan_rsdp(u8 *start, u32 len)
{
    for (u32 off = 0; off + 20 <= len; off += 16) {
        u8 *p = start + off;
        if (p[0] == 'R' && p[1] == 'S' && p[2] == 'D' && p[3] == ' ' &&
            p[4] == 'P' && p[5] == 'T' && p[6] == 'R' && p[7] == ' ' &&
            checksum_ok(p, 20))
            return p;
    }
    return NULL;
}

static u8 *find_rsdp(void)
{
    /* 1) EBDA（0x40E 处为段地址） */
    u16 ebda_seg = *(volatile u16 *)0x040E;
    if (ebda_seg) {
        u8 *p = scan_rsdp((u8 *)(u32)(ebda_seg * 16), 1024);
        if (p) return p;
    }
    /* 2) BIOS 只读区 0xE0000 - 0xFFFFF */
    return scan_rsdp((u8 *)0xE0000, 0x20000);
}

/* 解析 AML PkgLength，返回包长，*consumed 为编码字节数 */
static u32 aml_pkg_length(const u8 *p, int *consumed)
{
    u8 lead = p[0];
    int extra = (lead >> 6) & 0x03;
    if (!extra) { *consumed = 1; return (u32)(lead & 0x3F); }

    u32 len = (u32)(lead & 0x0F);
    for (int i = 0; i < extra; i++)
        len |= (u32)p[1 + i] << (4 + 8 * i);
    *consumed = 1 + extra;
    return len;
}

/* 在 DSDT 中找 \_S5_ 的 SLP_TYPa / SLP_TYPb，返回 1 成功 */
static int parse_s5(u8 *dsdt, u32 len, u16 *slp_typa, u16 *slp_typb)
{
    if (!dsdt) return 0;
    for (u32 i = 0; i + 8 < len; i++) {
        if (!(dsdt[i] == '_' && dsdt[i + 1] == 'S' &&
              dsdt[i + 2] == '5' && dsdt[i + 3] == '_'))
            continue;

        const u8 *p = dsdt + i + 4;
        if (*p != 0x12) continue;               /* PackageOp */
        p++;
        int consumed = 0;
        (void)aml_pkg_length(p, &consumed);
        p += consumed;
        u8 count = *p++;                         /* 元素个数 */
        if (count < 2) continue;

        u16 vals[4] = {0, 0, 0, 0};
        int got = 0;
        for (int e = 0; e < count && e < 4; e++) {
            if (*p == 0x0A) {                    /* BytePrefix */
                vals[e] = p[1];
                p += 2;
                got++;
            } else if (*p == 0x0B) {             /* WordPrefix */
                vals[e] = (u16)(p[1] | (p[2] << 8));
                p += 3;
                got++;
            } else {
                break;
            }
        }
        if (got >= 1) {
            *slp_typa = vals[0];
            *slp_typb = got >= 2 ? vals[1] : vals[0];
            return 1;
        }
    }
    return 0;
}

int acpi_available(void) { return fadt != NULL; }
const char *acpi_oem_id(void) { return oem_id; }

int acpi_init(void)
{
    u8 *rsdp = find_rsdp();
    if (!rsdp) return 0;

    u32 rsdt_addr = *(u32 *)(rsdp + 16);
    if (!rsdt_addr) return 0;

    u8 *rsdt = (u8 *)(u32)rsdt_addr;
    if (!sig_eq(rsdt, "RSDT")) return 0;

    u32 rsdt_len = *(u32 *)(rsdt + 4);
    if (rsdt_len > 4096) rsdt_len = 4096;

    int entries = (int)((rsdt_len - 36) / 4);
    for (int i = 0; i < entries; i++) {
        u32 addr = *(u32 *)(rsdt + 36 + i * 4);
        if (!addr) continue;
        u8 *tbl = (u8 *)(u32)addr;
        if (sig_eq(tbl, "FACP")) { fadt = tbl; break; }
    }
    if (!fadt) return 0;

    for (int i = 0; i < 6; i++) oem_id[i] = (char)fadt[10 + i];
    oem_id[6] = '\0';

    /* FADT 偏移（ACPI 1.0/2.0+ 一致）：
     *   36 FIRMWARE_CTRL  40 DSDT  48 SMI_CMD  52 ACPI_ENABLE
     *   56 PM1a_EVT_BLK   60 PM1b_EVT_BLK  64 PM1a_CNT_BLK  68 PM1b_CNT_BLK */
    smi_cmd         = fadt[48];
    acpi_enable_val = fadt[52];
    pm1a_cnt        = *(u16 *)(fadt + 64);
    pm1b_cnt        = *(u16 *)(fadt + 68);
    dsdt_addr       = *(u32 *)(fadt + 40);
    return 1;
}

static u16 g_slp = 5;
static u16 g_cur = 0;
static u16 g_rb = 0;
static u16 g_dbg[3] = {0,0,0};
static int g_sci_en = 0;
static int g_prepared = 0;

u16 acpi_pm1a_port(void)   { return pm1a_cnt; }
u16 acpi_slp_typ(void)     { return g_slp; }
u16 acpi_pm1cnt_raw(void)  { return g_cur; }
u16 acpi_readback(void)    { return g_rb; }
u16 acpi_dbg(int i)        { return (i >= 0 && i < 3) ? g_dbg[i] : 0; }
int acpi_sci_enabled(void) { return g_sci_en; }

int acpi_prepare(void)
{
    if (!fadt) return 0;

    /* 确保 ACPI 已启用（SCI_EN = PM1_CNT bit0） */
    if (pm1a_cnt) {
        g_cur = inw(pm1a_cnt);
        g_sci_en = (g_cur & 0x01) ? 1 : 0;
        if (!g_sci_en && smi_cmd) {
            outb(smi_cmd, acpi_enable_val);
            for (u32 i = 0; i < 5000000; i++) {
                if (inw(pm1a_cnt) & 0x01) { g_sci_en = 1; break; }
                cpu_pause();
            }
        }
    }

    /* 读 \_S5_ 得到 SLP_TYP */
    u16 slpa = 5, slpb = 5;
    if (dsdt_addr) {
        u8 *dsdt = (u8 *)(u32)dsdt_addr;
        u32 dsdt_len = *(u32 *)(dsdt + 4);
        if (dsdt_len > 0x100000) dsdt_len = 0x100000;
        if (!parse_s5(dsdt, dsdt_len, &slpa, &slpb))
            slpa = 5;
    }
    g_slp = slpa;
    g_prepared = 1;
    return 1;
}

int acpi_shutdown(void)
{
    serial_puts("ACPI: shutdown entry\n");
    serial_puts("  pm1a="); serial_hex(pm1a_cnt);
    serial_puts(" slp="); serial_dec((int)g_slp);
    serial_puts(" sci="); serial_dec(g_sci_en);
    serial_puts("\n");
    outb(0x501, 0x31);
    serial_puts("ACPI: debug-exit written\n");
    cli();
    if (fadt && pm1a_cnt) {
        u16 cur = inw(pm1a_cnt);
        serial_puts("  pm1_cnt before="); serial_hex(cur);
        serial_puts("\n");
        outw(pm1a_cnt, (u16)((cur & ~(u16)(0x07 << 10)) | (u16)((g_slp << 10) | 0x2000)));
        serial_puts("ACPI: pm1_cnt written\n");
    }
    return -1;
}
