# cunos 实体机配置参考

> 用途：把 cunos 从 QEMU 搬到真实硬件上时，需要什么样的机器、怎么配、会遇到什么。
> 依据：`src/pci.c`、`src/usb.c`、`src/acpi.c`、`src/ata.c`、`src/ahci.c`、`src/vga.c`、`src/kbd.c`、`src/mouse.c`、`src/e1000.c`、`src/rtl8139.c`、`src/rtl8169.c`、`src/ne2k.c`、`src/grub.cfg`、`tools/mk_grub_blob.py`、`src/install.c`
> 前提认知：cunos 走的是**传统 BIOS 引导链**（GRUB legacy multiboot → 1 MB 处 `_start`），不是 UEFI。这条决定了实机配置的第一道门槛。

---

## 1. 最低可运行配置

| 项目 | 要求 | 说明 |
|---|---|---|
| CPU | i386 及以上，**必须 32 位保护模式** | 支持 PAE 更好但非必需；x86_64 CPU 也可（内核只用 32 位模式） |
| 内存 | 32 MB 可跑起来，64 MB 起舒适，128 MB 推荐 | 内核堆 + 物理页池（`CUNOS_PMM_RESERVE` 8 MB 预留）+ 页缓存（`MM_PCACHE_MAX` 512 页） |
| 固件 | **BIOS / Legacy CSM 必须可用** | 纯 UEFI（无 CSM）的机器无法用这套引导链启动 |
| 显示 | VBE 2.0+，能给出 1024×768×32，退而求其次 800×600×32 / 640×480×32 | 拿不到帧缓冲会退到 VGA 文本 80×25，**中文无法显示** |
| 存储 | ATA/IDE（PIO）或 SATA（AHCI，单端口单命令槽） | NVMe 无驱动，不支持 |
| 键盘 | **PS/2 键盘**（或芯片组提供的 PS/2 兼容路径） | 见第 4 节，这是实机最容易翻车的地方 |
| 鼠标 | PS/2 鼠标（可选，桌面用） | 没有也能用键盘在 shell 里操作 |
| 网络 | 任一款第 5 节列出的网卡（可选） | 无网卡不影响启动，只是 `net` 命令无设备 |

**一句话结论**：一台 2010—2020 年间、带 CSM、有 PS/2 或 PS/2 兼容路径、SATA 走 AHCI 或 IDE 的机器最容易成功。纯 UEFI 新机、只有 USB 键盘、只有 NVMe 的机器基本起不来。

---

## 2. 引导方式与固件设置

### 2.1 为什么必须 Legacy/CSM

cunos 的镜像由 GRUB 的 `boot.img` + `core.img` 组成（`tools/mk_grub_blob.py` 生成，带 `CGB1` 魔数），`core.img` 是 **BIOS 引导**产物，不是 EFI 的 `.efi`。机器若只走 UEFI，固件根本不会执行这段 MBR 引导代码。

### 2.2 BIOS 设置清单

进固件设置（常见键：`F2` / `Del` / `F10` / `Esc`），逐项确认：

| 设置项 | 目标值 | 说不清就找的替代名 |
|---|---|---|
| Boot Mode | `Legacy` 或 `CSM` / `Legacy BIOS` | `UEFI and Legacy`、`CSM Support: Enabled` |
| Secure Boot | `Disabled` | 传统引导与 Secure Boot 通常不共存 |
| Fast Boot | `Disabled` | 加速启动会跳过部分初始化，可能让 VBE 模式枚举失败 |
| Storage / SATA Mode | `AHCI` 优先，或 `IDE` / `Compatibility` | `RAID`/`RST` 模式要改成 AHCI，否则 AHCI 驱动看到的是未就绪控制器 |
| USB Configuration | `Legacy USB Support: Enabled` | 影响 USB 键盘能否在 BIOS 阶段使用，也影响 `usb` 命令的端口可见性 |
| Boot Order | 首选 USB / 硬盘（视安装方式） | — |
| Virtualization | 与本项目无关，可保持默认 | — |

### 2.3 本机（ASUS Adol 14 M5451GA，AMD 平台）的实操提示

