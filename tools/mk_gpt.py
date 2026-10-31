
import os, struct, zlib

SRC = r"C:\cunos\disks\disk.img"
DST = r"C:\cunos\disks\disk_gpt.img"
SEC = 512
FAT_LBA, FAT_SECS = 2048, 131072
TOTAL = FAT_LBA + FAT_SECS + 2048

def crc32(b): return zlib.crc32(b) & 0xFFFFFFFF

# 读源 FAT16 分区内容
with open(SRC, "rb") as f:
    f.seek(FAT_LBA * SEC)
    fatdata = f.read(FAT_SECS * SEC)
assert len(fatdata) == FAT_SECS * SEC

img = bytearray(TOTAL * SEC)

# --- 保护性 MBR ---
p = 446
img[p] = 0x00
img[p+4] = 0xEE                     # GPT 保护分区
struct.pack_into("<I", img, p+8, 1)
struct.pack_into("<I", img, p+12, min(TOTAL - 1, 0xFFFFFFFF))
img[510] = 0x55; img[511] = 0xAA

TYPE_BASIC = bytes.fromhex("A2A0D0EBE5B9334487C068B6B72699C7")

def mkgpt_entry(t):
    e = bytearray(128)
    e[0:16] = t
    e[16:32] = bytes([(i * 37 + 11) & 0xFF for i in range(16)])
    struct.pack_into("<Q", e, 32, FAT_LBA)
    struct.pack_into("<Q", e, 40, FAT_LBA + FAT_SECS - 1)
    name = "CUNOSFAT16".encode("utf-16-le")
    e[56:56+len(name)] = name
    return bytes(e)

entries = bytearray(128 * 128)
entries[0:128] = mkgpt_entry(TYPE_BASIC)
ecrc = crc32(bytes(entries))

def mkheader(cur, backup, ent_lba):
    h = bytearray(92)
    h[0:8] = b"EFI PART"
    struct.pack_into("<I", h, 8, 0x00010000)
    struct.pack_into("<I", h, 12, 92)
    struct.pack_into("<Q", h, 24, cur)
    struct.pack_into("<Q", h, 32, backup)
    struct.pack_into("<Q", h, 40, 34)
    struct.pack_into("<Q", h, 48, TOTAL - 34)
    h[56:72] = bytes([0x11,0x22,0x33,0x44,0x55,0x66,0x77,0x88,0x99,0xAA,0xBB,0xCC,0xDD,0xEE,0xF0,0x01])
    struct.pack_into("<Q", h, 72, ent_lba)
    struct.pack_into("<I", h, 80, 128)
    struct.pack_into("<I", h, 84, 128)
    struct.pack_into("<I", h, 88, ecrc)
    struct.pack_into("<I", h, 16, 0)
    struct.pack_into("<I", h, 16, crc32(bytes(h)))
    return bytes(h)

img[1*SEC:1*SEC+92] = mkheader(1, TOTAL-1, 2)
img[2*SEC:2*SEC+len(entries)] = entries
img[(TOTAL-33)*SEC:(TOTAL-33)*SEC+len(entries)] = entries
img[(TOTAL-1)*SEC:(TOTAL-1)*SEC+92] = mkheader(TOTAL-1, 1, TOTAL-33)
img[FAT_LBA*SEC:(FAT_LBA+FAT_SECS)*SEC] = fatdata

with open(DST, "wb") as f:
    f.write(img)
print("gpt image:", DST, TOTAL*SEC//1024//1024, "MB")
