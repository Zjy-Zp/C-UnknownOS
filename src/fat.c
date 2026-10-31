#include "fat.h"
#include "ata.h"

/* ============================================================
 * FAT16 / FAT32 读写实现（根目录级操作）
 *  - 挂载：解析 BPB 判断 16/32
 *  - 功能：列目录、读文件、写/覆盖文件、删除文件
 * ============================================================ */

#define SEC_SIZE      512
#define FAT_EOC16     0xFFF8
#define FAT_EOC32     0x0FFFFFF8
#define FAT_BAD16     0xFFF7
#define FAT_BAD32     0x0FFFFFF7

static u8 fbuf[SEC_SIZE];      /* FAT 表扇区缓冲 */
static u8 dbuf[SEC_SIZE];      /* 目录扇区缓冲 */
static u8 rbuf[SEC_SIZE];      /* 通用扇区缓冲 */

typedef int (*root_cb_t)(fat_fs *fs, u32 sec, u32 idx, u8 *entry, void *ctx);

static int read_sec(fat_fs *fs, u32 rel, void *buf)
{
    return ata_read_sectors(fs->part_lba + rel, 1, buf);
}

static int write_sec(fat_fs *fs, u32 rel, const void *buf)
{
    return ata_write_sectors(fs->part_lba + rel, 1, buf);
}

static u32 cluster_to_sector(fat_fs *fs, u32 cluster)
{
    return fs->data_start + (cluster - 2) * fs->spc;
}

static u32 cluster_bytes(fat_fs *fs)
{
    return (u32)fs->spc * SEC_SIZE;
}

/* 读 FAT 表中的下一簇；0 表示空闲，EOC 表示链尾，0xFFFFFFFF 表示失败 */
static u32 fat_next(fat_fs *fs, u32 cluster)
{
    u32 off, sec, v;

    if (fs->type == 32) {
        off = cluster * 4;
        if (off / SEC_SIZE >= fs->fat_sectors) return 0xFFFFFFFF;
        sec = fs->fat_start + off / SEC_SIZE;
        if (!read_sec(fs, sec, fbuf)) return 0xFFFFFFFF;
        v = *(u32 *)(fbuf + (off % SEC_SIZE));
        v &= 0x0FFFFFFF;
        if (v >= FAT_EOC32) return 0x0FFFFFFF;
        if (v == FAT_BAD32) return 0x0FFFFFFF;
        return v;
    }

    off = cluster * 2;
    if (off / SEC_SIZE >= fs->fat_sectors) return 0xFFFFFFFF;
    sec = fs->fat_start + off / SEC_SIZE;
    if (!read_sec(fs, sec, fbuf)) return 0xFFFFFFFF;
    v = *(u16 *)(fbuf + (off % SEC_SIZE));
    v &= 0xFFFF;
    if (v >= FAT_EOC16) return 0x0FFFFFFF;
    if (v == FAT_BAD16) return 0x0FFFFFFF;
    return v;
}

/* 写 FAT 表项（同步所有副本） */
static int fat_set(fat_fs *fs, u32 cluster, u32 value)
{
    for (int copy = 0; copy < fs->fats; copy++) {
        u32 fat_base = fs->fat_start + (u32)copy * fs->fat_sectors;
        u32 off, sec;

        if (fs->type == 32) {
            off = cluster * 4;
            sec = fat_base + off / SEC_SIZE;
            if (!read_sec(fs, sec, fbuf)) return 0;
            *(u32 *)(fbuf + (off % SEC_SIZE)) = value & 0x0FFFFFFF;
        } else {
            off = cluster * 2;
            sec = fat_base + off / SEC_SIZE;
            if (!read_sec(fs, sec, fbuf)) return 0;
            *(u16 *)(fbuf + (off % SEC_SIZE)) = (u16)(value & 0xFFFF);
        }
        if (!write_sec(fs, sec, fbuf)) return 0;
    }
    return 1;
}

