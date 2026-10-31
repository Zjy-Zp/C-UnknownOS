#include "common.h"
#include "vga.h"
#include "kbd.h"
#include "ata.h"
#include "fat.h"
#include "acpi.h"
#include "editor.h"
#include "pci.h"
#include "usb.h"

#define CUNOS_VERSION "0.3.0"

/* ---------------- 配色（自定义主题） ---------------- */
#define C_PROMPT  VGA_LIGHT_CYAN
#define C_CMD     VGA_WHITE
#define C_OK      VGA_LIGHT_GREEN
#define C_ERR     VGA_LIGHT_RED
#define C_WARN    VGA_YELLOW
#define C_INFO    VGA_LIGHT_GRAY
#define C_DIM     VGA_DARK_GRAY
#define C_TITLE   VGA_LIGHT_MAGENTA
#define C_VALUE   VGA_LIGHT_CYAN
#define C_KEY     VGA_LIGHT_BLUE

extern u8 kernel_end[];

static u8 *heap_ptr = NULL;
static u32 heap_used_bytes = 0;

void *kmalloc(u32 size)
{
    if (!heap_ptr) {
        u32 base = (u32)(u32)&kernel_end;
        heap_ptr = (u8 *)((base + 15) & ~15u);
    }
    u32 sz = (size + 15) & ~15u;
    void *p = heap_ptr;
    heap_ptr += sz;
    heap_used_bytes += sz;
    return p;
}

u32 kheap_used(void) { return heap_used_bytes; }

/* ---------------- 全局文件系统状态 ---------------- */

static fat_fs g_fs[2];
static int    g_fs_count = 0;
static int    g_cur = 0;

static fat_fs *curfs(void)
{
    if (g_fs_count <= 0) return NULL;
    if (g_cur >= g_fs_count) g_cur = 0;
    return &g_fs[g_cur];
}

static void say_ok(const char *s)   { vga_set_color(C_OK, VGA_BLACK);   vga_puts(s); vga_set_color(C_INFO, VGA_BLACK); }
static void say_err(const char *s)  { vga_set_color(C_ERR, VGA_BLACK);  vga_puts(s); vga_set_color(C_INFO, VGA_BLACK); }
static void say_warn(const char *s) { vga_set_color(C_WARN, VGA_BLACK); vga_puts(s); vga_set_color(C_INFO, VGA_BLACK); }

static void print_fs_brief(void)
{
    if (!g_fs_count) { say_warn("  [fs] no FAT volume detected\n"); return; }
    for (int i = 0; i < g_fs_count; i++) {
        fat_fs *f = &g_fs[i];
        vga_set_color(i == g_cur ? C_OK : C_INFO, VGA_BLACK);
        vga_printf("  [%d]%s FAT%d  part LBA=%u  cluster=%u B  total=%u\n",
                   i, i == g_cur ? "*" : " ", f->type, f->part_lba,
                   (u32)f->spc * 512, f->total_clusters);
    }
    vga_set_color(C_INFO, VGA_BLACK);
}

/* ---------------- 磁盘探测 ---------------- */

static void probe_filesystems(void)
{
    u8 mbr[512];
    g_fs_count = 0;
    g_cur = 0;

    if (!ata_present()) return;
    if (!ata_read_sectors(0, 1, mbr)) return;

    if (mbr[510] == 0x55 && mbr[511] == 0xAA) {
        for (int i = 0; i < 4 && g_fs_count < 2; i++) {
            u8 *p = mbr + 446 + i * 16;
            u32 lba = *(u32 *)(p + 8);
            u32 cnt = *(u32 *)(p + 12);
            if (!cnt || !lba) continue;
            if (fat_mount(&g_fs[g_fs_count], lba)) g_fs_count++;
        }
    }
    if (!g_fs_count) {
        if (fat_mount(&g_fs[0], 0)) g_fs_count = 1;
    }
}

/* ---------------- 行输入 ---------------- */

