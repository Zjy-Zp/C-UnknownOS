# cunos 架构指南

本文描述 cunos 内核的整体结构：从 GRUB 交接控制权开始，到 `kernel_main()` 完成初始化、进入 shell 的全过程，以及各子系统（特权级底座、分页与内存管理、进程与 ELF 装载、系统调用分发、VFS、块层、网络栈、图形栈、输入栈、ACPI/PCI/USB）的职责、数据结构和相互依赖。

本文内容全部来自 `C:\cunos` 仓库源码核查，凡源码中不存在的机制，明确标注「未实现」，不做补齐式描述。主要依据：`src/boot.S`、`src/kernel.c`、`src/gdt.c`、`src/idt.c`、`src/paging.c`、`src/mm.c`、`src/proc.c`、`src/elf.c`、`src/elf_dyn.c`、`src/syscall.c`、`src/vfs.c`、`src/fat.c`、`src/ext2.c`、`src/ata.c`、`src/ahci.c`、`src/nic.c`、`src/net.c`、`src/vga.c`、`src/drm.c`、`src/x11.c`、`src/kbd.c`、`src/mouse.c`、`src/ime.c`、`src/acpi.c`、`src/pci.c`、`src/usb.c`、`src/install.c`。

术语沿用《cunos 操作指南》第 1 节：卷、当前卷、rootfs、安装介质、内建命令、外部命令、`/bin` 程序。

---

## 1. 代码地图

`src/` 共约 100 个 `.c`/`.h` 文件，按职责分组如下（括号内为主要源文件，大小取自仓库实际字节数）：

| 层 | 模块 | 主要文件 | 职责 |
| --- | --- | --- | --- |
| 引导 | Multiboot 与汇编底座 | `boot.S`（7156 B） | Multiboot 头、内核栈与系统调用栈、入口 `_start`、部分中断/特权级汇编 |
| 核心 | 内核主控 | `kernel.c`（60852 B） | `kernel_main()`、初始化编排、shell、`help_table`、内建命令 |
| 底座 | 段与任务 | `gdt.c`（3092 B） | GDT、代码/数据段、TSS、`tss_set_esp0()` |
| 底座 | 中断 | `idt.c`（4343 B） | 256 项 IDT、异常处理、IRQ 入口 |
| 内存 | 分页 | `paging.c`（9812 B） | 二级页表、物理页分配、用户地址空间、MMIO 4MB 映射、页目录克隆 |
| 内存 | 堆与虚拟内存 | `mm.c`（12693 B） | `kmalloc`、VMA 链表、缺页、写时复制（COW）、页缓存 |
| 进程 | 进程管理 | `proc.c`（6546 B） | PCB 表、fork/execve/wait、协作式切换 |
| 装载 | ELF | `elf.c`（15253 B）、`elf_dyn.c`（9390 B） | ET_EXEC 装载；PIE/ET_DYN 与 `PT_INTERP` 动态链接 |
| 系统调用 | 分派 | `syscall.c`（29149 B） | `int 0x80` 分发、i386 系统调用号映射、`socketcall` |
| 文件系统 | VFS | `vfs.c`（17088 B） | 32 项 fd 表、路径解析、`stat` 布局、四个字符设备后端 |
| 文件系统 | FAT | `fat.c`（37408 B）、`fat.h` | FAT12/16/32 读写、8.3 短名、`fat_mkpath` |
| 文件系统 | ext2 | `ext2.c`（10555 B） | ext2 只读：超级块、inode、目录遍历 |
| 文件系统 | 路径 | `path.c`（5260 B） | 当前目录、`cd -` 历史、路径拼接 |
| 存储 | 块层 | `ata.c`（5266 B）、`ahci.c`（6312 B） | ATA PIO；AHCI（SATA） |
| 网络 | 网卡抽象 | `nic.c`（4340 B）、`nic.h` | 驱动注册与探测 |
| 网络 | 驱动 | `e1000.c`（13011 B）、`rtl8139.c`（5889 B）、`rtl8169.c`（7910 B）、`ne2k.c`（6581 B） | 四类 PCI 网卡 |
| 网络 | 协议栈 | `net.c`（27619 B） | ARP、IPv4、ICMP、UDP、TCP 与命令入口 |
| 图形 | 文本与帧缓冲 | `vga.c`（18071 B） | VGA 文本 80x25、VBE 帧缓冲绘制、字体渲染、分页输出 |
| 图形 | 图形设备层 | `drm.c`（13268 B） | `/dev/fb0`、`/dev/dri/card0` 的 ioctl |
| 图形 | 窗口系统 | `x11.c`（70842 B） | COM1 上的 X11 core 子集服务端、窗口管理 |
| 输入 | 键盘 | `kbd.c`（4013 B） | PS/2 键盘、扫描码到键符、`/dev/kbd` |
| 输入 | 鼠标 | `mouse.c`（3510 B） | PS/2 辅助设备、三字节包解析 |
| 输入 | 输入法 | `ime.c`（12613 B）、`cjkdata.c`、`cjkword.c` | 中/俄/德输入法、3000 汉字表、拼音词库 |
| 国际化 | 语言 | `i18n.c`（6289 B） | 四语言消息表、`lang` 切换 |
| 字体 | 点阵 | `font8x16.c`、`fontwide.c`（585759 B） | ASCII 8x16；CJK 宽字点阵 |
| 平台 | ACPI | `acpi.c`（6253 B） | RSDP/FADT 查找、PM1a 关机 |
| 平台 | PCI | `pci.c`（5938 B） | 配置空间枚举、BAR 与 MMIO 映射 |
| 平台 | USB | `usb.c`（4377 B） | 主机控制器与端口枚举 |
| 安装 | 安装器 | `install.c`（16630 B） | MBR/core.img 写入、FAT32 格式化、内核与 `/bin` 落盘 |
| 工具 | 编辑器 | `editor.c`（7467 B） | 全屏编辑器 |
| 基础 | 运行时 | `common.h`、`string.c` | 类型、端口 IO、串口、`memset/memcpy/str*`，无 libc |