/* 分配一个空闲簇，失败返回 0 */
static u32 alloc_cluster(fat_fs *fs, u32 hint)
{
    for (u32 c = hint ? hint : 2; c < fs->total_clusters + 2; c++) {
        if (fat_next(fs, c) == 0) return c;
    }
    for (u32 c = 2; c < hint && c < fs->total_clusters + 2; c++) {
        if (fat_next(fs, c) == 0) return c;
    }
    return 0;
}

/* 释放整条簇链 */
static void free_chain(fat_fs *fs, u32 cluster)
{
    u32 guard = 0;
    while (cluster >= 2 && cluster < 0x0FFFFFF0 && guard++ < 65536) {
        u32 next = fat_next(fs, cluster);
        fat_set(fs, cluster, 0);
        if (next >= 0x0FFFFFF0 || next == 0 || next == 0xFFFFFFFF) break;
        cluster = next;
    }
}

/* 由 8.3 原始名生成可读名（小写、点分隔） */
static void name_from_raw(const u8 *raw, char out[FAT_NAME_LEN])
{
    int n = 0, last = 7;
    while (last >= 0 && raw[last] == ' ') last--;
    for (int i = 0; i <= last && n < 8; i++) out[n++] = (char)(raw[i] | 0x20);
    {
        int e_last = 10;
        while (e_last >= 8 && raw[e_last] == ' ') e_last--;
        if (e_last >= 8) {
            out[n++] = '.';
            for (int i = 8; i <= e_last && n < FAT_NAME_LEN - 1; i++)
                out[n++] = (char)(raw[i] | 0x20);
        }
    }
    out[n] = '\0';
}

/* 生成 11 字节短名 */
static void build_short(const char *in, u8 out[11])
{
    for (int i = 0; i < 11; i++) out[i] = ' ';
    int o = 0, i = 0;
    while (in[i] && in[i] != '.' && o < 8) {
        out[o++] = (u8)to_upper(in[i]);
        i++;
    }
    while (in[i] && in[i] != '.') i++;
    if (in[i] == '.') {
        i++;
        int o2 = 8;
        while (in[i] && o2 < 11) {
            out[o2++] = (u8)to_upper(in[i]);
            i++;
        }
    }
}

void fat_format_name(const char *input, char out[FAT_NAME_LEN])
{
    u8 raw[11];
    build_short(input, raw);
    name_from_raw(raw, out);
}

/* 遍历根目录 */
static void walk_root(fat_fs *fs, root_cb_t cb, void *ctx)
{
    if (fs->type == 16) {
        u32 nsec = (u32)fs->root_entries * 32 / SEC_SIZE;
        for (u32 s = 0; s < nsec; s++) {
            if (!read_sec(fs, fs->root_dir_start + s, dbuf)) return;
            for (u32 i = 0; i < SEC_SIZE / 32; i++)
                if (!cb(fs, fs->root_dir_start + s, i, dbuf + i * 32, ctx))
                    return;
        }
        return;
    }

    u32 cl = fs->root_cluster;
    u32 guard = 0;
    while (cl >= 2 && cl < 0x0FFFFFF0 && guard++ < 65536) {
        u32 base = cluster_to_sector(fs, cl);
        for (u32 s = 0; s < fs->spc; s++) {
            if (!read_sec(fs, base + s, dbuf)) return;
            for (u32 i = 0; i < SEC_SIZE / 32; i++)
                if (!cb(fs, base + s, i, dbuf + i * 32, ctx))
                    return;
        }
        cl = fat_next(fs, cl);
    }
}

static u32 ent_cluster(fat_fs *fs, const u8 *e)
{
    u32 lo = (u32)e[26] | ((u32)e[27] << 8);
    u32 hi = (u32)e[20] | ((u32)e[21] << 8);
    return fs->type == 32 ? (lo | (hi << 16)) : lo;
}