static void readline(char *buf, int max)
{
    int n = 0;
    for (;;) {
        int k = kbd_getkey();
        if (k == '\n' || k == '\r') { buf[n] = '\0'; vga_putc('\n'); return; }
        if (k == '\b') { if (n > 0) { n--; vga_putc('\b'); } continue; }
        if (k == K_LEFT || k == K_RIGHT || k == K_UP || k == K_DOWN) continue;
        if (k > 0 && k < 0x100) {
            char c = (char)k;
            if (c >= 32 && c < 127 && n < max - 1) { buf[n++] = c; vga_putc(c); }
        }
    }
}

static int split_args(char *line, char **argv, int max)
{
    int n = 0;
    char *p = line;
    while (*p) {
        while (*p == ' ' || *p == '\t') p++;
        if (!*p) break;
        if (n < max) argv[n++] = p;
        while (*p && *p != ' ' && *p != '\t') p++;
        if (*p) *p++ = '\0';
    }
    return n;
}

static void pad_name(char *out, const char *name, int width)
{
    int i = 0;
    while (name[i] && i < width - 1) { out[i] = name[i]; i++; }
    while (i < width) out[i++] = ' ';
    out[width] = '\0';
}

/* ---------------- GPU / USB 信息 ---------------- */

static void print_gpu(void)
{
    int n = pci_gpu_count();
    if (n == 0) { say_warn("  gpu   : no display controller on PCI bus\n"); return; }
    for (int i = 0; i < n; i++) {
        const pci_dev *d = pci_gpu(i);
        vga_set_color(C_VALUE, VGA_BLACK);
        vga_printf("  gpu%d  : %s", i, pci_gpu_family(d->vendor));
        vga_set_color(C_DIM, VGA_BLACK);
        vga_printf("  [%04x:%04x] %02x:%02x.%x\n",
                   d->vendor, d->device, d->bus, d->slot, d->func);
    }
    vga_set_color(C_INFO, VGA_BLACK);
}

static void print_usb(void)
{
    int n = usb_count();
    if (n == 0) { say_warn("  usb   : no USB host controller\n"); return; }
    for (int i = 0; i < n; i++) {
        const usb_ctrl *c = usb_at(i);
        vga_set_color(C_VALUE, VGA_BLACK);
        vga_printf("  usb%d  : %s", i, usb_kind_name(c->kind));
        vga_set_color(C_DIM, VGA_BLACK);
        vga_printf("  [%04x:%04x] %02x:%02x.%x", c->vendor, c->device, c->bus, c->slot, c->func);
        vga_set_color(C_INFO, VGA_BLACK);
        vga_printf("  ports=%d", c->ports);
        if (c->connected) {
            vga_set_color(C_OK, VGA_BLACK);
            vga_printf("  connected=%d", c->connected);
        }
        vga_set_color(C_INFO, VGA_BLACK);
        vga_putc('\n');
    }
}

/* ---------------- 图形桌面（KDE 风格简化版） ---------------- */