依赖方向总体是自下而上：`common.h` → `paging/mm` → `vfs` → `elf/proc/syscall` → `kernel`。图 1 给出主要依赖（箭头表示「调用」）。

```
                        ┌──────────────────────────────┐
                        │           kernel.c           │
                        │ kernel_main / shell / cmd_*  │
                        └───┬────┬────┬────┬────┬──────┘
             ┌──────────────┘    │    │    │    └────────────┐
             ▼                   ▼    ▼    ▼                 ▼
      ┌────────────┐   ┌──────────────┬─────────┐   ┌──────────────┐
      │  editor.c  │   │  syscall.c   │  x11.c  │   │ install.c    │
      └─────┬──────┘   └──┬────┬──────┴────┬────┘   └──┬─────┬─────┘
            │             │    │           │           │     │
            ▼             ▼    ▼           ▼           ▼     ▼
      ┌──────────┐  ┌─────────┐ ┌─────┐ ┌──────┐ ┌────────┐ ┌──────┐
      │  ime.c   │  │ proc.c  │ │vfs.c│ │drm.c │ │ fat.c  │ │ata.c │
      └────┬─────┘  └────┬────┘ └──┬──┘ └──┬───┘ └───┬────┘ └──┬───┘
           │             ▼         │       ▼         ▼         ▼
           │        ┌────────┐     │  ┌────────┐ ┌──────┐ ┌────────┐
           │        │ elf.c  │     │  │ vga.c  │ │ext2.c│ │ahci.c  │
           │        │elf_dyn │     │  └────────┘ └──┬───┘ └────────┘
           │        └───┬────┘     │                │
           ▼            ▼          ▼                ▼
      ┌──────────────────────────────────────────────────────┐
      │        paging.c   mm.c   (页表 / 物理页 / kmalloc)   │
      └──────────────────────────────────────────────────────┘
                              ▲
                              │
      ┌───────────────────────┴──────────────────────────────┐
      │  boot.S / gdt.c / idt.c / common.h（IO、串口、字符串）│
      └──────────────────────────────────────────────────────┘
```

图 1 主要模块依赖关系

---

## 2. 启动链路

### 2.1 从加电到 `_start`

1. BIOS/UEFI（CSM）读 MBR，交给 GRUB。
2. GRUB 依据 `grub.cfg` 的 `multiboot /boot/cunos.bin` 按 Multiboot 1 规范装载内核镜像。`src/grub.cfg` 先 `insmod all_video`，再 `set gfxmode=1024x768x32,...` 与 `set gfxpayload=1024x768x32`，因此内核拿到的往往已经是帧缓冲模式。
3. GRUB 已把 CPU 置于 32 位保护模式、开启分页（GRUB 自己的页表）并开 A（对齐）标志，把 `eax`=Multiboot 魔数 `0x2BADB002`、`ebx`=multiboot_info 指针后跳到内核 ELF 入口。

### 2.2 `boot.S` 的职责

`src/boot.S` 只做四件事（第 1–60 行可核对）：

1. **Multiboot 头**：魔数 `0x1BADB002`，标志 `0x00000007`——bit0 要求模块 4KB 对齐、bit1 要求 GRUB 提供内存信息、bit2 请求显示模式；随后补齐 a.out kludge 五个占位字段（`header_addr`…`entry_addr` 全为 0），注释明确说明「bit16 未置位时被忽略，但必须占位，否则 video 字段会落在 GRUB 期望的偏移 32 之外」；再给出 `mode_type=0`（线性图形）、`width=1024`、`height=768`、`depth=32`。
2. **两个 16 KB 栈**：`stack_bottom..stack_top`（16 KB，位于 `.bss`，不占镜像体积）用于内核正常运行；`sys_stack_bottom..sys_stack_top`（16 KB）供 TSS 的 `esp0` 使用，即**从 ring3 陷入时使用的中断栈**，其顶部符号 `sys_stack_top` 被 `kernel_main()` 传给 `tss_set_esp0()`。
3. **入口 `_start`**：`cli` 关中断 → `mov $stack_top, %esp` → 以 `push %ebx` / `push %eax` 传参 → `call kernel_main`。
4. **兜底死循环**：`kernel_main` 返回后 `cli; hlt; jmp 1b`。

### 2.3 `kernel_main()` 初始化顺序

`src/kernel.c` 的 `kernel_main(u32 magic, u32 mbi)` 严格按下面顺序编排，顺序本身即依赖约束：