static int entry_usable(const u8 *e, u32 *end)
{
    if (e[0] == 0x00) { *end = 1; return 0; }   /* 目录结束 */
    if (e[0] == 0xE5) return 0;                 /* 已删除 */
    u8 attr = e[11];
    if (attr == 0x0F) return 0;                 /* 长文件名项 */
    if (attr & 0x08) return 0;                  /* 卷标 */
    return 1;
}

/* ---------------- 列目录 ---------------- */

typedef struct {
    fat_dirent *out;
    int max;
    int count;
    int want_all;
} list_ctx;

static int cb_list(fat_fs *fs, u32 sec, u32 idx, u8 *entry, void *ctx)
{
    (void)sec; (void)idx;
    list_ctx *lc = (list_ctx *)ctx;
    u32 end = 0;

    if (!entry_usable(entry, &end)) {
        if (end) return 0;
        return 1;
    }
    if (lc->count < lc->max) {
        fat_dirent *d = &lc->out[lc->count];
        name_from_raw(entry, d->name);
        d->attr = entry[11];
        d->size = *(u32 *)(entry + 28);
        d->cluster = ent_cluster(fs, entry);
        lc->count++;
    }
    return 1;
}

int fat_list(fat_fs *fs, fat_dirent *out, int max)
{
    list_ctx lc = { out, max, 0, 0 };
    if (!fs->present) return -1;
    walk_root(fs, cb_list, &lc);
    return lc.count;
}

u32 fat_count_files(fat_fs *fs)
{
    static fat_dirent tmp[FAT_MAX_FILES];
    int n = fat_list(fs, tmp, FAT_MAX_FILES);
    return n < 0 ? 0 : (u32)n;
}

/* ---------------- 查找 / 是否存在 ---------------- */

typedef struct {
    u32  target;
    u8   raw[11];
    int  found;
    u32  sec;
    u32  idx;
    u32  cluster;
    u32  size;
} find_ctx;

static int cb_find(fat_fs *fs, u32 sec, u32 idx, u8 *entry, void *ctx)
{
    find_ctx *fc = (find_ctx *)ctx;
    u32 end = 0;

    if (entry[0] == 0x00) { end = 1; }
    if (!entry_usable(entry, &end)) {
        if (end) return 0;
        return 1;
    }
    if (memcmp(entry, fc->raw, 11) == 0) {
        fc->found = 1;
        fc->sec = sec;
        fc->idx = idx;
        fc->cluster = ent_cluster(fs, entry);
        fc->size = *(u32 *)(entry + 28);
        return 0;
    }
    return 1;
}

static int find_entry(fat_fs *fs, const char *name, find_ctx *out)
{
    memset(out, 0, sizeof(*out));
    build_short(name, out->raw);
    out->target = (u32)name;
    walk_root(fs, cb_find, out);
    return out->found;
}

int fat_file_exists(fat_fs *fs, const char *name)
{
    find_ctx fc;
    return fs->present && find_entry(fs, name, &fc);
}

/* ---------------- 读文件 ---------------- */

int fat_read_file(fat_fs *fs, const char *name, void *buf, u32 max, u32 *out_len)
{
    find_ctx fc;
    if (!fs->present) return 0;
    if (!find_entry(fs, name, &fc)) return 0;

    u32 want = fc.size < max ? fc.size : max;
    u8 *dst = (u8 *)buf;
    u32 done = 0, cl = fc.cluster, guard = 0;

    while (cl >= 2 && cl < 0x0FFFFFF0 && done < want && guard++ < 65536) {
        u32 base = cluster_to_sector(fs, cl);
        for (u32 s = 0; s < fs->spc && done < want; s++) {
            u32 n = want - done;
            if (n > SEC_SIZE) n = SEC_SIZE;
            if (!read_sec(fs, base + s, rbuf)) return 0;
            memcpy(dst + done, rbuf, n);
            done += n;
        }
        cl = fat_next(fs, cl);
    }

    if (out_len) *out_len = done;
    return 1;
}

/* ---------------- 写文件 ---------------- */

