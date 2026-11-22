# 五分钟跑起来

目标：用一个最小的引导 + 内核镜像，在 QEMU 里看到你自己的输出。这是后面一切工作的地基。

## 1. 安装依赖

WSL Debian 下：

```bash
sudo apt update
sudo apt install -y build-essential nasm qemu-system-x86 gdb-multiarch xorriso mtools
```

| 组件 | 作用 |
|---|---|
| `build-essential` | gcc / ld / make |
| `nasm` | 汇编器 |
| `qemu-system-x86` | 运行 i386 镜像 |
| `gdb-multiarch` | 远程调试 |
| `xorriso` `mtools` | 生成 ISO 镜像 |

## 2. 目录结构

```
cunos/
├── boot/          # 引导相关（引导扇区 / 多阶段加载器）
├── kernel/        # 内核主体
├── drivers/       # 驱动
├── fs/            # 文件系统与 VFS
├── net/           # 网络栈
├── lib/           # 内核内通用库
├── include/       # 公共头文件
├── linker.ld      # 链接脚本
└── Makefile
```

## 3. 最小可启动镜像

**引导扇区**（`boot/boot.asm`），NASM 语法，必须以 512 字节、末尾 `0xAA55` 结尾：

```nasm
[bits 16]
[org 0x7c00]

start:
    mov si, msg
.next:
    lodsb
    or al, al
    jz halt
    mov ah, 0x0e
    int 0x10
    jmp .next

halt:
    cli
    hlt
    jmp halt

msg db 'cunos boot ok', 0

times 510-($-$$) db 0
dw 0xaa55
```

编译与运行：

```bash
nasm -f bin boot/boot.asm -o boot.bin
qemu-system-i386 -drive format=raw,file=boot.bin -nographic
```

串口屏（或 `-nographic` 的终端）里出现 `cunos boot ok`，说明引导链已经通了。

## 4. 验证清单

进入下一步之前，确认这三件事都为真：

- [ ] `boot.bin` 大小恰好 512 字节
- [ ] 最后两字节是 `0x55 0xaa`（小端存放，文件里看到 `55 aa`）
- [ ] QEMU 启动后没有任何 `Boot failed` / `not a bootable disk` 报错

用 `xxd` 自查：

```bash
xxd boot.bin | tail -2
```

## 5. 下一步

引导通了之后，接着做 [进入保护模式](../arch/boot) —— 加载 GDT、置位 `CR0.PE`、远跳转刷新 CS。这是从 16 位实模式跨进 32 位世界的第一道门。

::: tip 关于"五分钟"
如果你的机器上工具链还没装，第一次大概要花 20 分钟下载。真正的编码动作本身，确实只有五分钟。
:::