| 序 | 调用 | 为什么必须在这个位置 |
| --- | --- | --- |
| 1 | `serial_init()` | 后续任何打印/调试都要 0x3F8（以及 0x2F8 调试口）可用 |
| 2 | `vga_init(mb_info)` | 依据 multiboot 信息决定走 VBE 帧缓冲还是 80x25 文本，之后 `printf` 才有落点 |
| 3 | `paging_init()` | 建立分页基础设施与物理页分配器 |
| 4 | `paging_map_mmio_4m(vga_fb_addr(), vga_fb_bytes())` | 帧缓冲物理地址要作为 4 MB 大页（带 PCD/PWT）恒等映射才能直接写 |
| 5 | `gdt_init()` | 装 GDT（含用户段） |
| 6 | `tss_set_esp0(&sys_stack_top)` | ring3→ring0 陷入时栈切换目标 |
| 7 | `idt_init()` | 装 IDT，此后异常可捕获 |
| 8 | `cpu_enable_sse()` | 置 CR4.OSFXSR/OSXMMEXCPT 等，允许 SSE |
| 9 | `mm_init()` | 建立 `kmalloc` 堆、VMA 与页缓存结构 |
| 10 | `kbd_init()` | PS/2 键盘 |
| 11 | `pci_scan()` | 枚举 PCI 设备，为后续网卡/存储/显卡探测提供依据 |
| 12 | `pci_map_mmio()` | 把各设备 BAR 映射进内核空间 |
| 13 | `net_init()` | 依 `pci_scan()` 结果探测网卡 |
| 14 | `drm_init()` | 图形设备层 |
| 15 | `usb_init()` | USB 控制器与端口枚举 |
| 16 | `ata_init()` | ATA/AHCI 块设备 |
| 17 | `probe_filesystems()` | 探测磁盘卷，登记为 1–2 个卷 |
| 18 | `path_init()` | 当前目录状态归位 |
| 19 | `load_lang_setting()` | 读 `/etc/lang` |
| 20 | `proc_init((u32)&sys_stack_top)` | 进程表，记录内核系统栈顶 |
| 21 | `load_ime_setting()` | 读 `/etc/ime` |
| 22 | `acpi_init()` | 供 `shutdown` 使用 |
| 23 | `banner()` | 打印启动横幅 |
| 24 | `shell()` | 进入 REPL，返回后 `hlt` 死循环 |

关键点：**文件系统探测排在第 17 位，而语言/输入法设置读在第 19、20 位**，因此 `/etc/lang`、`/etc/ime` 只能在 FAT 卷上（`fat_path_lookup` + `fat_read_at`），读不到就用默认值，不影响启动。

```
GRUB ──► _start ──► kernel_main ──► serial ─► vga ─► paging ─► mmio ─► gdt/tss/idt
                                          ─► sse ─► mm ─► kbd ─► pci ─► net ─► drm ─► usb
                                          ─► ata ─► probe_fs ─► path ─► lang ─► proc
                                          ─► ime ─► acpi ─► banner ─► shell()
```

图 2 启动时间线

---

## 3. 特权级与中断底座

### 3.1 GDT 与 TSS

`src/gdt.c` 建立平坦段模型：代码段与数据段基址 0、上限 4 GB，另设 ring3 的代码段与数据段，以便 `/bin` 程序在用户态运行；同时装一个 TSS 描述符。

TSS 的唯一实质用途是 `esp0`（`gdt.h` 的 `tss_set_esp0()`）：当 ring3 程序执行 `int $0x80` 或触发异常/中断时，CPU 从 TSS 取 `esp0`/`ss0` 切到内核栈，即 `boot.S` 预留的 16 KB `sys_stack_top`。**没有使用硬件任务切换**（`ltr` 只装一次 TSS），也没有 I/O 位图权限控制。

### 3.2 IDT 与异常处理

`src/idt.c` 装 256 项 IDT，覆盖 0–31 号处理器异常与 PIC 重映射后的 IRQ，以及 `0x80` 系统调用门。异常处理的核心价值是**不静默三连故障**：处理函数会 dump 现场（错误码、EIP、CS、EFLAGS、通用寄存器栈帧）到串口与屏幕，便于用 `tools/int_trace.py` 复现定位。

IRQ 面：PS/2 键盘（IRQ1）、PS/2 鼠标（IRQ12）、定时器等按 8259 PIC 的常规重映射配置。**cunos 不依赖定时器中断做抢占**（见第 5 节），时钟主要用于时间查询类系统调用。

---

## 4. 分页与内存管理

### 4.1 地址空间布局

`src/paging.h` 定义用户地址空间（对齐 i386 Linux 习惯）：

| 常量 | 值 | 含义 |
| --- | --- | --- |
| `PAGE_SIZE` | 4096 | 页大小 |
| `USER_ELF_BASE` | `0x08048000` | 传统 ET_EXEC 装载点 |
| `USER_BRK_BASE` | `0x09000000` | 堆起点，`brk` 按需扩页 |
| `USER_MMAP_BASE` | `0x40000000` | 匿名映射区，向上增长 |
| `USER_STACK_TOP` | `0xC0000000` | 用户栈顶 |
| `USER_STACK_SIZE` | 64 KB | 用户栈大小 |

页表项标志只有 `PAGE_PRESENT`(0x001)、`PAGE_RW`(0x002)、`PAGE_USER`(0x004) 三个被显式使用；**没有实现 NX 位保护**（i386 上也需 PAE 才可用）。