/* 找一个可用的空目录项（在写文件时用） */
typedef struct {
    u8   raw[11];
    int  found;      /* 已存在 */
    u32  sec, idx;
    u32  cluster;
    u32  size;
    int  hole;       /* 找到的空项 */
    u32  hsec, hidx;
} write_ctx;

static int cb_write_scan(fat_fs *fs, u32 sec, u32 idx, u8 *entry, void *ctx)
{
    (void)fs;
    write_ctx *wc = (write_ctx *)ctx;

    if (entry[0] == 0x00) {
        if (!wc->hole) { wc->hole = 1; wc->hsec = sec; wc->hidx = idx; }
        return 0;
    }
    if (entry[0] == 0xE5) {
        if (!wc->hole) { wc->hole = 1; wc->hsec = sec; wc->hidx = idx; }
        return 1;
    }
    u8 attr = entry[11];
    if (attr == 0x0F || (attr & 0x08)) return 1;

    if (!wc->found && memcmp(entry, wc->raw, 11) == 0) {
        wc->found = 1;
        wc->sec = sec;
        wc->idx = idx;
        wc->cluster = ent_cluster(fs, entry);
        wc->size = *(u32 *)(entry + 28);
    }
    return 1;
}

int fat_write_file(fat_fs *fs, const char *name, const void *data, u32 len)
{
    write_ctx wc;
    u32 cb_bytes;
    u32 need, cl, first = 0, prev = 0, written = 0;
    const u8 *src = (const u8 *)data;

    if (!fs->present) return 0;
    memset(&wc, 0, sizeof(wc));
    build_short(name, wc.raw);

    walk_root(fs, cb_write_scan, &wc);

    /* 旧文件先清空 */
    if (wc.found && wc.cluster >= 2) free_chain(fs, wc.cluster);

    cb_bytes = cluster_bytes(fs);
    need = len ? (len + cb_bytes - 1) / cb_bytes : 1;

    u32 hint = 2;
    for (u32 i = 0; i < need; i++) {
        cl = alloc_cluster(fs, hint);
        if (!cl) return 0;
        hint = cl + 1;
        fat_set(fs, cl, 0x0FFFFFFF);
        if (!first) first = cl;
        if (prev) fat_set(fs, prev, cl);
        prev = cl;

        /* 写这一簇的数据 */
        u32 base = cluster_to_sector(fs, cl);
        for (u32 s = 0; s < fs->spc; s++) {
            memset(rbuf, 0, SEC_SIZE);
            if (written < len) {
                u32 n = len - written;
                if (n > SEC_SIZE) n = SEC_SIZE;
                memcpy(rbuf, src + written, n);
                written += n;
            }
            if (!write_sec(fs, base + s, rbuf)) return 0;
        }
    }

    /* 更新 / 创建目录项 */
    u32 sec = wc.found ? wc.sec : wc.hsec;
    u32 idx = wc.found ? wc.idx : wc.hidx;
    if (!read_sec(fs, sec, dbuf)) return 0;
    u8 *e = dbuf + idx * 32;

    if (!wc.found) {
        memset(e, 0, 32);
        memcpy(e, wc.raw, 11);
        e[11] = 0x20;                   /* ARCHIVE */
        e[12] = 0;
        /* 固定时间戳：2026-10-31 12:00:00 */
        *(u16 *)(e + 14) = (u16)((12 << 11) | 0);
        *(u16 *)(e + 16) = (u16)((46 << 9) | (10 << 5) | 31);
        *(u16 *)(e + 18) = *(u16 *)(e + 16);
        *(u16 *)(e + 22) = *(u16 *)(e + 14);
        *(u16 *)(e + 24) = *(u16 *)(e + 16);
    }

    *(u16 *)(e + 26) = (u16)(first & 0xFFFF);
    *(u16 *)(e + 20) = (u16)((first >> 16) & 0xFFFF);
    *(u32 *)(e + 28) = len;

    return write_sec(fs, sec, dbuf);
}

/* ---------------- 删除文件 ---------------- */