这台机器是 AMD 平台笔记本（Ryzen AI 9 H 465 一类），实机验证时注意：

1. ASUS 机型进 BIOS 通常 `F2`，启动菜单 `Esc`；部分型号需要先关 `Fast Boot` 才能看到 USB 启动项。
2. AMD 平台的 `IOMMU` / `SVM` 与 cunos 当前功能无关（内核未启用 SVM，VMM 尚在设计阶段），保持默认即可。
3. 笔记本常见「无 PS/2 键盘」——现代笔记本的键盘走 i8042 或 eSPI 模拟，不同机型差异大。**先验证键盘能不能用**：进 shell 后随便敲几个字符，能回显才算过。这一条过不了，后面全都做不了。
4. 分辨率：笔记本内屏的 EDID 未必提供 1024×768 的 VBE 模式（很多只给原生分辨率）。如果 `grub.cfg` 的三种模式都拿不到，GRUB 会退到 `auto`，内核可能拿到非 4:3 的比例甚至文本模式。必要时把 `grub.cfg` 的 `gfxmode` 加上机器原生分辨率再试。
5. 外接显示器（HDMI）时 VBE 模式列表会变，调试阶段建议固定用内屏或固定一种外接方式。

---

## 3. 安装到硬盘的两种方式

### 3.1 方式 A：在 cunos 里自装（`cuninstall`）

前提：已经能从一个可引导介质起 cunos，且能看到目标 ATA 盘。

```
cunos:fat32 / > fs                   # 确认目标盘索引
cunos:fat32 / > cuninstall
```

流程：输入 `YES` → 确认写 GRUB → `[1/6]`..`[6/6]`（分区、FAT32 引导区、GRUB、内核、`/bin` 程序）。任一步失败会在 shell 打印 `cuninstall: exit <code>`。

**警告（破坏性）**：`cuninstall` 会重写目标盘的分区表与引导区，盘上原有数据全部丢失。`mkfat32` 同理。实机上务必先把目标盘单独拆出来或用可弃盘，不要在系统盘上试。

### 3.2 方式 B：从 ISO 写到 U 盘

```bash
# Linux / WSL（注意 of= 别写错，写错就是销毁那块盘）
sudo dd if=cunos.iso of=/dev/sdX bs=4M status=progress oflag=sync
```

Windows 侧用 Rufus 选 `DD 模式`（不要用 ISO 模式，GRUB 的 MBR 引导需要原始写入）。

写完后 U 盘在 BIOS 里表现为「USB HDD」，需要在启动菜单里选对条目（有些机器同时列 `UEFI: xxx` 与 `xxx`，选后者）。

### 3.3 首次实机启动的观察顺序

1. GRUB 短暂闪过（`set timeout=0`，几乎不可见）。
2. 屏幕切到 1024×768 帧缓冲，打印 banner。
3. 检查 banner 的 `DISPLAY` 行是不是帧缓冲模式。
4. 检查 `STORAGE` 行有没有盘、扇区数是否合理（比如 64 GB 盘应该给出约 1.2 亿扇区）。
5. 检查 `FS` 行有没有卷、当前卷是不是 `*`。
6. 检查 `INPUT` 行，然后**敲键盘**确认能输入。
7. `fs`、`ls` 逐个试。

哪一步断，就回到第 8 节的排障表。

---

## 4. 输入设备：实机的最大不确定性

### 4.1 现状

cunos 的输入驱动只有两个：`kbd.c`（PS/2 键盘）与 `mouse.c`（PS/2 鼠标），走 i8042 控制器（端口 `0x60`/`0x64`）。**没有 USB HID 运行时驱动**——`usb.c` 只做控制器枚举与端口状态，不解析 HID 报告描述符，也不产生键盘事件。

### 4.2 结论与对策

| 场景 | 能否输入 | 对策 |
|---|---|---|
| 有真 PS/2 口的老机器/台式机 | 能 | 直接接 PS/2 键盘 |
| 笔记本内置键盘（走 i8042 模拟） | 多数能 | 先测；不行就换机 |
| USB 键盘 + BIOS 的 `Legacy USB Support` 开启 | **可能能**（BIOS 把 USB 键盘模拟成 PS/2） | 在 BIOS 开启该项后重试 |
| USB 键盘 + 无 Legacy USB | 不能 | 换 PS/2 键盘或换机 |