### 4.2 两级页表操作

`src/paging.c` 提供的最小原语：

- `paging_map_page(vaddr, paddr, flags)`：建立 4 KB 映射，返回 0 成功。
- `paging_get_pte(vaddr)` / `paging_set_pte_flags(vaddr, flags)` / `paging_unmap_page(vaddr)`：直接读写/失效 PTE，供 COW 与按需分页使用。
- `paging_virt_to_phys(vaddr)`：地址翻译，DMA 与 `stat` 等场景使用。
- `paging_map_user_range(vaddr, len, flags)`：把一段用户地址按页映射到新物理页（已映射的跳过）。
- `paging_map_mmio_4m(phys, len)`：把物理区间以 **4 MB 大页**恒等映射进内核空间，带 PCD/PWT 绕 cache，专门用于 VBE 帧缓冲这类 MMIO。
- `user_write_bytes` / `user_read_bytes` / `user_zero_bytes`：**跨页安全**的用户内存读写（返回 -1 表示未映射），系统调用里拷贝用户缓冲区一律走这三个函数，避免用户传坏指针把内核打挂。

### 4.3 物理页分配与堆

- 物理页分配是 `pmm_alloc_page()`：**bump 分配 + 清零**，失败返回 0；`pmm_pages_used()`、`paging_pmm_free_bytes()` 用于统计。**没有实现页回收与 buddy/slab**。
- `paging_init()` 会避开 `CUNOS_PMM_RESERVE`（`common.h` 中为 `8*1024*1024`，即前 8 MB）这段内核自身与低端内存。
- `mm_init()` 建立 `kmalloc` 堆，`kheap_used()` 报告占用。`mm.c` 还负责 VMA 链表（`mem` 命令的数据源）、缺页处理、**写时复制（COW）**与页缓存计数：`paging_clone_pd()` 在 `fork` 时深拷贝用户页，实际共享则靠 COW 在写时复制。

### 4.4 多进程页目录

`paging_pd_phys()` 取当前页目录物理地址，`paging_switch_pd(phys)` 切 CR3，`paging_clone_pd()` 复制页目录，`paging_clear_user()` 在进程退出时清用户映射。这套接口构成了 `fork` 与进程切换的内存侧支撑。

---

## 5. 进程与调度

`src/proc.c`（251 行）实现进程管理的最小闭环：

- PCB 表保存 pid、页目录物理地址、用户栈、`brk` 当前值、退出码、父进程、状态。`proc_pid()` 提供 `getpid` 语义。
- `proc_init((u32)&sys_stack_top)` 记录内核系统栈顶，供陷入时定位内核栈。
- **调度是协作式的**：进程在系统调用里主动让出（`fork`/`wait4`/阻塞 I/O 处），没有基于时钟中断的抢占，也没有优先级与时间片。`help_planned[]` 中把 `preemptive scheduler` 列为未实现项，与此一致。
- **没有 SMP**：`help_planned[]` 明确写 `SMP` 未实现，`paging_*` 与 `proc_*` 中也没有自旋锁/原子操作层。

支持的进程原语（`src/syscall.c` 分支可核对）：`fork`(2)、`waitpid`(7)、`wait4`(114)、`execve`(11)、`exit`(1)、`exit_group`(252)。验证程序为 `/bin/forktest.elf`、`/bin/exectest.elf`。

---

## 6. ELF 装载

装载分两条路径，由 `run` 的调用点决定（`cmd_run()`）：

### 6.1 ET_EXEC 静态装载（`src/elf.c`）

- 校验 ELF 头（`\x7fELF`、`EM_386`、`ET_EXEC`），按 `PT_LOAD` 段把文件内容落到 `p_vaddr`，`.bss` 部分清零。
- 在 `USER_STACK_TOP` 之下建立 64 KB 用户栈，把 `argc`/`argv` 压栈（`/bin` 程序的 `crt0.c` 正是从栈上取 `argc/argv` 交给 `cu_main`）。
- 设置用户态 `ss:esp`、`cs:eip` 后 `iret` 进入 ring3。

### 6.2 PIE / ET_DYN 与动态链接（`src/elf_dyn.c`）

当 `run` 的路径以 `/` 开头且 rootfs 已挂载时，走树形 VFS 路径，此时支持：

- `ET_DYN`（PIE）按重定位基址装载，处理 `R_386_RELATIVE` 等重定位项。
- `PT_INTERP`：先装载解释器（如 `/lib/ld-linux.so.2`），按 Linux i386 习惯把 `AT_PHDR`、`AT_ENTRY`、`AT_BASE` 等辅助向量压到用户栈，再跳到解释器入口。

对应验证程序：`/bin/pietest.elf`（ET_EXEC 形态的 PIE 测试）与 `/bin/pie.elf`（`-fPIE -pie` 编译的 ET_DYN），源码 `user/bin/pietest.c`、`user/pie/pie.c`。编译参数见 `tools/build_bin.sh`：常规程序用 `-Wl,-Ttext=0x08048000`，PIE 程序用 `-fPIE -pie -nostdlib -Wl,-e,_start`。

**未实现**：`dlopen`/共享库多模块加载、`ELF` 的 `PT_GNU_STACK` 栈可执行性控制、TLS（`set_thread_area` 只做占位，见第 7 节）。