static void desktop_frame(void)
{
    int W = disp_cols(), H = disp_rows();
    if (W < 60 || H < 20) W = 80, H = 25;

    vga_clear();

    /* 顶栏 */
    vga_set_color(VGA_BLACK, VGA_CYAN);
    vga_gotoxy(0, 0);
    for (int c = 0; c < W; c++) vga_putc(' ');
    vga_gotoxy(0, 1);
    vga_puts(" cunos desktop   Applications   Places   System ");
    vga_gotoxy(0, W - 26);
    vga_puts(" 2026-10-31   Sat  14:20 ");

    /* 桌面图标区 */
    vga_set_color(VGA_LIGHT_GRAY, VGA_BLACK);
    vga_gotoxy(3, 3); vga_puts("[ Home ]");
    vga_gotoxy(5, 3); vga_puts(" FAT32 ");
    vga_gotoxy(7, 3); vga_puts(" FAT16 ");

    /* 主窗口：文件管理器 */
    int wx = 6, wy = 5, ww = W - 16, wh = H - 14;
    if (ww < 40) ww = 40;
    if (wh < 8) wh = 8;

    vga_set_color(VGA_WHITE, VGA_BLUE);
    vga_gotoxy(wy, wx);
    for (int c = 0; c < ww; c++) vga_putc(' ');
    vga_gotoxy(wy, wx + 2);
    vga_puts(" /  -  File Manager ");

    vga_set_color(VGA_BLACK, VGA_LIGHT_GRAY);
    for (int r = 1; r < wh; r++) {
        vga_gotoxy(wy + r, wx);
        for (int c = 0; c < ww; c++) vga_putc(' ');
    }

    vga_set_color(VGA_BLACK, VGA_LIGHT_GRAY);
    vga_gotoxy(wy + 1, wx + 2);
    vga_puts("Name                 Type      Size");
    vga_set_color(VGA_DARK_GRAY, VGA_LIGHT_GRAY);
    vga_gotoxy(wy + 2, wx + 1);
    for (int c = 0; c < ww - 2; c++) vga_putc('-');

    fat_fs *f = curfs();
    int shown = 0;
    if (f) {
        static fat_dirent ents[FAT_MAX_FILES];
        int n = fat_list(f, ents, FAT_MAX_FILES);
        if (n > 0) {
            for (int i = 0; i < n && wy + 3 + i < wy + wh - 2; i++) {
                char name[24];
                pad_name(name, ents[i].name, 21);
                vga_set_color(VGA_BLACK, VGA_LIGHT_GRAY);
                vga_gotoxy(wy + 3 + i, wx + 2);
                vga_puts(name);
                vga_puts(ents[i].attr & 0x10 ? "DIR       " : "File      ");
                vga_printf("%u", ents[i].size);
                shown++;
            }
        }
    }
    if (!shown) {
        vga_set_color(VGA_DARK_GRAY, VGA_LIGHT_GRAY);
        vga_gotoxy(wy + 3, wx + 2);
        vga_puts("(empty directory)");
    }

    /* 底部任务栏 */
    vga_set_color(VGA_BLACK, VGA_CYAN);
    vga_gotoxy(H - 1, 0);
    for (int c = 0; c < W; c++) vga_putc(' ');
    vga_gotoxy(H - 1, 1);
    vga_puts("[K]");
    vga_gotoxy(H - 1, 5);
    vga_puts("File Manager");
    vga_gotoxy(H - 1, W - 16);
    vga_puts(" cunos 0.3.0 ");

    vga_set_color(VGA_LIGHT_GRAY, VGA_BLACK);
    vga_gotoxy(H - 2, 1);
    vga_puts("Press any key to return to shell");
}

static void cmd_desktop(void)
{
    if (!vga_fb_active()) {
        say_err("  desktop: framebuffer mode required (VESA VBE not active)\n");
        return;
    }
    desktop_frame();
    (void)kbd_getkey();
    vga_clear();
    say_ok("  returned from desktop\n");
}

/* ---------------- 命令 ---------------- */

static void cmd_help(void)
{
    vga_set_color(C_TITLE, VGA_BLACK);
    vga_puts("\n  cunos built-in commands\n");
    vga_set_color(C_DIM, VGA_BLACK);
    vga_puts("  ------------------------------------------------------------\n");

    const char *cmd[] = { "help", "echo <text>", "ver", "ls", "enter <file>",
                          "del <file>", "edit <file>", "fs", "pci", "usb",
                          "desktop", "color", "clear", "reboot", "shutdown" };
    const char *desc[] = {
        "show this help",
        "print text on screen",
        "kernel version and feature list",
        "list files in current directory",
        "write a file (a single '.' on a line ends input)",
        "delete a file",
        "open the nano-like full screen editor (^O save, ^X exit)",
        "show/switch FAT volume, e.g. fs 0",
        "list PCI devices (GPU, storage, USB)",
        "show USB host controllers and root ports",
        "enter the KDE-inspired graphical desktop",
        "draw the 16 colour palette",
        "clear the screen",
        "reboot the machine",
        "ACPI S5 power off"
    };
    for (int i = 0; i < 15; i++) {
        vga_set_color(C_VALUE, VGA_BLACK);
        vga_printf("  %-14s", cmd[i]);
        vga_set_color(C_INFO, VGA_BLACK);
        vga_printf("%s\n", desc[i]);
    }
    vga_putc('\n');
}

static void cmd_echo(char *arg)
{
    vga_set_color(VGA_WHITE, VGA_BLACK);
    if (!arg || !*arg) { vga_putc('\n'); return; }
    vga_puts(arg);
    vga_putc('\n');
    vga_set_color(C_INFO, VGA_BLACK);
}