typedef struct {
    u8  raw[11];
    int found;
    u32 sec, idx, cluster;
} del_ctx;

static int cb_del_scan(fat_fs *fs, u32 sec, u32 idx, u8 *entry, void *ctx)
{
    del_ctx *dc = (del_ctx *)ctx;
    u32 end = 0;

    if (!entry_usable(entry, &end)) {
        if (end) return 0;
        return 1;
    }
    if (memcmp(entry, dc->raw, 11) == 0) {
        dc->found = 1;
        dc->sec = sec;
        dc->idx = idx;
        dc->cluster = ent_cluster(fs, entry);
        return 0;
    }
    return 1;
}

int fat_delete_file(fat_fs *fs, const char *name)
{
    del_ctx dc;
    if (!fs->present) return 0;
    memset(&dc, 0, sizeof(dc));
    build_short(name, dc.raw);

    walk_root(fs, cb_del_scan, &dc);
    if (!dc.found) return 0;

    if (dc.cluster >= 2) free_chain(fs, dc.cluster);

    if (!read_sec(fs, dc.sec, dbuf)) return 0;
    dbuf[dc.idx * 32] = 0xE5;
    return write_sec(fs, dc.sec, dbuf);
}

/* ---------------- 挂载 ---------------- */

int fat_mount(fat_fs *fs, u32 part_lba)
{
    u8 b[SEC_SIZE];
    memset(fs, 0, sizeof(*fs));
    fs->part_lba = part_lba;

    if (!ata_read_sectors(part_lba, 1, b)) return 0;
    if (b[510] != 0x55 || b[511] != 0xAA) return 0;

    u16 bps = *(u16 *)(b + 11);
    u8  spc = b[13];
    u16 reserved = *(u16 *)(b + 14);
    u8  fats = b[16];
    u16 root_entries = *(u16 *)(b + 17);
    u16 total16 = *(u16 *)(b + 19);
    u16 fatsz16 = *(u16 *)(b + 22);
    u32 total32 = *(u32 *)(b + 32);
    u32 fatsz32 = *(u32 *)(b + 36);
    u32 root_cluster = *(u32 *)(b + 44);

    if (bps != SEC_SIZE || spc == 0 || fats == 0) return 0;

    u32 total = total16 ? total16 : total32;
    if (!total) return 0;

    fs->bps = bps;
    fs->spc = spc;
    fs->reserved = reserved;
    fs->fats = fats;
    fs->root_entries = root_entries;

    for (int i = 0; i < 11; i++) fs->label[i] = (char)b[71 + i];
    fs->label[11] = '\0';

    if (fatsz16 && root_entries) {
        fs->fat_sectors = fatsz16;
        fs->root_dir_sectors = (u32)(root_entries * 32 + SEC_SIZE - 1) / SEC_SIZE;
        fs->fat_start = reserved;
        fs->root_dir_start = reserved + (u32)fats * fatsz16;
        fs->data_start = fs->root_dir_start + fs->root_dir_sectors;
        fs->total_clusters = (total - fs->data_start) / spc;
        fs->type = (fs->total_clusters < 4085) ? 12 : 16;
    } else if (fatsz32) {
        fs->fat_sectors = fatsz32;
        fs->root_dir_sectors = 0;
        fs->fat_start = reserved;
        fs->root_dir_start = 0;
        fs->data_start = reserved + (u32)fats * fatsz32;
        fs->root_cluster = root_cluster;
        fs->total_clusters = (total - fs->data_start) / spc;
        fs->type = 32;
    } else {
        return 0;
    }

    if (fs->type != 16 && fs->type != 32) return 0;   /* FAT12 不支持 */

    fs->present = 1;
    return 1;
}

u32 fat_free_clusters(fat_fs *fs)
{
    u32 free_cnt = 0;
    if (!fs->present) return 0;
    for (u32 c = 2; c < fs->total_clusters + 2; c++) {
        if (fat_next(fs, c) == 0) free_cnt++;
    }
    return free_cnt;
}