这条限制必须在上机前确认，否则会出现「系统启动了但一个字都敲不进去」的困局——而且此时没有任何补救手段（没有 USB HID 驱动就没有）。

### 4.3 输入法在实机上的表现

`ime zh` 的拼音输入依赖候选上屏（空格或 1-9）与宽字符字形绘制，两者都要求帧缓冲。实机上如果退到文本模式，输入法仍然会「工作」（能上屏字节），但**中文会显示成方块或乱码**，这不是输入法坏了，是显示后端退化了。

---

## 5. 网卡支持清单

识别由 `nic.c` 统一入口按 vendor/device 匹配，驱动各自持有一张 device id 白名单表。**准确的白名单以源码里的 `g_ids[]` 为准**，下表是家族级归纳：

| 驱动 | vendor | 覆盖家族 | 实机可得性 |
|---|---|---|---|
| `e1000.c` | `0x8086`（Intel） | 8254x / 8257x（82571/82572/82573/82574）/ 8258x / 8259x、I210、I217、I218、I219 系列（含大量 OEM 变体 device id） | 高。Intel 千兆网卡在台式机与商用本上普及度最高 |
| `rtl8139.c` | `0x10EC`（Realtek） | RTL8139 / 8100 / 8101 | 很高。老机器与廉价 USB/PCI 网卡常见 |
| `rtl8169.c` | `0x10EC` | RTL8168 / 8169 / 8167 / 8136 / 8137 / 8110 / 8112 / 8101E / 8102E / 8103E | 高。现代主板板载 Realtek 千兆多用这一族 |
| `ne2k.c` | `10EC:8029` | RTL8029AS / NE2000 兼容 | 低（老古董），但结构最简单，适合教学 |

### 5.1 关键警告

`rtl8169.c` 的源码注释明确写着：**QEMU 没有 RTL8168 模型，该驱动无法在 QEMU 中验证**。也就是说这一族在模拟器里永远绿不了，只能上实机。如果你的目标机是板载 Realtek 千兆（极常见），实机验证是唯一途径。

### 5.2 用 `pci` + `net` 定位问题

```
cunos:fat32 / > pci        # 看有没有对应 vendor/device 出现
cunos:fat32 / > net        # 看驱动是否绑定、链路是否 up、MAC 是否合理
```

- `pci` 有设备、`net` 无驱动 → device id 不在白名单里，需要往 `g_ids[]` 补一行。
- `net` 有驱动但 MAC 全 0 或全 FF → 初始化没读到 EEPROM/寄存器。
- 链路 down → 网线/交换机问题，或状态寄存器读错位。

### 5.3 网络配置

```
cunos:fat32 / > ifconfig 192.168.1.50 255.255.255.0 192.168.1.1
cunos:fat32 / > ping 192.168.1.1
cunos:fat32 / > tcp 192.168.1.1 80 GET / HTTP/1.0
cunos:fat32 / > chat 192.168.1.10 5555
```

实机没有 DHCP 客户端，**必须手工配 IP**。`chat` 只能当主动连接方（`accept` 未实现），对端要先监听。

---

## 6. 存储控制器

| 控制器 | 驱动 | 能力边界 |
|---|---|---|
| IDE / PATA（PIO） | `ata.c` | 主通道识别，MBR 分区扫描，`ata_sector_count()` |
| SATA（AHCI） | `ahci.c` | **单端口、命令槽 0、LBA48 DMA**。全局只有一个 ABAR 与一个就绪标志 |

### 6.1 AHCI 的实机注意

1. BIOS 里 SATA 模式若为 `RAID` / `RST` / `Intel Rapid Storage`，控制器对 cunos 呈现的不是标准 AHCI 寄存器语义，**必须改成 AHCI**。
2. 因为只用一个命令槽，同一时刻只有一条命令在飞。这在实机上意味着读大文件慢，但不会出错。
3. 端口签名 `SIG_ATA=0x00000101` 用于区分 ATA 设备与 ATAPI/其它，端口状态位（`P_SSTS`）能判链路建立。

### 6.2 不支持的存储