static void cmd_ver(void)
{
    vga_set_color(C_TITLE, VGA_BLACK);
    vga_printf("cunos %s", CUNOS_VERSION);
    vga_set_color(C_DIM, VGA_BLACK);
    vga_puts("   i386 protected mode kernel\n");
    vga_set_color(C_DIM, VGA_BLACK);
    vga_puts("  ------------------------------------------------------------\n");

    vga_set_color(C_KEY, VGA_BLACK);  vga_puts("  display  ");
    vga_set_color(C_INFO, VGA_BLACK);
    if (vga_fb_active())
        vga_printf("framebuffer %ux%u %u bpp (VESA VBE), text grid %dx%d\n",
                   vga_fb_width(), vga_fb_height(), vga_fb_bpp(), disp_cols(), disp_rows());
    else
        vga_puts("VGA text 80x25, 16 colours\n");

    vga_set_color(C_KEY, VGA_BLACK);  vga_puts("  gpu      ");
    vga_set_color(C_INFO, VGA_BLACK);
    vga_puts("AMD Radeon / NVIDIA GeForce via PCI + linear framebuffer\n");

    vga_set_color(C_KEY, VGA_BLACK);  vga_puts("  input    ");
    vga_set_color(C_INFO, VGA_BLACK);
    vga_puts("PS/2 keyboard (i8042 polling, scan set 1 + E0 prefix)\n");

    vga_set_color(C_KEY, VGA_BLACK);  vga_puts("  storage  ");
    vga_set_color(C_INFO, VGA_BLACK);
    vga_printf("ATA PIO LBA28 (%u sectors) + USB host controllers\n",
               ata_present() ? ata_sector_count() : 0);

    vga_set_color(C_KEY, VGA_BLACK);  vga_puts("  fs       ");
    vga_set_color(C_INFO, VGA_BLACK);
    vga_puts("FAT16 + FAT32 read/write (8.3 names, cluster chain)\n");

    vga_set_color(C_KEY, VGA_BLACK);  vga_puts("  power    ");
    vga_set_color(C_INFO, VGA_BLACK);
    vga_printf("ACPI %s, S5 soft off\n", acpi_available() ? "ready" : "unavailable");

    vga_set_color(C_KEY, VGA_BLACK);  vga_puts("  apps     ");
    vga_set_color(C_INFO, VGA_BLACK);
    vga_puts("nano-style editor (cunos edit), KDE-inspired desktop\n");

    vga_set_color(C_KEY, VGA_BLACK);  vga_puts("  kernel   ");
    vga_set_color(C_INFO, VGA_BLACK);
    vga_printf("heap used %u bytes, build 2026-10-31\n", kheap_used());
    vga_set_color(C_DIM, VGA_BLACK);
    vga_puts("  ------------------------------------------------------------\n");
    vga_set_color(C_INFO, VGA_BLACK);
}

static void cmd_ls(void)
{
    static fat_dirent ents[FAT_MAX_FILES];
    char padded[24];

    fat_fs *f = curfs();
    if (!f) { say_err("  ls: no FAT volume mounted\n"); return; }

    int n = fat_list(f, ents, FAT_MAX_FILES);
    if (n < 0) { say_err("  ls: cannot read directory\n"); return; }

    vga_set_color(C_DIM, VGA_BLACK);
    vga_printf("  FAT%d volume, directory /\n", f->type);

    if (n == 0) { say_warn("  (directory is empty)\n"); return; }

    int total = 0, files = 0, dirs = 0;
    for (int i = 0; i < n; i++) {
        pad_name(padded, ents[i].name, 18);
        if (ents[i].attr & 0x10) {
            vga_set_color(C_VALUE, VGA_BLACK);
            vga_printf("  %s<DIR>\n", padded);
            dirs++;
        } else {
            vga_set_color(VGA_WHITE, VGA_BLACK);
            vga_printf("  %s", padded);
            vga_set_color(C_INFO, VGA_BLACK);
            vga_printf("%u bytes\n", ents[i].size);
            total += (int)ents[i].size;
            files++;
        }
    }
    vga_set_color(C_OK, VGA_BLACK);
    vga_printf("  %d item(s): %d file(s), %d dir(s), %d bytes\n", n, files, dirs, total);
    vga_set_color(C_INFO, VGA_BLACK);
}

