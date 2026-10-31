#!/usr/bin/env python3
# 生成带 MBR 分区表的测试磁盘：分区1 = FAT16，分区2 = FAT32
import os, struct

SECTOR = 512
F16 = '/tmp/f16.img'
F32 = '/tmp/f32.img'
OUT = '/mnt/c/cunos/disks/disk.img'

start1 = 2048
sectors1 = (os.path.getsize(F16) + SECTOR - 1) // SECTOR
start2 = start1 + sectors1
sectors2 = (os.path.getsize(F32) + SECTOR - 1) // SECTOR
total_sectors = start2 + sectors2

os.makedirs(os.path.dirname(OUT), exist_ok=True)


def chs(lba):
    heads, spt = 255, 63
    c = lba // (heads * spt)
    h = (lba // spt) % heads
    s = (lba % spt) + 1
    if c > 1023:
        c, h, s = 1023, 254, 63
    return bytes([h, ((c >> 2) & 0xC0) | (s & 0x3F), c & 0xFF])


mbr = bytearray(SECTOR)


def add_part(idx, ptype, start, count):
    e = bytearray(16)
    e[0] = 0x00
    e[1:4] = chs(start)
    e[4] = ptype
    e[5:8] = chs(start + count - 1)
    e[8:12] = struct.pack('<I', start)
    e[12:16] = struct.pack('<I', count)
    mbr[446 + idx * 16:446 + (idx + 1) * 16] = e


add_part(0, 0x06, start1, sectors1)   # FAT16
add_part(1, 0x0C, start2, sectors2)   # FAT32 (LBA)
mbr[510] = 0x55
mbr[511] = 0xAA

with open(OUT, 'wb') as f:
    f.truncate(total_sectors * SECTOR)
    f.seek(0)
    f.write(mbr)

    f.seek(start1 * SECTOR)
    with open(F16, 'rb') as s:
        while True:
            chunk = s.read(1 << 20)
            if not chunk:
                break
            f.write(chunk)

    f.seek(start2 * SECTOR)
    with open(F32, 'rb') as s:
        while True:
            chunk = s.read(1 << 20)
            if not chunk:
                break
            f.write(chunk)

print("disk.img 已生成:", OUT)
print("  总大小 %.1f MB" % (total_sectors * SECTOR / 1048576))
print("  分区1 FAT16 start=%d sectors=%d" % (start1, sectors1))
print("  分区2 FAT32 start=%d sectors=%d" % (start2, sectors2))
