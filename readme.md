# cunos 系统简介

> cunos 是一个从零手写、能在真实硬件与 QEMU 上直接启动的 32 位 i386 保护模式操作系统内核，
> 定位是教学 / 实验性质的自研内核：所有子系统自己实现，坚持"真可启动、真可交互、真可验证"。

---

## 一、基本信息

| 项目 | 内容 |
|------|------|
| 版本 | 1.1 |
| 架构 | 32 位 i386，保护模式（ring0 / ring3 双特权级） |
| 内核形态 | 单体内核（monolithic），单核运行，无 SMP |
| 引导方式 | GRUB（BIOS / MBR：boot.img + core.img），也提供光盘 ISO 引导 |
| 显示 | VGA 文本模式；VESA VBE 帧缓冲（默认 1024x768x32，文本网格 128x48） |
| 输入 | PS/2 键盘、PS/2 鼠标 |
| 存储 | ATA PIO（主通道）、AHCI |
| 文件系统 | FAT32（完整读写）、ext2（只读挂载，作为第二卷） |
| 网络 | 通用网卡抽象层 + 4 种驱动 + 自研 TCP/IP 栈 |
| 用户态 | ELF32 静态可执行文件，Linux i386 系统调用 ABI（`int $0x80`） |
| 安装 | 内核自带安装器 `cuninstall`，可把引导器、文件系统、内核、用户态程序一次写到本机磁盘 |
| 开发环境 | WSL（Debian）交叉编译、`grub-mkrescue` 打包、QEMU 无头实机验证 |

---

## 二、分层结构

```
+-----------------------------------------------------------+
| 应用层     /bin 下 10 个独立 ELF32 程序（user/bin/*.c）    |
|            ls tree cat echo ver pwd mkdir enterf rm copy   |
+-----------------------------------------------------------+
| 用户态接口 Linux i386 syscall（int $0x80，约 70 个调用号）  |
|            open/read/write/close/getdents64/mkdir/unlink   |
|            chdir/getcwd/mmap/brk/stat/uname/socketcall ... |
+-----------------------------------------------------------+
| 内核服务层 VFS + 路径系统 / 进程上下文 / 图形与输入 / 网络  |
+-----------------------------------------------------------+
| 驱动层     ATA AHCI PS2 VBE 网卡 PCI ACPI USB 探测         |
+-----------------------------------------------------------+
| 硬件抽象   分页内存管理、COW、按需分页、堆分配器            |
+-----------------------------------------------------------+
```

---

## 三、已实现的子系统

| 子系统 | 实现要点 |
|--------|----------|
| 引导与初始化 | GDT / IDT / TSS，用户段 `0x1b` / `0x23`，`int 0x80` 门 DPL=3 |
| 内存管理 | 4KB 分页、物理页分配器、VMA、用户栈与 brk 按需分页、写时复制（COW）、`mmap` |
| 文件系统 | FAT32 读写（创建 / 覆盖 / 删除 / 子目录 / FSInfo 维护）；ext2 只读；VFS 统一 32 个 fd 槽位 |
| 路径系统 | 工作目录 cwd、`cd` / `pwd`、`.` 与 `..`、绝对与相对路径统一规范化 |
| 可执行加载 | ELF32 ET_EXEC 加载器，按段映射用户页，构造 Linux 风格初始栈（argc/argv/envp/auxv） |
| 系统调用 | i386 ABI 兼容层，覆盖文件、目录、内存、时间、uname、socketcall 等约 70 个调用号 |
| 存储 | ATA PIO 扇区读写、AHCI 端口探测、分区与 MBR 写入 |
| 网络 | nic 抽象层 + e1000 / rtl8169 / rtl8139 / ne2k 驱动，ARP / IP / ICMP / UDP / TCP，socketcall |
| 图形 | VGA 文本控制台、VESA 帧缓冲绘图、`/dev/fb0`、DRM 风格 ioctl（`/dev/dri/card0`） |
| 交互 | PS/2 键盘（含 Ctrl / 方向键）、鼠标、全屏文本编辑器 `edit` |
| 电源 | ACPI 关机（S5）、`reboot` |
| 桌面 | 内核内窗口管理 + 内置 desktop，X11 服务端雏形（实验性） |
| 安装器 | `cuninstall`：MBR + core.img + FAT32 + `/grub/grub.cfg` + `/cunos.bin` + `/bin/*.elf` |
| 调试 | `-d` 开关打开内核侧 exec / 段映射 / 系统调用跟踪；控制台镜像到串口便于自动化断言 |

### 尚未实现（不做夸大）

`fork` / `clone` / `execve` 进程创建、管道、futex、完整 POSIX 信号、SMP 多核、可加载内核模块、
udev、cgroup、POSIX 定时器、ext2 写入。

