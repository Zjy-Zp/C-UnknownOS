# cunos

一个用 C 从零写的 32 位 x86 保护模式操作系统内核，由 GRUB (Multiboot 1) 引导，
自带命令行 shell、FAT16/FAT32 读写、nano 风格编辑器和一个 KDE 风格的字符窗口桌面。

当前版本 **0.4.0**。

---

## 1. 当前能力

| 模块 | 状态 | 说明 |
| --- | --- | --- |
| 引导 | 可用 | GRUB Multiboot 1，内核加载到 1 MB 处 |
| 显示 | 可用 | VESA VBE 帧缓冲 1024x768x32，128x48 字符网格；VGA 文本模式回落 |
| 键盘 | 可用 | PS/2 键盘轮询（i8042），支持换行、退格 |
| 存储 | 可用 | ATA PIO（IDE，LBA28/LBA48）与 SATA AHCI（LBA48 + DMA）双后端 |
| 分区表 | 可用 | MBR（4 主分区）与 GPT（保护性 MBR 0xEE + 128 项分区数组） |
| 文件系统 | 可用 | FAT16 / FAT32 挂载、列目录、读写文件、删除文件 |
| 分区诊断 | 可用 | `fs` 打印分区表条目、起止 LBA、GPT 类型 GUID、每分区实际文件系统签名 |
| 电源 | 可用 | ACPI S5 软关机 |
| 总线 | 可用 | PCI 设备枚举；GPU 厂商识别（Intel / AMD / NVIDIA / Bochs VBE） |
| USB | 部分 | 仅 EHCI 控制器与根端口枚举，**无 Mass Storage 传输层** |
| 图形界面 | 可用 | `desktop` 进入 KDE 风格文件管理器窗口 |
| 编辑器 | 可用 | `edit` 打开全屏编辑器，^O 保存、^X 退出 |

---

## 2. 目录结构

```
C:\cunos
├─ src
│  ├─ boot.S        Multiboot 头 + 汇编入口，建立 16KB 栈后进入 kernel_main
│  ├─ kernel.c      内核主体：启动横幅、shell、命令分发、分区探测
│  ├─ linker.ld     链接脚本，内核基址 1 MB
│  ├─ grub.cfg      GRUB 菜单，timeout=0 直接启动
│  ├─ ata.c/.h      IDE 兼容 PIO 驱动（端口 0x1F0，LBA28/LBA48）
│  ├─ ahci.c/.h     SATA AHCI 驱动（PCI 查找、命令槽 + PRDT DMA）
│  ├─ fat.c/.h      FAT12/16/32 解析、目录遍历、文件读写删除
│  ├─ vga.c/.h      帧缓冲 / 文本模式输出、printf、调色板
│  ├─ kbd.c/.h      PS/2 键盘驱动
│  ├─ pci.c/.h      PCI 配置空间枚举与设备命名
│  ├─ usb.c/.h      EHCI 控制器枚举
│  ├─ acpi.c/.h     ACPI 表解析与 S5 关机
│  ├─ editor.c/.h   nano 风格全屏编辑器
│  ├─ font8x16.c/.h 8x16 点阵字体
│  ├─ string.c      memcpy / memset / memcmp / strcmp 等最小实现
│  └─ common.h      公共类型与内核堆声明
├─ tools            构建与验证用 Python 脚本（磁盘镜像、GPT 生成、QEMU 截图探针）
├─ disks            测试磁盘镜像（MBR 双分区 / GPT 单分区 / 混合签名盘）
├─ shots            QEMU 截图输出（PPM + PNG）
├─ build.sh         构建脚本（WSL / Linux）
├─ Makefile         等价构建入口
└─ cunos_v*.iso     构建产物
```

---

## 3. 构建

在 WSL 的 Debian/Ubuntu 中执行（不要在 `/mnt/c` 里直接用 `ld`，见下方踩坑）：

```bash
cd /mnt/c/cunos
bash build.sh                 # 产物 cunos.iso
OUTFILE=cunos_v5.iso bash build.sh   # 指定产物名，避免旧文件被占用
```

依赖：

```bash
sudo apt-get install -y build-essential nasm grub-pc-bin grub-common xorriso mtools
```

构建要点：