---

## 7. 系统调用分发

### 7.1 ABI 与入口

- 指令：`int $0x80`（`user/bin/cu.h` 中 `sys1`/`sys3` 内联汇编可核对）。
- 寄存器：`eax` = 系统调用号，`ebx`/`ecx`/`edx` = 第 1/2/3 个参数，返回值写回 `eax`。
- 栈切换：TSS.`esp0` → `sys_stack_top`。
- 分发函数：`src/syscall.c` 的主体是 `switch (n)`，绝大多数分支是「系统调用号 → `sys_*()` 实现」的一行映射。

### 7.2 已实现的系统调用号（按功能分组，摘自 `src/syscall.c`）

| 功能 | 系统调用号 |
| --- | --- |
| 进程 | `exit`(1)、`exit_group`(252)、`fork`(2)、`waitpid`(7)、`wait4`(114)、`execve`(11)、`getpid`(20)、`gettid`(224)、`getppid`(64) |
| 身份（占位，恒返回 1 或 0） | `getuid`(24)/`geteuid`(49)、`getgid`(47)/`getegid`(50)、`getuid32`(199)/`getgid32`(200)、`geteuid32`(201)/`getegid32`(202) |
| 文件读写 | `read`(3)、`write`(4)、`pread64`(180)、`pwrite64`(181)、`readv`(145)、`writev`(146)、`lseek`(19)、`dup`(41) |
| 打开关闭 | `open`(5)、`openat`(295)、`close`(6) |
| 目录与元数据 | `stat`(106)、`fstat`(108)、`fstat64`(197)、`newfstatat`(300)、`statx`(383)、`access`(33)、`mkdir`(39)、`unlink`(10)、`chdir`(12)、`getcwd`(183)、`getdents64`(220)/`getdents`(141) |
| 内存 | `brk`(45)、`old_mmap`(90)、`mmap2`(192)、`munmap`(91)、`mprotect`(125)、`msync`(144) |
| 网络 | `socketcall`(102)（内部再分 `socket`/`bind`/`connect`/`listen`/`accept`/`getsockname`/`send`/`recv`/`sendto`/`recvfrom`/`shutdown`/`setsockopt`/`getsockopt`） |
| 杂项 | `uname`(122)、`time`(13)、`gettimeofday`(78)、`ioctl`(54)、`fcntl`(55)、`getrlimit`(76)/`ugetrlimit`(191)、`clock_nanosleep`(265)、`getrandom`(355)、`set_tid_address`(258)、`set_robust_list`(311)、`set_thread_area`(243)、`rseq`(386) |

几处值得注意的实现取舍：

- **`openat` 忽略 `dirfd`**，路径仍按当前卷解析（源码注释如此）。
- **`access` 只判断路径是否存在**，因为没有权限模型。
- **`ioctl` 只有图形设备走 DRM 形状**，其它 fd 一律返回「无操作」。
- **`fcntl` 只是告诉调用者「没有特殊行为」**，不实现锁。
- **`getrlimit` 固定值**：`RLIMIT_STACK`=8 MB、`RLIMIT_NOFILE` 固定、`RLIMIT_CORE`=不产生 core。
- **`rseq` 明确声明不支持**，让 glibc 走退化路径；`getrandom`、`set_tid_address`、`set_robust_list` 属兼容桩。
- **`sigaction`/`sigprocmask` 系列是空实现**：能返回，但**信号机制整体未实现**（`help_planned[]` 亦列出 `signal` 未实现）。
- **`accept` 暂无被动连接**（`case 5: /* accept：暂无被动连接 */`），即 TCP 只有客户端能力。

### 7.3 调用路径示例：`cat a.txt`

```
cat.elf: sys3(5, "a.txt", O_RDONLY, 0)
   └─ int $0x80 eax=5 → TSS.esp0 切栈 → syscall.c case 5
        └─ sys_open → vfs_open → 卷判定（ext2 优先，否则 fat_path_lookup）
             └─ 返回 fd → cat.elf: sys3(3, fd, buf, 4096)
                  └─ sys_read → vfs_read → fat_read_at / ext2 读取
                       └─ copy_to_user（user_write_bytes 跨页安全）→ 用户缓冲
```

---

## 8. VFS 与文件系统

### 8.1 VFS 抽象

`src/vfs.h` 定义的边界：

- `VFS_MAX_FD` = 32，fd 0/1/2 固定为标准输入/输出/错误；`VFS_PATH_MAX` = 256。
- 打开标志与 POSIX 数值对齐，便于用户程序直接透传：`VFS_O_RDONLY`(0)、`VFS_O_WRONLY`(1)、`VFS_O_RDWR`(2)、`VFS_O_CREAT`(0x40)、`VFS_O_EXCL`(0x80)、`VFS_O_TRUNC`(0x200)、`VFS_O_APPEND`(0x400)、`VFS_O_DIRECTORY`(0x10000)。
- 错误码用负值：`VFS_ENOENT`(2)、`VFS_EBADF`(9)、`VFS_ENODEV`(19)、`VFS_ENOTDIR`(20)、`VFS_EISDIR`(21)、`VFS_EINVAL`(22)、`VFS_ENOSPC`(28)、`VFS_EROFS`(30)。
- `vfs_stat_t` 按 **i386 Linux 64 字节布局**声明（`st_dev/st_ino/st_mode/st_nlink/st_uid/st_gid/st_rdev/st_size/st_blksize/st_blocks/时间戳五组/保留`），只做布局兼容，数值由本 VFS 填写。
- `VFS_WRITE_MAX` = 32768，是内核侧「整文件读改写」缓冲上限。