static void cmd_enter(char *fname)
{
    static char content[4096];
    char line[128];
    int len = 0;

    fat_fs *f = curfs();
    if (!f) { say_err("  enter: no FAT volume mounted\n"); return; }
    if (!fname || !*fname) { say_err("  usage: enter <filename>\n"); return; }

    vga_set_color(C_WARN, VGA_BLACK);
    vga_printf("  write mode -> %s\n", fname);
    vga_set_color(C_DIM, VGA_BLACK);
    vga_puts("  (type a single '.' on a line to finish)\n");
    vga_set_color(C_INFO, VGA_BLACK);

    for (;;) {
        vga_set_color(C_PROMPT, VGA_BLACK);
        vga_puts("  | ");
        vga_set_color(VGA_WHITE, VGA_BLACK);
        readline(line, sizeof(line));
        if (strcmp(line, ".") == 0) break;

        int n = (int)strlen(line);
        if (len + n + 1 < (int)sizeof(content)) {
            memcpy(content + len, line, n);
            len += n;
            content[len++] = '\n';
        } else {
            say_warn("  ! buffer full, rest is dropped\n");
        }
    }

    vga_set_color(C_INFO, VGA_BLACK);
    if (len == 0) { say_warn("  nothing written\n"); return; }

    if (fat_write_file(f, fname, content, (u32)len)) {
        vga_set_color(C_OK, VGA_BLACK);
        vga_printf("  wrote %s (%d bytes)\n", fname, len);
    } else {
        say_err("  write failed: disk error or directory full\n");
    }
    vga_set_color(C_INFO, VGA_BLACK);
}

static void cmd_del(char *fname)
{
    fat_fs *f = curfs();
    if (!f) { say_err("  del: no FAT volume mounted\n"); return; }
    if (!fname || !*fname) { say_err("  usage: del <filename>\n"); return; }

    if (!fat_file_exists(f, fname)) { say_err("  del: no such file\n"); return; }
    if (fat_delete_file(f, fname)) {
        vga_set_color(C_OK, VGA_BLACK);
        vga_printf("  deleted %s\n", fname);
    } else {
        say_err("  del: delete failed\n");
    }
    vga_set_color(C_INFO, VGA_BLACK);
}

static void cmd_fs(char *arg)
{
    if (arg && *arg) {
        int idx = atoi(arg);
        if (idx >= 0 && idx < g_fs_count) {
            g_cur = idx;
            vga_set_color(C_OK, VGA_BLACK);
            vga_printf("  switched to volume [%d] FAT%d\n", idx, g_fs[idx].type);
            vga_set_color(C_INFO, VGA_BLACK);
            return;
        }
        say_err("  fs: invalid index\n");
        return;
    }

    vga_set_color(C_INFO, VGA_BLACK);
    vga_puts("  disk : ");
    if (ata_present()) {
        vga_set_color(C_VALUE, VGA_BLACK);
        vga_printf("ATA PIO master, %u sectors (~%u MB)\n",
                   ata_sector_count(), ata_sector_count() / 2048);
    } else {
        say_warn("no ATA disk\n");
    }
    print_fs_brief();

    fat_fs *f = curfs();
    if (f) {
        vga_set_color(C_INFO, VGA_BLACK);
        vga_printf("  current: [%d] FAT%d  free clusters %u / %u\n",
                   g_cur, f->type, fat_free_clusters(f), f->total_clusters);
    }
    vga_set_color(C_INFO, VGA_BLACK);
}