- 编译：`gcc -m32 -ffreestanding -nostdlib -fno-pic -fno-stack-protector`
- 链接：**必须用 `gcc -m32 -T linker.ld -nostdlib`**，Debian 的 `ld` 不接受 `-m32`
- 打包：`grub-mkrescue` 先做到 `/tmp`，再 `cp` 回 `C:\cunos`，直接在 `/mnt/c` 打包会因权限失败

---

## 4. 运行

### 4.1 QEMU：IDE + MBR 盘

```powershell
C:\EXES\qemu\qemu-system-i386.exe -cdrom C:\cunos\cunos_v5.iso -m 256 `
  -drive file=C:\cunos\disks\disk.img,format=raw,if=ide,index=0,media=disk `
  -vga std
```

### 4.2 QEMU：q35 + SATA AHCI + 盘

```powershell
C:\EXES\qemu\qemu-system-i386.exe -cdrom C:\cunos\cunos_v5.iso -m 256 -machine q35 `
  -device ich9-ahci,id=ah `
  -drive file=C:\cunos\disks\disk.img,format=raw,if=none,id=d0 `
  -device ide-hd,drive=d0,bus=ah.0 -vga std
```

仓库内 `start_cunos_ahci.bat` 就是第二套参数的封装，双击即可。

### 4.3 QEMU：不挂盘（用于对照）

```powershell
C:\EXES\qemu\qemu-system-i386.exe -cdrom C:\cunos\cunos_v5.iso -m 128
```

此时 `storage` 必然显示 `no ATA disk`，这是正确行为，不是故障。

### 4.4 实体机

把 ISO 写入 U 盘后，**以 Legacy/CSM 模式启动**（本项目用 `grub-pc-bin`，只有 BIOS 引导，纯 UEFI 模式起不来）。
启动后能通过 `storage` 行确认内核是否认到了本机磁盘。

---

## 5. 命令

| 命令 | 别名 | 说明 |
| --- | --- | --- |
| `help` | `?` | 列出内置命令 |
| `ver` | `version` | 内核版本与特性清单 |
| `echo <text>` | 原样打印文本 |
| `ls` | `dir` | 列出当前 FAT 卷根目录 |
| `enter <file>` | — | 写入文件，单独一行 `.` 结束输入 |
| `del <file>` | `rm` | 删除文件 |
| `edit <file>` | `nano` | 全屏编辑器，`^O` 保存、`^X` 退出 |
| `fs` | — | 显示磁盘后端、分区表、各分区文件系统、已挂载卷 |
| `fs <n>` | — | 切换到第 n 个 FAT 卷 |
| `pci` | — | 列出 PCI 设备（GPU / 存储 / USB） |
| `usb` | — | 列出 USB 主机控制器与根端口 |
| `desktop` | `kde` | 进入 KDE 风格图形桌面 |
| `color` | `colours` | 打印 16 色调色板 |
| `clear` | `cls` | 清屏 |
| `reboot` | — | 重启（ACPI 复位 / 三重故障兜底） |
| `shutdown` | `poweroff` / `halt` | ACPI S5 关机 |

---

## 6. 启动横幅怎么看

```
cunos 0.4.0        i386 protected mode kernel

