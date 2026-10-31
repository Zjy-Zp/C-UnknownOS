---
AIGC:
    Label: "1"
    ContentProducer: 001191440300708461136T1XGW3
    ProduceID: 0a49692c9e8a806948f690d4834b2457_61f5eff8d52111f180d5525400248c00
    ReservedCode1: LKNwjmWflEKKUQZmwXkplKMuFOr04t+YFR0SVJONDEMGummhEzF6kq/7xthfYFwt9PEXqwp64lF58+QJNT7idN9QCuRvzCWliTUi+DgR9DDv+/6o1YHtWl3nglYSc0wke0168QyZIxKpyzeHRSfuNxae2X9XE9dD3b7B7Ho2xzwiVSSby6ZRL0Foh9U=
    ContentPropagator: 001191440300708461136T1XGW3
    PropagateID: 0a49692c9e8a806948f690d4834b2457_61f5eff8d52111f180d5525400248c00
    ReservedCode2: LKNwjmWflEKKUQZmwXkplKMuFOr04t+YFR0SVJONDEMGummhEzF6kq/7xthfYFwt9PEXqwp64lF58+QJNT7idN9QCuRvzCWliTUi+DgR9DDv+/6o1YHtWl3nglYSc0wke0168QyZIxKpyzeHRSfuNxae2X9XE9dD3b7B7Ho2xzwiVSSby6ZRL0Foh9U=
---

# cunos

一个用 C 语言写的极简 32 位操作系统内核，由 GRUB (Multiboot 1) 引导。
开机后清屏并在左上角显示绿色的 `hi`，随后关闭中断、执行 `hlt` 挂住 CPU，
机器保持运行不关机、不重启。

## 目录结构

```
C:\cunos
├─ src
│   ├─ boot.S      Multiboot 头部 + 汇编入口，建立 16KB 栈后调用 kernel_main
│   ├─ kernel.c    内核主体：直接写 VGA 文本显存 0xB8000 输出 "hi"，然后 cli/hlt 死循环
│   ├─ linker.ld   链接脚本，内核加载到 1MB
│   └─ grub.cfg    GRUB 菜单配置，timeout=0 直接启动
├─ build.sh        WSL/Linux 下的构建脚本，产物 cunos.iso
├─ Makefile        make 构建（等价于 build.sh）
├─ cunos.iso       构建产物：可引导光盘镜像
└─ README.md
```

## 构建（WSL Debian 内执行）

```bash
cd /mnt/c/cunos
bash build.sh
```

依赖（Debian/Ubuntu）：

```bash
sudo apt-get install -y build-essential nasm grub-pc-bin grub-common xorriso mtools
```

## 运行（Windows 上的 QEMU）

```powershell
C:\EXES\qemu\qemu-system-i386.exe -cdrom C:\cunos\cunos.iso -m 128
```

预期：GRUB 一闪而过后黑屏，左上角出现绿色 `hi`，然后画面静止（CPU 停在 hlt 指令上）。
按 `Ctrl+Alt+2` 切到 QEMU monitor，输入 `quit` 退出。

## 实现要点

- **Multiboot 头部**：`boot.S` 开头三个 `.long`（魔数 `0x1BADB002`、标志 `0x3`、校验和）让 GRUB 识别这是一个可加载内核。
- **显存直写**：文本模式下 `0xB8000` 起每字符 2 字节（ASCII + 属性），`ATTR(0xA, 0x0)` 为黑底亮绿。
- **不关机**：`kernel_main` 末尾 `for(;;)` 内执行 `cli; hlt`，关中断后 CPU 停机等待，永不到达关机流程。
- **无 libc**：全程 `-ffreestanding -nostdlib`，自行定义 `vga_*` 系列函数输出文本。
*（内容由AI生成，仅供参考）*