static void cmd_pci(void)
{
    vga_set_color(C_TITLE, VGA_BLACK);
    vga_printf("\n  PCI devices: %d\n", pci_count());
    vga_set_color(C_DIM, VGA_BLACK);
    vga_puts("  ------------------------------------------------------------\n");
    for (int i = 0; i < pci_count(); i++) {
        const pci_dev *d = pci_at(i);
        vga_set_color(C_VALUE, VGA_BLACK);
        vga_printf("  %02x:%02x.%x  %04x:%04x ", d->bus, d->slot, d->func, d->vendor, d->device);
        vga_set_color(C_INFO, VGA_BLACK);
        vga_printf("%-18s %s\n", pci_vendor_name(d->vendor), pci_class_name(d->class_code, d->subclass));
    }
    vga_putc('\n');
    vga_set_color(C_INFO, VGA_BLACK);
}

static void cmd_usb(void)
{
    vga_set_color(C_TITLE, VGA_BLACK);
    vga_printf("\n  USB host controllers: %d, attached devices: %d\n",
               usb_count(), usb_total_devices());
    vga_set_color(C_DIM, VGA_BLACK);
    vga_puts("  ------------------------------------------------------------\n");
    print_usb();
    vga_set_color(C_DIM, VGA_BLACK);
    vga_puts("  ------------------------------------------------------------\n");
    vga_set_color(C_INFO, VGA_BLACK);
}

static void cmd_shutdown(void)
{
    vga_set_color(C_WARN, VGA_BLACK);
    vga_puts("\n  ACPI S5 power off ...\n");
    vga_set_color(C_DIM, VGA_BLACK);
    if (acpi_available())
        vga_printf("  ACPI: %s ready\n", acpi_oem_id());
    else
        vga_puts("  ACPI: no FADT found, trying legacy PM1a_CNT\n");

    if (acpi_shutdown() != 0) {
        vga_set_color(C_ERR, VGA_BLACK);
        vga_puts("  shutdown failed, system halted (close the window manually)\n");
        vga_set_color(C_INFO, VGA_BLACK);
    }
    for (;;) hlt();
}

static void cmd_reboot(void)
{
    vga_set_color(C_WARN, VGA_BLACK);
    vga_puts("\n  rebooting ...\n");
    u32 guard = 100000;
    while ((inb(0x64) & 0x02) && --guard) cpu_pause();
    outb(0x64, 0xFE);
    for (;;) hlt();
}

/* ---------------- Banner ---------------- */

static void banner(void)
{
    int W = disp_cols();
    vga_set_color(C_TITLE, VGA_BLACK);
    vga_puts("\n  cunos ");
    vga_set_color(C_OK, VGA_BLACK);
    vga_puts(CUNOS_VERSION);
    vga_set_color(C_DIM, VGA_BLACK);
    vga_puts("   i386 protected mode kernel");

    vga_set_color(C_DIM, VGA_BLACK);
    vga_putc('\n');
    vga_puts("  ");
    for (int i = 0; i < W - 4 && i < 60; i++) vga_putc('-');
    vga_putc('\n');

    vga_set_color(C_KEY, VGA_BLACK);  vga_puts("  display : ");
    vga_set_color(C_VALUE, VGA_BLACK);
    if (vga_fb_active())
        vga_printf("VESA framebuffer %ux%u %ubpp, grid %dx%d\n",
                   vga_fb_width(), vga_fb_height(), vga_fb_bpp(), disp_cols(), disp_rows());
    else
        vga_puts("VGA text 80x25\n");

    vga_set_color(C_KEY, VGA_BLACK);  vga_puts("  input   : ");
    vga_set_color(C_VALUE, VGA_BLACK);
    vga_printf("PS/2 keyboard %s\n", kbd_present() ? "ready" : "not responding");

    vga_set_color(C_KEY, VGA_BLACK);  vga_puts("  storage : ");
    vga_set_color(C_VALUE, VGA_BLACK);
    if (ata_present()) vga_printf("ATA PIO %u sectors\n", ata_sector_count());
    else vga_puts("no ATA disk\n");

    vga_set_color(C_KEY, VGA_BLACK);  vga_puts("  fs      : ");
    vga_set_color(C_VALUE, VGA_BLACK);
    if (g_fs_count) {
        for (int i = 0; i < g_fs_count; i++) vga_printf("FAT%d%s ", g_fs[i].type, i == g_cur ? "*" : "");
        vga_putc('\n');
    } else {
        vga_puts("none mounted\n");
    }

    vga_set_color(C_KEY, VGA_BLACK);  vga_puts("  power   : ");
    vga_set_color(C_VALUE, VGA_BLACK);
    vga_printf("ACPI %s\n", acpi_available() ? "ready (S5 shutdown)" : "not available");

    print_gpu();
    print_usb();

    vga_set_color(C_KEY, VGA_BLACK);  vga_puts("  colours : ");
    vga_set_color(C_DIM, VGA_BLACK);
    vga_puts("\n  ");
    vga_color_bar();

    vga_set_color(C_DIM, VGA_BLACK);
    vga_puts("  ");
    for (int i = 0; i < W - 4 && i < 60; i++) vga_putc('-');
    vga_set_color(C_WARN, VGA_BLACK);
    vga_puts("\n  type help for commands, edit <file> for the editor, desktop for the GUI\n\n");
    vga_set_color(C_INFO, VGA_BLACK);
}

