# cunos 开发指南

> 读者对象：要改 cunos 代码、加命令、加驱动、跑验证的人。
> 依据：`Makefile`、`tools/build_bin.sh`、`tools/rebuild_all.sh`、`tools/build.sh`、`src/linker.ld`、`src/grub.cfg`、`src/kernel.c` 的 `help_table[]` / `kernel_exec_line()`、`src/user/` 下的 `crt0.c` / `cu.h` / `mkpath.h`
> 铁律一：命令、别名、参数一律以 `src/kernel.c` 的 `help_table[]` 为准，本文件与之逐字对齐。
> 铁律二：任何改动都要经过「真实可启动验证」——QEMU 无头（`-nographic` + 串口日志）或实机，二选一，不接受「看起来对」。

---

## 1. 环境与工具链

### 1.1 必需工具

| 工具 | 用途 | 备注 |
|---|---|---|
| `i686-elf-gcc` 或 `gcc -m32` | 编译 32 位 freestanding 内核 | 必须带 `-m32`，不链接任何 libc |
| `ld`（binutils） | 按 `linker.ld` 链接 | — |
| `objcopy` | 把 blob 转成 `elf32-i386` 目标文件 | `-I binary -O elf32-i386` |
| `nasm` 或 `as` | `boot.S` 与汇编桩 | `boot.S` 用 GNU as 语法 |
| `grub-mkrescue` / `grub-file` | 生成 ISO、校验 multiboot | 也可用 Makefile 里的 `grub_blob` 路径 |
| Python 3 | `mk_grub_blob.py`、`mk_bin_blob.py` | 生成内嵌载荷 |
| QEMU (`qemu-system-i386`) | 验证 | Windows 侧 `C:\EXES\qemu\` |

### 1.2 WSL（Debian）+ Windows QEMU 的分工

本项目长期采用的组合：**WSL 里编译，Windows 侧跑 QEMU**。

```bash
# WSL 内
cd /mnt/c/cunos
make                     # → cunos.iso
```

```bat
:: Windows 侧
C:\EXES\qemu\qemu-system-i386.exe -cdrom C:\cunos\cunos.iso -m 128
```

为什么这么分工：WSL 的 apt 工具链齐全，Windows 的 QEMU 带图形窗口与串口重定向，验截图方便。如果反过来在 WSL 里跑 QEMU，需要 WSLg 才能看窗口，串口抓取反而不如在 Windows 用 `-serial file:` 直观。

---

## 2. 构建系统

### 2.1 `Makefile` 的三个目标

```make
make            # 编译 + 打 ISO（OUTFILE 可覆盖输出名）
make run        # 用默认 QEMU 参数启动
make clean      # 清理 build/ 与中间产物
```

要点：

1. **编译是 freestanding 的**。核心 flag 组合是 `-m32 -ffreestanding -fno-builtin -fno-stack-protector -nostdlib -fno-pic`（具体以 Makefile 为准），目的：不引入 libc 符号、不生成栈保护调用、不要 PIC 重定位。
2. **内核不链接 libc**，所以 `string.c` 自己实现 `mem*`/`str*`。如果你在代码里写了 `strdup`、`sprintf`、`malloc` 这类符号，链接会失败——这是设计而非缺陷。
3. **三个内嵌 blob 通过 objcopy 变成目标文件参与链接**：`grub_blob.bin`、`bin_blob.bin`。`grub_blob` 用于「把 GRUB 装到硬盘」（`cuninstall`），`bin_blob` 是 `/bin` 程序的归档。
4. **构建自检**：Makefile 会检查两个 blob 的前 4 字节是否为 `CGB1`，且 `grub_blob` 不小于 4096 字节。这个检查的作用是拦住「生成脚本被中途打断，产出空文件但构建仍成功」的坏情况。

### 2.2 `build_bin.sh` —— 用户态程序

编译 `src/user/bin/*.c` 为 ELF32，关键约束：

- 链接地址 `0x08048000`（传统 i386 ET_EXEC 基址），与 `paging.h` 的用户装载点一致。
- 带 **PIE 自检**：脚本会检查产物是不是意外生成了 PIE。cunos 的静态装载路径（`elf.c`）处理 ET_EXEC 最稳，PIE 要走 `elf_dyn.c` 与 `PT_INTERP`，那是另一条更复杂的路径，不能让普通 `/bin` 程序误入。
- 产物输出到 `build/bin/*.elf`，随后由 `mk_bin_blob.py` 打包成 `bin_blob.bin`。

单独编一个程序：

```bash
bash tools/build_bin.sh src/user/bin/ls.c      # 只编 ls
```

### 2.3 `rebuild_all.sh` —— 全量同步

这个脚本解决的是「改了代码但镜像里还是旧文件」这类高频事故。它做两件事：

1. 全量重建内核与所有 `/bin` 程序；
2. 把新产物**同步进磁盘镜像**的 `/boot` 与 `/bin`。

也就是说，`make` 只保证 ISO 是新的；如果你从 `grub_disk.img` 启动，不同步就等于没改。**改完代码先 `rebuild_all.sh`，再启动硬盘**。

### 2.4 `build.sh`

单步构建脚本，用于快速验证某个改动能否过编译，不做镜像同步。日常小改动用它，出 ISO 用 `make`，验硬盘用 `rebuild_all.sh`。

---

## 3. 仓库地图与阅读顺序

```
src/
  boot.S            multiboot 头 + 进保护模式前的准备，跳 kernel_main
  linker.ld         1 MB 加载点，.text/.rodata/.data/.bss 分节
  kernel.c          入口 kernel_main / shell / 命令分发 / help_table
  common.h          类型、端口 I/O、串口分工（COM1=X11, COM2=DBG）、版本宏
  gdt.c idt.c paging.c   保护模式底座
  mm.c proc.c syscall.c  内存/进程/系统调用
  fat.c ext2.c vfs.c path.c elf.c elf_dyn.c   文件系统与装载
  nic.c e1000.c rtl8139.c rtl8169.c ne2k.c net.c  网络
  ata.c ahci.c install.c  存储与安装器
  pci.c usb.c acpi.c kbd.c mouse.c  硬件枚举与输入
  vga.c x11.c drm.c   显示
  editor.c i18n.c ime.c cjkdata.c cjkword.c font8x16.c fontwide.c  文本与本地化
  string.c          自实现 mem/str
  user/
    crt0.c          用户程序入口：取 argc/argv → cu_main → int 0x80 退出
    cu.h            sys1/sys3 系统调用封装 + 写屏辅助
    mkpath.h        norm_path / abspath 路径规范化
    bin/*.c         /bin 外部命令源码
tools/              mk_grub_blob.py mk_bin_blob.py build*.sh rebuild_all.sh
disks/              各类磁盘镜像（见操作指南 2.2）
shots/  build/      截图与构建产物
```

**推荐的阅读顺序**（避免一上来被 585 KB 的 `fontwide.c` 劝退）：

1. `boot.S` → `linker.ld` → `kernel_main()`（`kernel.c` 约 1700 行之后）
2. `gdt.c` → `idt.c` → `paging.c`（先理解「恒等映射 + 开启分页 + MMIO 大页」）
3. `string.c` → `common.h`（看内核的「无 libc 词汇表」）
4. `path.c` → `vfs.c` → `fat.c`（先读 `path.c`，因为所有命令都依赖它）
5. `mm.c` → `proc.c` → `syscall.c`（VMA/COW/缺页 → fork/exit → ABI 表）
6. `elf.c` → `elf_dyn.c`（两条装载路径）
7. `vga.c` → `x11.c`（输出与图形）
8. `nic.c` → 某个具体网卡驱动 → `net.c`
9. `ata.c` → `ahci.c` → `install.c`
10. `i18n.c` → `ime.c`（最后看，纯查表逻辑）

**大文件警告**（不要全文读，读会拖垮工具链且无信息增益）：

| 文件 | 体积 | 处理方式 |
|---|---|---|
| `fontwide.c` | 约 585 KB | 只读头部宏与函数签名，数据用脚本改 |
| `cjkword.c` | 约 144 KB | 同上 |
| `cjkdata.c` | 约 90 KB | 同上 |
| `x11.c` | 较大 | 读前 120 行拿到协议常量，再按函数名跳读 |

---

## 4. 加一条内核内建命令（完整流程）

假设要加 `uptime`，打印开机以来的 tick。

### 4.1 在 `help_table[]` 加一行（`src/kernel.c` 第 587 行起）

`help_entry_t` 五个字段，顺序不能错：

```c
{ "uptime", "uptime", "shell", "show ticks since boot", "tick counter is PIT based" },
```

- `names`：规范名 + `|` 分隔的别名，如 `"ls|dir"`。**别把 `<arg>` / `[arg]` 写进 `names`**，`help_aliases()` 会把它当别名统计，导致计数错乱。
- `usage`：参数形式，如 `"uptime"` 或 `"ifconfig [ip] [mask] [gw]"`。
- `group`：必须是已有组名之一（`shell` / `filesystem` / `bin` / `userland` / `hardware` / `network` / `memory` / `graphics`），否则 `help <group>` 查不到。
- `desc`：一行，控制在 80 列内（`help` 的输出排版按此设计）。
- `note`：前置条件或行为说明，如 `"requires ACPI"`。

### 4.2 写命令函数

放在 `kernel.c` 里靠近同类命令的位置，或新建 `uptime.c` 并在 Makefile 的 `OBJS` 里加一项。

```c
static int cmd_uptime(int argc, char **argv)
{
    (void)argc; (void)argv;
    vga_printf("%s: %u ticks\n", tr(MSG_OK), pit_ticks());
    return 0;
}
```

约定：返回 `0` 表示成功，非 0 是错误码（`cuninstall` 就靠这个打印 `cuninstall: exit <code>`）。

### 4.3 在 `kernel_exec_line()` 的分发链注册

分发链是 `strcmp` 序列（约 1625 行起），加一支：

```c
if (!strcmp(argv[0], "uptime")) return cmd_uptime(argc, argv);
```

**位置有讲究**：`run_external()` 在前，内建在后。如果你希望 `/bin` 版本能覆盖内建，就放在内建链里（`/bin` 优先）；如果希望内建绝对优先，就必须在 `run_external()` **之前**判断——目前所有内建都在其后，所以「同名 ELF 会抢走内建命令」是当前的真实行为，加命令时要知道这一点。

### 4.4 加消息 id（可选）

要支持多语言，就在 `i18n.c` 的消息表里加 `MSG_UPTIME_FMT`，四种语言各一条字符串，然后用 `tr(MSG_UPTIME_FMT)` 取。

### 4.5 验证

```bash
make && bash tools/rebuild_all.sh
```

```bat
qemu-system-i386.exe -cdrom C:\cunos\cunos.iso -m 128 -serial file:C:\cunos\shots\boot.log
```

然后：

```
cunos:fat32 / > help uptime          # 表格里能查到？
cunos:fat32 / > help shell           # 组内能列出？
cunos:fat32 / > uptime
cunos:fat32 / > -d uptime            # 调试输出路径不崩？
```

四条都过才算完成。**只过第 3 条不算**——`help` 一致性是这套设计的一部分。

---

## 5. 加一个 `/bin` 用户态程序

比加内建命令更简单，因为不涉及内核重编（但要走 blob 打包）。

### 5.1 源码骨架（`src/user/bin/hello.c`）

```c
#include "cu.h"
#include "mkpath.h"

int cu_main(int argc, char **argv)
{
    cu_puts("hello from ring3\n");
    if (argc > 1) { cu_puts("arg: "); cu_puts(argv[1]); cu_puts("\n"); }
    return 0;
}
```

### 5.2 启动与退出约定（`crt0.c`）

- 入口不是 `main` 而是 `cu_main`。
- `crt0.c` 从**内核准备好的栈**上取 `argc` / `argv`（内核在装载时把参数压进用户栈），调 `cu_main`，然后通过 `int 0x80` 的 `exit(1)` 退出。
- 因此你的程序**不需要自己解析命令行**，`argv[0]` 是程序名。

### 5.3 系统调用封装（`cu.h`）

`cu.h` 提供 `sys1`（单参数）与 `sys3`（三参数）宏，直接内联 `int 0x80`：

```c
long sys1(long n, long a);
long sys3(long n, long a, long b, long c);
```

典型用法：

```c
sys3(SYS_WRITE, 1, (long)buf, len);       /* 写 stdout */
long fd = sys3(SYS_OPEN, (long)path, 0, 0);
```

**注意 read/write 的第 1 个参数是 fd**（Linux 习惯：0/1/2 是 stdin/stdout/stderr）。`cat` 一次只读 4096 字节，就是因为它这么调 `read`。

### 5.4 路径规范化（`mkpath.h`）

`norm_path()` 与 `abspath()` 处理 `.`/`..`/重复斜杠，并按 cwd 补全。**用户态拿到的路径不会自动带 cwd**，需要你自己的程序调用 `abspath()`（`cat`/`ls`/`copy`/`del`/`enterf` 都这么做）。写新程序时照抄现有写法，否则相对路径会失效。

### 5.5 编译与打包

```bash
bash tools/build_bin.sh src/user/bin/hello.c
python3 tools/mk_bin_blob.py            # 重打包 bin_blob.bin
make && bash tools/rebuild_all.sh
```

验证：

```
cunos:fat32 / > ls
cunos:fat32 / > hello 参数一
```

如果提示命令不存在，先 `ls` 看 `/bin` 里有没有 `hello.elf`——没有就是 blob 没重打包或镜像没同步。

---

## 6. 加一个网卡驱动

以「新 PCI 网卡」为例，`nic.c` 已经定义了抽象，实现者只需满足三个函数：

```c
int  nic_send(const void *buf, int len);
int  nic_recv(void *buf, int cap);
void nic_mac(unsigned char out[6]);
```

### 6.1 步骤

1. **在 `pci.c` 扫描结果里找设备**：vendor/device + BAR。记录 BAR 是 MMIO 还是 IO。
2. **映射 BAR**：MMIO 走 `paging_map_mmio_4m()`（4 MB 大页，PCD/PWT），IO 端口走 `inb/outb`。
3. **复位与初始化**：写控制寄存器、设置 MAC、开接收。
4. **实现 `nic_recv`**：轮询式，无中断。别忘了在返回 0 时不要 memset 缓冲区。
5. **挂到 `net_init()` 的探测顺序**：当前顺序是 e1000 → rtl8169 → rtl8139 → ne2k，新驱动插在合适位置（兼容性越差越靠后）。

### 6.2 必须知道的现状

- **全部是 PIO + 轮询**：shell 主循环每轮调 `net_poll()`，这是收包能推进的唯一动力。所以「怎么让包进来」不需要中断，只需要 `net_poll()` 被调用。
- **`rtl8169.c` 无法在 QEMU 验证**：代码注释明确写了 QEMU 没有 RTL8168 模型。这条路径只能上实机。
- **QEMU 里最省事的验证卡**：`-device e1000` 或 `-device rtl8139`。

### 6.3 验证清单

```
net                     # 驱动名、链路、MAC 是否合理（MAC 不能全 0 或全 FF）
ifconfig 10.0.2.15 255.255.255.0 10.0.2.2
ping 10.0.2.2           # ARP 先通，再 ICMP
tcp 10.0.2.2 80 GET / HTTP/1.0
```

`net` 显示的链路状态必须与 QEMU 的 `-netdev` 配置一致，否则说明读状态寄存器读错了。

---

## 7. 加系统调用

### 7.1 选号

编号必须用 Linux i386 约定（不是随便挑）。已有覆盖见架构指南第 4.2 节。挑一个与语义一致的号，或者挑一个明确「未实现」的号来做桩。

### 7.2 实现位置

`syscall.c` 的分发 switch。三件事必须做：

1. **用户指针校验**：任何来自 ring3 的指针都要走 `user_read_bytes()` / `user_write_bytes()` / `user_zero_bytes()`，未映射返回 -1。禁止直接解引用用户指针——没有信号机制兜底，一次野指针就是内核崩溃。
2. **返回值约定**：成功返回 ≥0，失败返回负 errno 值。
3. **fd 清理**：涉及 fd 的调用要在进程退出时被 `vfs_reset()` 覆盖，否则句柄泄漏（用 `vfs` 命令可以观察 fd 表）。

### 7.3 桩的写法（重要）

有些调用故意只做「让 glibc 别死」：

```c
case 386: /* rseq：明确不支持，让 glibc 退化 */ return -ENOSYS;
case 209: /* getuid 恒为 1，无权限模型 */ return 1;
```

**桩必须留注释说明为什么**，否则下一个人会误以为忘记实现了。

---

## 8. 改文件系统 / 路径层

### 8.1 修改 `path.c` 的红线

`path.c` 是所有命令的唯一路径入口，改动影响面是全系统。红线：

- `CU_PATH_MAX` = 256，`CU_PATH_DISP_MAX` = 48。前者是缓冲上限，后者只影响提示符显示（过长保留尾部）。
- 错误码语义不能改：`PATH_ENOENT(-1)` 不存在 / `PATH_ENOTDIR(-2)` 是文件不是目录 / `PATH_ETOOLONG(-3)` 超长 / `PATH_ENOSYS(-4)` 后端不支持。
- **FAT 平坦根返回 `PATH_ENOSYS` 不是 bug**。`path_probe()` 的注释说明 FAT 后端尚无子目录，所以是「能力不支持」而不是「不存在」。调用方要区分这两种情形。

### 8.2 两条装载路径不能合并

```c
/* kernel.c 的 cmd_run() 逻辑 */
if (fname[0] == '/')  → 需要 vfs_ext2_ready()，走 elf_exec_path()（PT_INTERP / PIE）
else                  → 需要 curfs()（FAT 卷），走 elf_exec()（静态 ET_EXEC）
```

前后各有一次 `vfs_reset()`，作用是「一次 run 不污染 shell 的挂载状态」。改这段代码时必须保留这两次 reset，否则跑完一个程序 shell 的当前卷就变了。

---

## 9. 改图形 / 桌面

### 9.1 `x11.c` 的协议边界

- Core Protocol **子集**，单客户端，**无 backing store**。
- 传输是 **COM1 (0x3F8) 串口字节流**，协议数据大端。
- 绘制直接写 VBE 帧缓冲，`0xRRGGBB`（depth 24 TrueColor）。
- 字节序：`bx16()` / `bx32()` 按客户端声明的 `g_le` 分支——协议要求，不是可选。

### 9.2 加一个窗口操作

`wm_*` 系列是窗口管理逻辑。现有交互：拖标题栏移位（约束在屏内，左边至少露 80 像素）、`-` 最小化、方框最大化/还原、`x` 关闭、`Ctrl+Q` 退出整个会话。

加操作时要考虑：**没有 backing store**，遮挡恢复必须由客户端重绘，所以任何改变窗口可见性的操作都要给客户端发对应事件，否则会出现残影。

### 9.3 复用 shell

桌面内的终端直接复用 `kernel_exec_line()`，输出走当前 vga sink。加新命令不用管桌面——它自动可用。

---

## 10. 验证方法论（本项目硬要求）

### 10.1 QEMU 无头验证（首选，可脚本化）

```bat
qemu-system-i386.exe -cdrom C:\cunos\cunos.iso -m 128 -nographic ^
  -serial file:C:\cunos\shots\boot.log
