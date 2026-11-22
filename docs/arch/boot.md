# 引导与进入保护模式

从 BIOS 交权的那一刻起，CPU 处于 16 位实模式：只能寻址 1 MiB、没有内存保护、段基址需要左移 4 位。内核工作的第一步是把这颗 CPU 安全地带进 32 位保护模式。

## 完整流程

| 阶段 | 动作 | 失败表现 |
|---|---|---|
| 1 | 引导扇区被 BIOS 加载到 `0x7C00` | 无输出 / `not a bootable disk` |
| 2 | 关中断 `cli`，初始化段寄存器与栈 | 后续硬件中断踩栈 |
| 3 | 打开 A20 地址线 | 访问 1 MiB 以上地址时回绕 |
| 4 | 加载 GDT（`lgdt`） | 远跳转崩溃 |
| 5 | 置位 `CR0.PE` | —— |
| 6 | 远跳转刷新 CS，进入 32 位段 | 三重故障 |
| 7 | 重设数据段寄存器、建立新栈 | 数据访问异常 |
| 8 | 跳入 C 语言内核入口 | —— |

## 关键代码片段

**打开 A20**（Fast A20 方式）：

```nasm
in  al, 0x92
or  al, 0x02
and al, 0xFE
out 0x92, al
```

**GDT 加载**：

```nasm
lgdt [gdtr]

mov eax, cr0
or  eax, 0x1        ; CR0.PE = 1
mov cr0, eax

jmp 0x08:protected_mode   ; 远跳转，刷新 CS
```

`0x08` 是代码段选择子：`index=1`、`TI=0`、`RPL=0`。

**进入 32 位后**：

```nasm
[bits 32]
protected_mode:
    mov ax, 0x10     ; 数据段选择子
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x7C00
    call kernel_main
```

## 必须理解的三个细节

**1. 段选择子不只是索引**
它的低 2 位是 RPL（请求特权级），第 2 位是 TI（0 表示查 GDT，1 表示查 LDT）。写 `0x08` 而不是 `0x8 | 3` 才能保证进入 ring 0。

**2. 远跳转不能省**
`mov cr0, eax` 之后 CPU 已经处于保护模式，但 CS 里还残留着实模式的段基址缓存。必须用 `jmp far` 重新加载 CS 描述符缓存，否则下一条指令的取指地址是错的。这是新手最常踩的坑，表现为加完 `CR0.PE` 立刻三重故障。

**3. A20 不打开，一切白搭**
A20 门控关闭时，物理地址第 20 位被强制为 0，`0x100000` 会回绕到 `0x000000`。内核加载到 1 MiB 处会覆盖低端内存，症状是"加载成功但执行的是垃圾"。

## 验证方法

在 `kernel_main` 第一行打印：

```c
void kernel_main(void) {
    serial_init();
    serial_write("cunos: protected mode ok\n");
    for (;;) __asm__ volatile("hlt");
}
```

QEMU 里能看到这行输出，说明引导链全部正确。看不到则按下面顺序排查：

1. 是否输出了引导扇区的早期信息 → 判断 BIOS 是否加载成功
2. 是否在 `lgdt` 之后崩溃 → GDT 描述符结构或 `gdtr` 基址错误
3. 是否在远跳转后崩溃 → 段选择子或 GDT 表项权限位错误

::: tip 用 gdb 定点验证
在 `break *0x7c00` 处停下，单步执行 `mov cr0, eax` 之后立刻 `info registers cr0`，确认 PE 位已置位；然后在 `jmp far` 目标地址下断，能命中就说明 CS 已正确刷新。
:::

## 下一步

保护模式稳定后，接 [分页与内存管理](./memory)。
