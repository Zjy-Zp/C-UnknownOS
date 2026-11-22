# 调试手段

## 手段优先级

内核调试没有 `print` 之外的奢侈。按"信息量 / 成本"排序：

| 手段 | 成本 | 适用 |
|---|---|---|
| 串口日志 | 最低 | 一切阶段的首选 |
| QEMU monitor | 低 | 查看寄存器、内存、映射 |
| gdb 远程调试 | 中 | 精确定位崩溃现场 |
| QEMU 日志开关 | 中 | 观察中断、页表、设备行为 |
| 实体机验证 | 高 | 确认不依赖模拟器巧合 |

## 串口日志分级

```c
#define LOG_ERR(fmt, ...)  kprintf("[ERR] " fmt "\n", ##__VA_ARGS__)
#define LOG_WARN(fmt, ...) kprintf("[WRN] " fmt "\n", ##__VA_ARGS__)
#define LOG_INFO(fmt, ...) kprintf("[INF] " fmt "\n", ##__VA_ARGS__)
#define LOG_DBG(fmt, ...)  kprintf("[DBG] " fmt "\n", ##__VA_ARGS__)
```

早期阶段（串口未初始化前）只能用 VGA 文本模式写 `0xB8000`。建议保留一个 `early_print`，它不依赖任何子系统，是三重故障前唯一可用的输出。

## 崩溃现场导出

异常处理函数应打印完整现场：

```
=== EXCEPTION 14 (page fault) ===
CR2 = 0xC0001000   (出错地址)
ERR = 0x00000002   (写操作 + 页不存在)
EIP = 0x00101234
CS=0x0008 DS=0x0010 EFLAGS=0x00010046
EAX=... EBX=... ECX=... EDX=...
栈回溯：0x00101234 → 0x00101567 → 0x00102011
```

`ERR` 码的位含义在缺页时尤其重要：bit0=1 表示保护违规（页存在但权限不足），bit0=0 表示页不存在；bit1=1 表示写操作。

## QEMU 日志开关

```bash
# 记录所有中断与异常
qemu-system-i386 -d int -D qemu-int.log ...

# 记录页表翻译（非常啰嗦，只在定位映射问题时用）
qemu-system-i386 -d mmu -D qemu-mmu.log ...

# 记录 CPU 状态与指令流
qemu-system-i386 -d in_asm,cpu -D qemu-cpu.log ...
```

这些日志动辄几十 MB，只在定位特定问题时打开。

## gdb 实用命令

| 命令 | 用途 |
|---|---|
| `info registers` | 全寄存器 |
| `x/16i $eip` | 反汇编当前指令附近 |
| `x/16xb 0x1000` | 查看物理内存 |
| `p/x $cr2` | 缺页地址 |
| `monitor info mem` | 让 QEMU 打印当前映射 |
| `hbreak *0x100000` | 硬件断点（只读代码段） |
| `set architecture i8086` | 调试引导扇区时切换 |

## 排查顺序

遇到"启动后什么都没发生"：

1. QEMU 是否输出了 BIOS 信息 → 排除镜像格式问题
2. 引导扇区第一行是否能打印 → 排除 512 字节 / `0xAA55` 问题
3. 是否有任何串口输出 → 排除串口初始化问题
4. `-d int` 日志里最后的异常号是多少 → 直接定位到出错阶段
5. gdb 停在最后一条能执行的指令上，单步跨过去看崩在哪里