- **NVMe**：无驱动，完全不可见。
- **USB 存储**：`usb.c` 只做枚举，没有 UASP/BOT 类驱动，U 盘不能当文件系统卷用（只能用于 BIOS 阶段的启动）。
- **软驱**：无驱动。
- **光驱**：ATA 侧的 ATAPI 未做完整实现，别指望挂 CD。

---

## 7. 显示与图形

### 7.1 模式协商

`src/grub.cfg` 的写法：

```
insmod all_video
set gfxmode=1024x768x32,800x600x32,640x480x32,auto
set gfxpayload=1024x768x32
```

- `gfxmode` 是候选列表，**按顺序尝试**，第一个成功就用。
- 都失败时退到 `auto`，内核可能拿到非预期分辨率。
- `gfxpayload` 指定交给内核的帧缓冲模式，必须与 `gfxmode` 一致，否则内核拿到的尺寸与 GRUB 认为的不一致。

### 7.2 帧缓冲 MMIO 的硬要求

`kernel_main` 里必须调用：

```c
paging_map_mmio_4m(vga_fb_addr(), vga_fb_bytes());
```

帧缓冲一般挂在 PCI MMIO 高端（常见 `0xFD000000` 一带），不在内核启动时的恒等映射范围内。**不补这段映射，第一次写像素就是 #PF**，表现为「打印完 banner 前几行就崩」。这是实机移植最常见的第一个坑。

这段映射用 4 MB 大页并带 PCD/PWT，目的是绕过 cache——帧缓冲写入不需要缓存，走了缓存会既慢又可能花屏。

### 7.3 分辨率与文本列数

`disp_cols()` / `disp_rows()` 由帧缓冲尺寸和字形尺寸换算。1024×768 下用 8×16 字形，理论 128 列 × 48 行；实际实现会留边距。窗口管理（`x11.c`）的约束「左边至少露 80 像素、顶边不越界」也是按屏幕尺寸算的，换分辨率时这套逻辑要重新验证。

### 7.4 像素格式

`x11.c` 按 `0xRRGGBB`（depth 24 TrueColor）组织像素。如果实机拿到的模式是 16 位（`0xRRRRRGGGGGBBBBB`）或 32 位带 alpha，会出现颜色错乱——`vga.c` 按 `vga_fb_bpp()` 分支处理，换机器时这是要核对的一项。

---

## 8. 实机排障表

| 现象 | 最可能原因 | 处理 |
|---|---|---|
| 直接进原系统，不认 U 盘 | 启动项选错（选了 UEFI 条目）或 CSM 关闭 | 启动菜单选非 UEFI 条目；BIOS 开 CSM |
| 黑屏停在 GRUB 提示符 | `core.img` 没写入或盘不可读 | 重写 U 盘（DD 模式）；换盘 |
| 卡在 `BOOT: vga ok` 之后 | 帧缓冲 MMIO 未映射，写像素 #PF | 确认 `paging_map_mmio_4m()` 调用与参数 |
| banner 显示但 `DISPLAY` 是文本模式 | 三种 VBE 模式都拿不到 | 在 `gfxmode` 加机器原生分辨率；关 Fast Boot |
| 中文/宽字符显示成方块 | 文本模式或宽字形未加载 | 先解决帧缓冲模式 |
| `STORAGE: no ATA disk` | SATA 处于 RAID/RST 模式，或只有 NVMe | BIOS 改 AHCI；换有 SATA/IDE 的盘 |
| `FS: none` | 盘上没有可识别分区/卷 | `mkfat32` 或 `cuninstall` 先建卷 |
| 键盘完全无输入 | 只有 USB 键盘且无 Legacy USB 模拟 | BIOS 开 `Legacy USB Support`；换 PS/2 键盘 |
| 键盘输出乱字符 | 键盘布局/扫描码集差异 | 换键盘；记录扫描码再修 `kbd.c` |
| `pci` 看不到网卡 | 网卡是 USB 或过新（2.5G/10G） | 换第 5 节清单内的网卡 |
| `net` 显示驱动名但 MAC 全 FF | EEPROM 读取失败/初始化顺序 | 检查驱动初始化返回值；换同族别的卡试 |
| `shutdown` 无效 | ACPI 表不可用，或 `\_S5_` 解析失败 | 看 banner 的 POWER 行；改 BIOS 的 ACPI 相关项 |
| 桌面花屏 | bpp 不是 24/32，或分辨率非预期 | 核对 `vga_fb_bpp()` 与 `gfxmode` |
| 跑程序 #UD | SSE 未启用（实机 CPUID 走 SSE2 路径） | 确认 `cpu_enable_sse()` 在 `kernel_main` 中已执行 |