```

或分双串口（COM1 给 X11，COM2 给调试日志）：

```bat
qemu-system-i386.exe -cdrom C:\cunos\cunos.iso -m 128 ^
  -serial file:C:\cunos\shots\x11.log ^
  -serial file:C:\cunos\shots\dbg.log -display none
```

好处：`boot.log` 里每个 `BOOT: xxx ok` 都能被脚本 `grep`，可以做到「无人值守验证」。这是把「看起来能启动」变成「日志证明启动到第 N 步」的关键手段。

### 10.2 图形验证（截图）

`shots/` 目录保存历次验证截图。图形改动的验证流程：

1. QEMU 带窗口启动；
2. 在 shell 里执行 `desktop`；
3. 用 QEMU 的 monitor 截图（`screendump`）或宿主截图；
4. 存到 `shots/`，文件名带版本与用途（现有命名如 `cunos_窗口拖动.png`、`cunos_桌面_双窗口任务栏.png`）。

### 10.3 实机验证

见《cunos 实体机配置参考》。核心提醒：cunos 用传统 BIOS 引导链（GRUB legacy multiboot），实机必须开 CSM/Legacy，或者把盘做成可被 CSM 引导的形式。UEFI-only 的机器起不来。

### 10.4 验证不过时的排障顺序

1. 最后一个 `BOOT:` 行是哪一步 → 定位模块。
2. 如果卡在 `paging_init` 之后、`gdt_init` 之前 → 大概率是帧缓冲 MMIO 未映射就写像素（#PF）。
3. 如果跑用户程序 #UD → `cpu_enable_sse()` 没生效（CR0.EM/TS、CR4.OSFXSR/OSXMMEXCPT）。
4. 如果 `run` 报 ext2 未挂载 → 用了绝对路径但没 `root`，属正常报错，不是 bug。
5. 如果 `/bin` 命令全找不到 → `bin_blob` 没重打包或镜像没同步（`rebuild_all.sh`）。

---

## 11. 代码规范与约定（从现有代码反推）

1. **命名**：内核模块公共函数带模块前缀（`paging_*`、`mm_*`、`proc_*`、`vfs_*`、`path_*`、`nic_*`、`vga_*`、`usb_*`、`acpi_*`）。命令实现函数是 `cmd_<name>`。
2. **类型**：统一用 `common.h` 的定宽类型（`u8/u16/u32/i32` 一类），不用 `int` 表示硬件寄存器。
3. **端口 I/O**：`inb/outb/inw/outw/inl/outl` 全在 `common.h`；串口分工是 **COM1=X11 协议，COM2=调试日志**，不要混用。
4. **头文件保护**：统一宏名风格，模块注释写在文件头（说明职责与边界，`paging.h` / `mm.h` / `x11.c` / `rtl8169.c` / `ahci.c` 的文件头注释都值得照抄格式）。
5. **禁止引入 Linux 源码**：本项目的实现不得引用 Linux 内核源码，ABI 号与地址布局是「按规范对齐」，不是「抄实现」。
6. **不撒谎原则**：没实现的能力进 `help_planned[]`，不要写进 `help_table[]`；文档与代码冲突时，改文档。
7. **数据文件不改手写**：`fontwide.c` / `cjkdata.c` / `cjkword.c` 是生成产物，改动要改生成脚本。

---

## 12. 常见编译/运行故障

| 故障 | 原因 | 处理 |
|---|---|---|
| `undefined reference to memcpy_type` 一类 | freestanding 缺符号，或误加了 libc | 用 `string.c` 的实现；检查是否漏了 `-ffreestanding -nostdlib` |
| 链接报 `.multiboot` 节缺失 | `KEEP(*(.multiboot))` 被优化掉或 `boot.S` 未参与链接 | 检查 `linker.ld` 与 Makefile 的 OBJS |
| ISO 能启动但 `FS` 为 `none` | 镜像里没有可识别卷 | 用 `blank64.img` + `mkfat32`，或换已含卷的镜像 |
| `/bin` 命令全没有 | `bin_blob` 旧了 | 重跑 `mk_bin_blob.py` + `rebuild_all.sh` |
| 硬盘启动还是旧内核 | 只 `make` 没同步 | `rebuild_all.sh` |
| QEMU 无输出 | `-nographic` 下 VGA 不可见 | 抓 `-serial file:` 看 `BOOT:` 行 |
| 用户程序 #UD | SSE 未配置 | 检查 `cpu_enable_sse()` 在 `kernel_main` 中的位置 |
| 写像素即崩 | 帧缓冲 MMIO 未映射 | 确认 `paging_map_mmio_4m(vga_fb_addr(), vga_fb_bytes())` 已调用 |
| `desktop` 花屏 | 分辨率为文本模式 | 检查 `grub.cfg` 的 `gfxmode` 与 `gfxpayload` |

---

## 13. 提交前自检清单

- [ ] 新命令已加入 `help_table[]`，`help`、`help <cmd>`、`help <group>` 三种查法都正确
- [ ] 未实现的能力没有写进 `help_table[]`，而是在 `help_planned[]`
- [ ] 用户指针全部经 `user_*_bytes()` 校验
- [ ] 改了 `/bin` 程序 → 重打包 `bin_blob` 并 `rebuild_all.sh`
- [ ] QEMU 无头跑过一遍，`boot.log` 里 `BOOT:` 打点全部 ok
- [ ] 图形改动有截图存 `shots/`
- [ ] 文档（`README.md` 与四份指南）与代码一致
- [ ] 没有引入 Linux 源码片段