后端选择规则：`vfs_mount_ext2()` 成功后 **ext2 优先**，`vfs_ext2_ready()` 为真时走树形路径解析；FAT 退为备用后端（`vfs_set_current(fat_fs*)`）。

### 8.2 四个字符设备后端

| 后端常量 | 路径 | 行为 |
| --- | --- | --- |
| `VFS_BACKEND_FB`(2) | `/dev/fb0` | 帧缓冲，读写像素区 |
| `VFS_BACKEND_DRI`(3) | `/dev/dri/card0` | 图形设备，ioctl 走 DRM 形状 |
| `VFS_BACKEND_KBD`(4) | `/dev/kbd` | 非阻塞单键读取 |
| `VFS_BACKEND_VT`(5) | `/dev/vga` | ANSI 子集虚拟终端（`tetris` 用它画屏） |

`vfs_readdir()` 输出 **linux_dirent64 流**，正好对接 `getdents64`；`vfs_fd_info()` 是 `vfs` 命令的数据源。

### 8.3 FAT（读写）

`src/fat.c`（37 KB，全仓最大文件系统实现）支持 FAT12/16/32 的读写，关键约束：

- **短名 8.3**：`fat.h:7` `#define FAT_NAME_LEN 13`，注释写明 `8.3 + 点 + \0`；`fat_format_name()` 负责把输入规范化为短名。**长文件名（LFN）未实现**。
- 提供 `fat_mkpath()`（递归建父目录，`/bin/mkdir` 与 `enterf` 依赖）、`fat_path_lookup()`、`fat_read_at()`、`fat_write_path()`、`fat_mount(fs, part_lba)`（支持整盘与分区两种起点）。
- 卷可来自 MBR 分区（`part_lba`）或整盘。

### 8.4 ext2（只读）

`src/ext2.c` 实现超级块读取、块组与 inode 表定位、`ext2_lookup(path, &ino)`、`ext2_read_inode()` 与目录项遍历，`ext2_mount(fs, 1)` 的第 2 个参数表示从第 2 块盘读取。

**ext2 仅支持读**：没有分配器、没有写入、没有删除，也没有日志恢复。`help_planned[]` 中 `ext4 write` 列为未实现，与此一致。

### 8.5 路径层

`src/path.c` 维护当前工作目录（字符串）与上一目录（`path_prev()` 支撑 `cd -`），`path_abs()` 负责相对→绝对转换，`path_disp()` 负责提示符里显示的窄化展示。`path_init()` 在初始化与切换卷时调用。

---

## 9. 块层

| 实现 | 文件 | 能力 |
| --- | --- | --- |
| ATA PIO | `src/ata.c` | 主/从盘识别、`ata_present()`、`ata_sector_count()`、按 LBA 读写扇区；`cuninstall` 与 `mkfat32` 的写盘都走这里 |
| AHCI | `src/ahci.c` | SATA 控制器初始化、命令列表/命令表、端口 DMA 读写 |

`kernel_main()` 里只有一次 `ata_init()`，AHCI 由 `ata.c`/`ahci.c` 内部按控制器类型选择链路（`cuninstall` 打印 `link: AHCI` 或 `link: ATA PIO` 即由此而来）。**未实现**：NVMe、USB 存储、软驱、分区表的 GPT 解析（`mkfat32`/`cuninstall` 写的是 MBR + 单 FAT32 分区）。

---

## 10. 网络栈

### 10.1 网卡抽象

`src/nic.c` / `nic.h` 定义统一的 NIC 操作表（探测、收发、MAC 读取），`net_init()` 依次尝试：`e1000` → `rtl8139` → `rtl8169` → `ne2k`。探测依据是 PCI 的 `vendor:device`（`pci_scan()` 结果），对应源码 `src/e1000.c`、`src/rtl8139.c`、`src/rtl8169.c`、`src/ne2k.c`。

**未实现**：virtio-net、USB 网卡、无线网卡、多网卡并行（只用一个活动 NIC）、网卡中断驱动收包（`net_poll()` 轮询，在 shell 每轮循环里被调用）。

### 10.2 协议栈

`src/net.c`（27 KB）实现：

- **ARP**：`arp_resolve()`、缓存最多 8 项（`net` 输出的 `arp=` 计数）。
- **IPv4**：默认 `10.0.2.15 / 255.255.255.0 / 10.0.2.2`，即 QEMU slirp 网段。
- **ICMP**：`net_ping()`，默认 4 次。
- **UDP**：收发与计数。
- **TCP**：客户端握手、收发、关闭；`net_tcp_test()` 供 `tcp` 命令；`chat` 在其上做全双工交互。
- 统计：`net_dump()` 输出 `arp=/ip=/icmp=/udp=/tcp=/drop=`。

**未实现**：DHCP、DNS 解析（`curl`/`ping` 只接受点分 IPv4）、IPv6、分片重组、TCP 服务端被动连接（`accept` 为桩）、netfilter/防火墙。

### 10.3 一次 `ping` 的数据流