---

## 9. 实机与 QEMU 的差异清单

把在 QEMU 里「绿了」的东西搬到实机，这些点会不一样：

| 维度 | QEMU | 实机 |
|---|---|---|
| VBE 模式 | 固定几种，稳定可预期 | EDID 决定，笔记本内屏常缺 4:3 模式 |
| 键盘 | 虚拟 PS/2，永远可用 | 可能是 USB 键盘并经 BIOS 模拟，也可能没有 |
| 网卡 | `-device e1000` / `rtl8139` 好验；无 RTL8168 模型 | 板载多为 RTL8168 族，**只能在实机验证** |
| 存储 | 镜像文件，`-drive if=ide` 直接给 | SATA 模式受 BIOS 设置影响；NVMe 不可用 |
| ACPI | 表完整，S5 可用 | 各厂商 DSDT 差异大，`\_S5_` 解析要容错 |
| 计时 | 稳定 | PIT/TSC 频率需实测，别写死 |
| USB | 提供 UHCI/EHCI/XHCI 模型 | 现代机器基本只有 XHCI，`usb` 命令看到的多是 XHCI 主控 |
| 复位/关机 | 8042 复位与 ACPI S5 都灵 | 少数机器 8042 复位无效，只能物理断电 |

---

## 10. 上机检查单（照着走）

**准备阶段**

- [ ] 目标机 BIOS 可开 CSM/Legacy
- [ ] Secure Boot 已关，Fast Boot 已关
- [ ] SATA 模式为 AHCI 或 IDE（不是 RAID/RST）
- [ ] 备有 PS/2 键盘，或确认 Legacy USB 已开
- [ ] 目标盘可弃（会被重写分区表与引导区）
- [ ] 记下机器型号与网卡型号（板载 Realtek/Intel 哪一族）

**启动阶段**

- [ ] GRUB 闪过，进入 cunos banner
- [ ] `DISPLAY` 行为帧缓冲（`1024x768x32` 一类）
- [ ] `STORAGE` 行有盘且扇区数合理
- [ ] `INPUT` 行正常，且能实际敲入字符
- [ ] `POWER` 行反映 ACPI 状态

**功能阶段**

- [ ] `fs` 列出卷，`ls` 有输出
- [ ] `enter` 写文件 → `cat` 读回（验证 FAT 写路径）
- [ ] `edit` 全屏编辑，`^O` 保存 `^X` 退出
- [ ] `pci` 能看到网卡，`net` 驱动绑定且 MAC 合理
- [ ] `ifconfig` 配 IP，`ping` 网关通
- [ ] `mem` 基线合理，跑一个 `/bin` 程序后 VMA 数回到基线
- [ ] `desktop` 起窗口，拖动/最小化/最大化/关闭都正常，`Ctrl+Q` 退出
- [ ] `shutdown` 能软关机（否则记入已知限制）

**收尾**

- [ ] 截图与串口日志归档（命名带版本与用途）
- [ ] 不支持的项（如键盘型号不兼容、分辨率缺失）写进文档，不写成「大概能用」

---

## 11. 明确不支持（别浪费时间试）

- 纯 UEFI 无 CSM 的机器
- NVMe 作为系统盘或数据盘
- USB 键盘（无 BIOS 模拟时）、USB 鼠标、USB 存储
- 2.5G / 10G 网卡（清单外的芯片）
- 无线网卡（无无线驱动，也无 802.11 栈）
- 多核并行（SMP 未实现）
- 显卡 3D / 硬件加速（只有 VBE 线性帧缓冲与自研 X11 子集）
- USB 3.0 设备的实际数据传输（XHCI 只做枚举）
