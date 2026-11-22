# QEMU 无头验证

本项目的硬性约定：**任何改动都要能真实启动并看到结果**。不接受"编译通过即可"。本页给出无图形界面下的标准验证流程。

## 基础启动

```bash
qemu-system-i386 \
  -m 128M \
  -drive format=raw,file=cunos.img \
  -serial stdio \
  -display none \
  -no-reboot -no-shutdown
```

| 参数 | 作用 |
|---|---|
| `-m 128M` | 内存大小，建议固定，便于堆分配器行为可复现 |
| `-drive format=raw,file=...` | 以裸盘方式挂载镜像 |
| `-serial stdio` | 串口输出重定向到终端，无头环境的关键通道 |
| `-display none` | 关闭图形窗口 |
| `-no-reboot` | 三重故障时直接停住，而不是无限重启（否则你看不到现场） |
| `-no-shutdown` | 关机指令后保留 QEMU 进程，便于检查最终状态 |

::: warning 务必加 `-no-reboot`
三重故障默认行为是重启。如果不加这个参数，你会看到 QEMU 反复重启、日志刷屏，无法判断第一次故障发生在哪。加上它，QEMU 会停在故障点。
:::

## 用 `-nographic` 同时看串口与 VGA

```bash
qemu-system-i386 -drive format=raw,file=cunos.img -nographic
```

`-nographic` 会把串口和显示器都接到当前终端。切换方式：

- `Ctrl-A` 然后按 `c` —— 进入 QEMU monitor
- `Ctrl-A` 然后按 `x` —— 退出 QEMU

monitor 里可用的排查命令：

| 命令 | 用途 |
|---|---|
| `info registers` | 查看通用寄存器、CR0/CR2/CR3/CR4、EIP |
| `info mem` | 查看当前页表映射 |
| `info tlb` | 查看 TLB 内容 |
| `xp /16xb 0x100000` | 按字节查看物理内存 |
| `info block` | 确认镜像是否正确挂载 |

## 串口日志

建议内核尽早初始化 COM1（`0x3F8`），把日志全部走串口。好处是无头环境下也能完整观测，且不受 VGA 滚动覆盖影响。

一个最小实现的关键步骤：

1. 关闭中断：`outb(0x3F8 + 1, 0x00)`
2. 使能 DLAB：`outb(0x3F8 + 3, 0x80)`
3. 设分频（115200）：`outb(0x3F8 + 0, 0x03)` / `outb(0x3F8 + 1, 0x00)`
4. 8N1、关闭 DLAB：`outb(0x3F8 + 3, 0x03)`
5. 使能 FIFO：`outb(0x3F8 + 2, 0xC7)`
6. 输出字符前轮询 LSR（`0x3F8 + 5`）的 bit5（THR empty）

## 调试模式

```bash
# 终端 A：启动并暂停等待调试器
qemu-system-i386 -drive format=raw,file=cunos.img -s -S -display none -serial stdio

# 终端 B：连接
gdb-multiarch -ex 'target remote :1234' -ex 'set architecture i8086'
```

常用断点位置：

| 阶段 | 建议断点 |
|---|---|
| 引导扇区 | `break *0x7c00` |
| 进入保护模式后 | `break *0x100000`（内核加载地址） |
| 中断处理 | 在 IDT 表项填写的地址上下断 |

硬件断点（`hbreak`）在只读代码段上更可靠，`break` 依赖的内存写入在 ROM 段可能失败。

## 验证清单

- [ ] 串口打印出预期的启动横幅
- [ ] `info registers` 里 `CR0.PE=1`、`CR0.PG=1`（进入分页后）
- [ ] 无三重故障（QEMU 未停在 `-no-reboot` 的停机状态）
- [ ] 关键子系统的自检输出全部为 PASS

## 常见的"看起来成功了"陷阱

| 现象 | 真实原因 |
|---|---|
| 屏幕上有一行字，但后面全死 | VGA 文本模式仍可用，不代表中断/分页已正常工作 |
| 串口没有任何输出 | COM1 波特率配置错误，或 LSR 轮询写错导致死等 |
| QEMU 立刻退出 | 镜像末尾缺少 `0xAA55`，BIOS 判定不可引导 |
| 启动到一半重启 | 三重故障，去掉 `-no-reboot` 才能看到现场 |