```
ping 10.0.2.2
  └─ net_ping(dst=10.0.2.2, count=4)
       ├─ arp_resolve(gw)  ──► ARP 请求/应答 ──► 缓存 MAC
       ├─ icmp_build(type=8) ──► ip_send ──► nic_send（驱动发送环）
       └─ net_poll() 轮询收包 ──► ip_input ──► icmp_reply ──► 打印 RTT/序号
```

---

## 11. 图形栈

分四层，自下而上：

| 层 | 文件 | 内容 |
| --- | --- | --- |
| 帧缓冲层 | `vga.c` | VBE 模式探测（`vga_init(mb_info)`）、`vga_fb_active()`、`vga_fb_addr()`/`vga_fb_bytes()`、像素/矩形/线/圆/多边形绘制、`font8x16` 与 `fontwide` 字形渲染、`vga_color_bar()`、`vga_pager_set()` 分页输出 |
| 图形设备层 | `drm.c` | `/dev/fb0` 与 `/dev/dri/card0`，`drm_selftest()`（`drm` 命令） |
| 窗口系统 | `x11.c`（70 KB） | COM1 上的 X11 core 协议子集服务端 + 窗口管理（拖动、最小化、最大化、关闭、焦点） |
| 桌面 | `kernel.c` 的 `cmd_desktop()` / `desktop_frame()` | 桌面外壳、顶栏、任务栏、内嵌终端 |

X11 传输是本项目的一个特殊设计：**GUI 走 COM1（0x3F8）115200 8N1 串口**，客户程序在宿主侧，服务端在内核里把请求转成帧缓冲绘制。`common.h` 里 `SERIAL_X11=0x3F8`、`SERIAL_DBG=0x2F8` 与 `xser_init()/xser_ready()/xser_get()/xser_putc()` 就是这条通道。

`desktop_frame()` 绘制的骨架（可对照源码中的字符串常量）包括顶栏 `cunos desktop   Applications   Places   System`、右侧时钟、图标区 `Home`/`FAT32`/`FAT16`、主窗口标题 `/  -  File Manager`、表头 `Name/Type/Size`、底部任务栏 `[K] File Manager` 与版本串 `cunos 0.3.0`、以及 `Press any key to return to shell` 提示。

**未实现**：3D 加速、硬件光标、多显示器、`libX11` 的完整请求集、合成（compositing）。`kde` 命令只是只读探测（`kde_list_cb` 列 ext2 ino，最多 10 项），**不启动任何桌面**。

---

## 12. 输入栈

```
PS/2 键盘 ──IRQ1──► kbd.c ──► 键符/扫描码 ──► /dev/kbd、readline、editor
PS/2 鼠标 ──IRQ12─► mouse.c ─► 三字节包解析 ─► x11.c 指针事件
键盘字节 ──► ime.c ──► cjkdata.c(3000 汉字) / cjkword.c(拼音词库) ──► UTF-8 上屏
```

`src/ime.c` 是命令行输入的核心：`ime_toggle()`（Ctrl+Space）、`ime_key()`（返回 >0 上屏文本 / 0 被消费 / `IME_PASS`、`IME_PASS_BUF` 透传）、`ime_reset()`、`ime_status_refresh()`（底部候选行），并提供 `ime_lookup()`/`ime_name()`/`ime_code()` 供 `ime` 命令使用。四种输入法：`en`、`zh`（拼音，空格或 `1`–`9` 上屏，ESC 取消）、`ru`（拉丁转写西里尔）、`de`（死键 `"` + `a/o/u/s`）。

字体：`font8x16.c` 提供 ASCII 8x16 点阵，`fontwide.c`（586 KB，全仓最大文件）提供 CJK 宽字点阵，`cjkdata.c`（90 KB）是 3000 汉字表，`cjkword.c`（144 KB）是拼音词库，三者由 `tools/gen_font.py`、`tools/gen_cjk3000.py`、`tools/gen_i18n.py` 生成。

**未实现**：USB HID 键盘鼠标（只枚举 USB，不提供 HID 驱动）、触摸板多点、IME 与编辑器的集成（编辑器只接受 32–126 可打印 ASCII）。

---

## 13. ACPI / PCI / USB

### 13.1 PCI

`src/pci.c`：通过配置空间（0xCF8/0xCFC）枚举总线/设备/功能，记录 `bus:slot.func vendor:device class`；`pci_map_mmio()` 把 BAR 映射进内核空间，供网卡与图形设备使用。`cmd_pci()` 输出设备总数与列表，并附 `print_gpu()`/`print_usb()` 摘要。

### 13.2 ACPI

`src/acpi.c` 查找 RSDP → RSDT → FADT，取 `PM1a_CNT` 与 SLP_TYP，写 S5 实现软关机；找不到 FADT 时退回传统 PM1a 端口并打印提示。**未实现**：AML 解释器、电源管理策略、睡眠状态（S1–S4）、电池与温度。

### 13.3 USB

`src/usb.c` 枚举 USB 主机控制器（UHCI/OHCI/EHCI 形态）与端口状态，`cmd_usb()` 打印结果。**未实现**：完整的 URB 提交、枚举（SET_ADDRESS/GET_DESCRIPTOR 流程）、任何设备类驱动（HID/存储/音频）。

---

## 14. 安装器与内嵌载荷