/* ---------------- Shell ---------------- */

static void shell(void)
{
    static char line[256];
    char *argv[8];

    for (;;) {
        fat_fs *f = curfs();
        vga_set_color(C_PROMPT, VGA_BLACK);
        vga_puts("cunos");
        vga_set_color(C_DIM, VGA_BLACK);
        if (f) vga_printf(":fat%d", f->type);
        else   vga_puts(":nodisk");
        vga_set_color(C_PROMPT, VGA_BLACK);
        vga_puts(" / > ");

        vga_set_color(C_CMD, VGA_BLACK);
        readline(line, sizeof(line));
        vga_set_color(C_INFO, VGA_BLACK);

        int argc = split_args(line, argv, 8);
        if (argc == 0) continue;
        const char *cmd = argv[0];

        if (!strcmp(cmd, "help") || !strcmp(cmd, "?"))          cmd_help();
        else if (!strcmp(cmd, "echo"))                          cmd_echo(argc > 1 ? argv[1] : "");
        else if (!strcmp(cmd, "ver") || !strcmp(cmd, "version")) cmd_ver();
        else if (!strcmp(cmd, "ls") || !strcmp(cmd, "dir"))     cmd_ls();
        else if (!strcmp(cmd, "enter"))                         cmd_enter(argc > 1 ? argv[1] : NULL);
        else if (!strcmp(cmd, "del") || !strcmp(cmd, "rm"))     cmd_del(argc > 1 ? argv[1] : NULL);
        else if (!strcmp(cmd, "edit") || !strcmp(cmd, "nano")) {
            if (argc < 2) { say_err("  usage: edit <filename>\n"); continue; }
            editor_open(curfs(), argv[1]);
            vga_set_color(C_INFO, VGA_BLACK);
            vga_printf("  exited editor\n");
        }
        else if (!strcmp(cmd, "fs"))                            cmd_fs(argc > 1 ? argv[1] : NULL);
        else if (!strcmp(cmd, "pci"))                           cmd_pci();
        else if (!strcmp(cmd, "usb"))                           cmd_usb();
        else if (!strcmp(cmd, "desktop") || !strcmp(cmd, "kde")) cmd_desktop();
        else if (!strcmp(cmd, "color") || !strcmp(cmd, "colours")) { vga_putc('\n'); vga_color_bar(); vga_putc('\n'); }
        else if (!strcmp(cmd, "clear") || !strcmp(cmd, "cls"))  vga_clear();
        else if (!strcmp(cmd, "shutdown") || !strcmp(cmd, "poweroff") || !strcmp(cmd, "halt")) cmd_shutdown();
        else if (!strcmp(cmd, "reboot"))                        cmd_reboot();
        else {
            vga_set_color(C_ERR, VGA_BLACK);
            vga_printf("  %s: command not found (try help)\n", cmd);
            vga_set_color(C_INFO, VGA_BLACK);
        }
    }
}

/* ---------------- 入口 ---------------- */

void kernel_main(u32 magic, u32 mb_info)
{
    (void)magic;

    vga_init(mb_info);
    vga_clear();
    vga_set_color(C_INFO, VGA_BLACK);

    kbd_init();
    pci_scan();
    usb_init();
    ata_init();
    probe_filesystems();
    acpi_init();

    banner();
    shell();

    for (;;) hlt();
}