---

## 四、命令一览（36 条，含别名）

### shell 组

| 命令 | 说明 |
|------|------|
| `help` / `?` | 命令列表、单条帮助、分组帮助（`help bin`、`help network`） |
| `-d <cmd>` | 打开内核调试输出执行一条命令 |
| `color` | 打印 16 色 VGA 调色板 |
| `clear` | 清屏 |
| `reboot` | 重启 |
| `shutdown` | ACPI S5 关机 |

### 文件系统组

| 命令 | 说明 |
|------|------|
| `cd [dir]` | 切换工作目录（`cd` 回根，`cd -` 回上一层） |
| `mkfat32 [start] [sectors]` | 把 ATA 盘格式化为 FAT32 卷 |
| `cuninstall` | 把系统安装到本机磁盘（见第六节） |
| `ino <path>` | 查看 ext2 inode 信息 |
| `enter <file>` | 交互式创建文本文件（`.` 结束） |
| `edit <file>` | 全屏编辑器，`^O` 保存、`^X` 退出 |
| `fs [n]` | 查看磁盘与卷，切换当前卷 |
| `vfs` | 查看 VFS 状态 |
| `root` | 把 ext2 卷挂成 VFS 根 |

### /bin 组（独立 ELF32 可执行文件）

| 命令 | 说明 |
|------|------|
| `ls` | 列目录（`-a` 含 `.` `..`） |
| `tree` | 目录树（4 层） |
| `cat` | 打印文件 |
| `echo` | 输出参数 |
| `ver` | 系统版本摘要 |
| `pwd` | 当前工作目录 |
| `mkdir` / `enterf` | 建目录（自动补建父目录） |
| `del` / `rm` | 删除文件 |
| `copy` | `copy <文件> <目录>` 复制文件到目标目录 |

> 这些命令全部走 `int $0x80`，路径按当前工作目录解析；在 `/bin` 下执行 `enterf 123` 就建在 `/bin/123`。

### 用户态 / 硬件 / 网络 / 图形

| 命令 | 说明 |
|------|------|
| `run` / `exec <elf>` | 执行指定 ELF32 程序 |
| `pci` | PCI 设备枚举 |
| `usb` | USB 控制器探测 |
| `net` / `ifconfig` | 网络状态与地址配置 |
| `ping` / `tcp` / `chat` | ICMP、TCP 请求、简易聊天 |
| `mem` | 内存使用摘要 |
| `desktop` / `x11` / `kde` | 图形桌面、X11 服务端、KDE 启动尝试 |

---

## 五、用户态程序与构建

- 每个命令一份源码：`user/bin/<name>.c`，自带 `crt0` 入口，从栈上取 `argc` / `argv`。
- 编译：`gcc -m32 -static -nostdlib -Ttext=0x08048000`，经 `int $0x80` 调用内核。
- 构建脚本 `tools/build_bin.sh` 输出 `build/bin/*.elf`；
  `tools/mk_bin_blob.py` 把它们打包成 `CGB1` 归档并 `objcopy` 内嵌进内核镜像，
  因此安装器可以独立把 `/bin` 写到目标盘，无需外部介质。

---

## 六、安装到本机磁盘

在 shell 里执行 `cuninstall`，需要两次大写确认（`YES` 然后 `GRUB`），然后六步完成：

```
[1/6] MBR written (boot.img 440B + partition table)
[2/6] core.img written at LBA 1..365
[3/6] FAT32 on partition: 127006 clusters x 512 bytes
[4/6] /grub/grub.cfg written
[5/6] /cunos.bin copied            ← 从安装介质取内核
[6/6] 10 programs written to /bin  ← 内嵌的 /bin 归档
install complete: reboot and boot from the disk to check
```

装完直接重启即从硬盘启动，整盘自洽，不依赖安装介质。

---

## 七、构建与验证

```bash
# WSL / Debian 侧
make                     # 编译内核 + 用户态程序 + 打包 bin 归档 + 生成 cunos.iso
bash tools/build_bin.sh  # 单独重建 /bin 下的 ELF 并写进磁盘镜像
boot.bat                 # Windows 侧启动 QEMU（磁盘引导 / 光盘引导两种模式）
```

验证方式坚持"实机可启动"：QEMU 无头 + QMP 注入键盘输入 + 串口抓屏做断言，
每一项功能都以屏幕输出或串口日志为证据，不靠"编译通过"代替运行验证。

---

*文档对应内核版本 1.1，命令清单与子系统列表取自当前源码（`src/`、`user/bin/`）。*
*（介绍内容由AI生成，仅供参考）*
