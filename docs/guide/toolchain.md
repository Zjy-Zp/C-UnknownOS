# 工具链与交叉编译

## 为什么必须交叉编译

内核代码运行在 **freestanding** 环境：没有 libc、没有动态链接、没有操作系统提供的启动代码。宿主机自带的 `gcc` 默认会链接 glibc 的 crt 对象，并假设存在完整的运行时，这些假设在内核里全部不成立。

正确做法是构建 `i686-elf` 目标工具链：目标平台是"裸机 i386"，编译器知道不能依赖宿主机运行时。

## 编译标志

```makefile
CC      := i686-elf-gcc
CFLAGS  := -std=gnu11 -ffreestanding -fno-stack-protector \
           -fno-pic -fno-pie -Wall -Wextra -O2 \
           -m32 -mno-sse -mno-mmx -mno-80387
LDFLAGS := -m elf_i386 -T linker.ld -nostdlib
```

| 标志 | 为什么需要 |
|---|---|
| `-ffreestanding` | 声明不依赖标准库宿主环境 |
| `-fno-stack-protector` | 栈保护依赖 `__stack_chk_fail`，内核里没有 |
| `-fno-pic -fno-pie` | 内核按固定地址加载，位置无关代码会引入 GOT/PLT |
| `-m32` | 强制 32 位输出 |
| `-mno-sse -mno-mmx -mno-80387` | 早期内核未保存 FPU/SSE 上下文，避免编译器悄悄生成相关指令 |
| `-nostdlib` | 不链接任何默认库与启动对象 |

::: danger 最常见的"启动即三重故障"
如果编译时漏掉 `-mno-sse`，编译器可能在纯 C 函数里生成 SSE 指令；而早于 CR4.OSFXSR 配置的阶段执行 SSE 指令会直接触发异常，表现为 QEMU 重启或黑屏。这类问题用 gdb 才能看清，务必提前用对标志。
:::

## 链接脚本

控制内存布局，把内核放在引导加载器约定的位置：

```ld
ENTRY(_start)

SECTIONS
{
    . = 1M;

    .text : ALIGN(4K)
    {
        *(.multiboot)
        *(.text*)
    }

    .rodata : ALIGN(4K) { *(.rodata*) }
    .data   : ALIGN(4K) { *(.data*) }

    .bss : ALIGN(4K)
    {
        *(COMMON)
        *(.bss*)
    }

    _kernel_end = .;
}
```

要点：

- `. = 1M` —— 内核加载到物理地址 1 MiB，避开低端内存里的 BIOS 数据区和 VGA 缓冲区。
- `.bss` 不占文件空间，但必须由启动代码手工清零，否则里面是随机垃圾。
- `_kernel_end` 供内存管理器知道哪些物理页可以安全分配。

## 验证工具链是否装对

```bash
i686-elf-gcc -v 2>&1 | grep Target
# 期望输出：Target: i686-elf
```

## 备选方案

不想自己编译工具链时：

| 方案 | 代价 |
|---|---|
| 发行版打包的 `i686-elf-gcc`（部分发行版有） | 版本可能偏旧 |
| 宿主 `gcc -m32` + 严格 freestanding 标志 | 可行，但对齐/ABI 细节需自行确认 |
| 官方 osdev 交叉工具链脚本 | 编译一次约 30～60 分钟 |

::: tip
如果坚持用宿主 gcc，务必在 CI 里加一条 `objdump -d` 检查：搜索是否出现了 `%xmm` / `%st(` 寄存器，出现即说明标志没生效。
:::