display : VESA framebuffer 1024x768 32bpp, grid 128x48
input   : PS/2 keyboard ready
storage : SATA AHCI 250069680 sectors
fs      : FAT16* FAT32
power   : ACPI ready (S5 shutdown)
gpu0    : <厂商族名> <vendor:device> <bus:slot.func>
usb     : <主机控制器与根端口摘要>
```

（`display` / `input` / `storage` / `fs` / `power` 为固定字段；`gpuN` / `usb` 行内容随硬件变化。）

- `storage` 是**存储后端**结论：`ATA PIO` = IDE 兼容模式；`SATA AHCI` = 走 AHCI；
  二者都没有时为 `no ATA disk`。
- `fs` 是**可挂载卷**结论：列出所有成功挂载的 FAT 卷，`*` 表示当前卷；
  `none mounted` 表示盘上没有能挂的 FAT 卷（与存储驱动无关）。

---

## 7. 排障

| 现象 | 含义 | 处理 |
| --- | --- | --- |
| `storage : no ATA disk` | 没有任何磁盘被识别 | 确认 QEMU 是否真的挂了 `-drive`；确认盘接在 IDE 或 SATA AHCI 上（NVMe、virtio、USB 目前都不支持） |
| `storage : SATA AHCI ...` 但 `fs : none mounted` | 盘认到了，但盘上没有 FAT 卷 | 敲 `fs` 看分区表与文件系统签名；NTFS / exFAT / ext4 都挂不上，只有 FAT16/FAT32 可以 |
| `table : none (no 0x55AA ...)` | 盘上没有合法分区表 | 裸盘未分区，或用 `fat_mount(lba=0)` 直接挂超级软盘格式 |
| `table : GPT, 128 entries (header NOT found)` | MBR 有 0xEE 但 LBA1 不是 `EFI PART` | 分区表损坏，或盘使用 4Kn 扇区 |
| `[fs] no FAT volume detected` | FAT 挂载全部失败 | 看紧邻的分区表：`fs=` 列会直接告出 NTFS / exFAT / FAT32 / FAT16 |

`fs` 输出示例（GPT 三分区盘）：

```
disk : SATA AHCI, 16384 sectors (~8 MB)
[fs] no FAT volume detected
table : GPT, 128 entries
#      start LBA  sectors  size MB  type            fs
1        2048     2048        1  EFI System      2873  FAT
2        4096     4096        2  MS Basic Data   a2a0  NTFS
3        8192     4096        2  Linux FS        af3d  exFAT
```

存在可挂载卷时会多出一行 `current: [n] FAT16  free clusters x / y`。

---

## 8. 已知限制

- **文件系统只认 FAT16/FAT32**：NTFS、exFAT、ext2/3/4 一律不识别；FAT12 可识别签名但不挂载。
- **无 USB 大容量存储**：`usb.c` 只有 EHCI 枚举，缺少 Mass Storage + SCSI 传输层，U 盘与移动硬盘不可用。
- **无 NVMe、无 virtio-blk**：这两类设备不经过 ATA/AHCI 路径。
- **仅 MBR 主分区与 GPT**：MBR 扩展分区（0x05/0x0F）内的逻辑分区未解析。
- **单任务、无进程调度、无用户态**：shell 与命令在同一特权级顺序执行。
- **`vga_printf` 只支持宽度与 `0` 填充**，不支持 `%-Ns` 左对齐（会原样输出字面量）。
- **磁盘写入无缓存与日志**，掉电可能留下半写状态。

---

## 9. 路线图

1. USB Mass Storage（BOT + SCSI READ(10)/WRITE(10)），打通 U 盘。
2. MBR 扩展分区与逻辑分区解析（EBR 链）。
3. NTFS 只读（$MFT 解析 + data run 读取），用于直接读 Windows 盘。
4. FAT32 格式化命令（在空闲分区上自建卷）。
5. NVMe 驱动；virtio-blk 作为 QEMU 快速测试通道。
6. 内核态任务切换与简单调度。

---

## 10. 版本记录

| 版本 | 内容 |
| --- | --- |
| 0.1 | 最小内核，GRUB 引导，屏幕输出后 `cli; hlt` 挂住 |
| 0.2 | 16 色调色板、IDE PIO 存储、FAT16/FAT32、ACPI S5、PS/2 键盘、PCI/USB 枚举、命令集、nano 编辑器、KDE 风格桌面 |
| 0.3 | 新增 AHCI 驱动与 GPT 解析，修复 SATA/GPT 盘 `no ATA disk` |
| 0.4 | `fs` 增加分区表与文件系统签名诊断，版本号 0.4.0 |

---

## 11. 踩坑记录

- Debian 的 `ld` 拒绝 `-m32`，链接一律走 `gcc -m32 -T linker.ld -nostdlib`。
- `grub-mkrescue` 在 `/mnt/c` 下因权限失败，改为在 `/tmp` 打包再 `cp` 回 Windows 侧。
- QEMU 下写 ACPI `PM1a_CNT`(0x604) 或 `isa-debug-exit`(0x501) 都不会让 QEMU 进程退出；
  QEMU 内验证关机需用 monitor 的 `o /b 0x501 0x31` 触发 SHUTDOWN，返回码 99。
- 旧的 `cunos.iso` 常被 QEMU 进程占用导致覆盖失败，构建时用 `OUTFILE=` 输出新名字规避。
- QEMU 的 `-vga std` 才会给出 VBE 帧缓冲；缺了它 `desktop` 会提示需要 framebuffer。
- 只挂 `-cdrom` 时显示 `no ATA disk` 是**正确**结果，别把它当驱动 bug 排查。