`src/install.c` 是「把系统装到裸盘」的完整实现，也是理解磁盘镜像布局的最佳入口：

- 两个载荷通过 `blob_find("boot.img")` / `blob_find("core.img")` 从内核镜像内的 CGB1 容器取（`tools/mk_grub_blob.py` 生成 `build/grub_blob.bin`，`tools/mk_bin_blob.py` 生成 `build/bin_blob.bin`，两者头 4 字节均为 `CGB1`）。
- `PART_START` = 2048，分区大小 = `total - PART_START`。
- `build_mbr()` 把 `boot.img` 前 440 字节写进 MBR，并构造分区表：状态 0x80、CHS 起点 0x00/0x02/0x00、类型 0x0C（FAT32 LBA）、CHS 终点 0xFE/0xFF/0xFF、LBA 起点与扇区数，末尾 `0x55 0xAA`。
- 之后写 `core.img`（LBA 1 起）、格式化 FAT32 主分区、写 `/grub/grub.cfg`、复制内核到 `/cunos.bin`、把内嵌 `/bin` 程序写入 `/bin`，共 `[1/6]`–`[6/6]` 六步；`YES` + `GRUB` 双确认后才真正写盘。

---

## 15. 未实现能力汇总（架构层面）

| 领域 | 未实现 | 依据 |
| --- | --- | --- |
| 调度 | 抢占式调度、优先级、时间片；SMP 多核 | `help_planned[]`、`src/proc.c` |
| 同步 | 自旋锁/原子操作层（无 SMP，亦无内核互斥） | `src/proc.c`、`src/paging.c` |
| 信号与 IPC | 信号、`pipe`、`futex`、`shm`（`sigaction` 为桩） | `help_planned[]`、`syscall.c` 分支 |
| 权限 | uid/gid、文件权限检查、登录 | `help_planned[]`（`uid-gid permission checks`） |
| 虚拟文件系统 | `/proc`、`/sys`、`udev` | `help_planned[]` |
| ext2 | 写入、删除、日志、ext4 | `src/ext2.c` |
| FAT | 长文件名、多卷并发（最多 2 个卷） | `src/fat.h`、`kernel.c` |
| 块层 | NVMe、USB 存储、GPT | `src/ata.c`、`src/ahci.c` |
| 网络 | DHCP、DNS、IPv6、分片、TCP 服务端、virtio-net、无线 | `src/net.c`、`src/nic.c` |
| 图形 | 3D、硬件光标、多屏、完整 X11 请求集、合成器 | `src/vga.c`、`src/drm.c`、`src/x11.c` |
| 输入 | USB HID、触摸、编辑器内输入法 | `src/usb.c`、`src/editor.c` |
| 平台 | ACPI AML、睡眠状态、动态模块加载、cgroup、POSIX 定时器 | `src/acpi.c`、`help_planned[]` |
| 安全 | NX 保护、栈保护（编译期 `-fno-stack-protector`）、ASLR | `src/paging.h`、`Makefile` |

---

## 16. 源码索引表

| 想了解 | 直接看 | 关键符号 |
| --- | --- | --- |
| 引导与栈 | `src/boot.S` | `MB_FLAGS`、`stack_top`、`sys_stack_top`、`_start` |
| 初始化顺序 | `src/kernel.c` | `kernel_main()` |
| shell 与命令表 | `src/kernel.c` | `shell()`、`kernel_exec_line()`、`help_table[]`、`help_planned[]` |
| 段与 TSS | `src/gdt.c` | `gdt_init()`、`tss_set_esp0()` |
| 中断 | `src/idt.c` | `idt_init()` |
| 分页 | `src/paging.c` | `paging_map_page()`、`paging_map_mmio_4m()`、`user_write_bytes()` |
| 堆与 COW | `src/mm.c` | `kmalloc()`、VMA 链表、缺页处理 |
| 进程 | `src/proc.c` | `proc_init()`、`proc_pid()`、`paging_clone_pd()` 调用点 |
| ELF | `src/elf.c`、`src/elf_dyn.c` | `elf_exec()`、`elf_exec_path()` |
| 系统调用 | `src/syscall.c` | `switch (n)` 分发表、`sys_*()` |
| VFS | `src/vfs.c`、`src/vfs.h` | `vfs_open()`、`vfs_readdir()`、`VFS_MAX_FD` |
| FAT / ext2 | `src/fat.c`、`src/ext2.c` | `fat_path_lookup()`、`fat_mkpath()`、`ext2_lookup()` |
| 块层 | `src/ata.c`、`src/ahci.c` | `ata_present()`、`ata_sector_count()` |
| 网络 | `src/net.c`、`src/nic.c` | `net_init()`、`net_poll()`、`net_dump()`、`net_cmd_ifconfig()` |
| 图形 | `src/vga.c`、`src/drm.c`、`src/x11.c` | `vga_fb_active()`、`drm_selftest()`、`x11_run()` |
| 输入 | `src/kbd.c`、`src/mouse.c`、`src/ime.c` | `kbd_*`、`mouse_*`、`ime_key()` |
| 平台 | `src/acpi.c`、`src/pci.c`、`src/usb.c` | `acpi_init()`、`pci_scan()`、`usb_init()` |
| 安装 | `src/install.c` | `install_grub_run()`、`build_mbr()`、`grub_cfg` |
