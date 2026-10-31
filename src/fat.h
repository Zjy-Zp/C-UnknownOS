#ifndef FAT_H
#define FAT_H

#include "common.h"

#define FAT_MAX_FILES   64
#define FAT_NAME_LEN    13          /* 8.3 + 点 + \0 */

typedef struct {
    int  present;
    int  type;              /* 16 / 32 */
    u32  part_lba;          /* 分区起始 LBA（绝对） */
    u16  bps;               /* 每扇区字节数，固定 512 */
    u8   spc;               /* 每簇扇区数 */
    u16  reserved;          /* 保留扇区（相对分区） */
    u8   fats;              /* FAT 副本数 */
    u16  root_entries;
    u32  fat_sectors;
    u32  root_cluster;
    u32  root_dir_sectors;
    u32  fat_start;         /* 相对分区扇区 */
    u32  root_dir_start;    /* 相对分区扇区（FAT16） */
    u32  data_start;        /* 相对分区扇区 */
    u32  total_clusters;
    char label[12];
} fat_fs;

typedef struct {
    char name[FAT_NAME_LEN];
    u32  size;
    u8   attr;
    u32  cluster;
} fat_dirent;

int  fat_mount(fat_fs *fs, u32 part_lba);
u32  fat_count_files(fat_fs *fs);
int  fat_list(fat_fs *fs, fat_dirent *out, int max);
int  fat_read_file(fat_fs *fs, const char *name, void *buf, u32 max, u32 *out_len);
int  fat_write_file(fat_fs *fs, const char *name, const void *data, u32 len);
int  fat_delete_file(fat_fs *fs, const char *name);
int  fat_file_exists(fat_fs *fs, const char *name);
u32  fat_free_clusters(fat_fs *fs);
void fat_format_name(const char *input, char out[FAT_NAME_LEN]);

#endif
