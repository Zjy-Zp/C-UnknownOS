# 构建命令速查

## 常用命令

| 命令 | 作用 |
|---|---|
| `make` | 构建内核镜像 |
| `make clean` | 清理产物 |
| `make run` | QEMU 启动 |
| `make debug` | 带 `-s -S` 启动，等待 gdb |
| `make iso` | 生成 ISO |
| `make disasm` | 反汇编内核，检查是否引入浮点/SSE |

## 手工构建链

```bash
nasm -f bin boot/boot.asm -o build/boot.bin
i686-elf-gcc -c kernel/main.c -o build/main.o $(CFLAGS)
i686-elf-ld -T linker.ld -o build/kernel.elf build/*.o
objcopy -O binary build/kernel.elf build/kernel.bin
```

## QEMU 启动模板

```bash
# 最简无头
qemu-system-i386 -m 128M -drive format=raw,file=build/cunos.img \
  -serial stdio -display none -no-reboot -no-shutdown

# 调试
qemu-system-i386 -m 128M -drive format=raw,file=build/cunos.img \
  -serial stdio -display none -s -S

# 带网络抓包
qemu-system-i386 -m 128M -drive format=raw,file=build/cunos.img \
  -netdev user,id=n0 -device e1000,netdev=n0 \
  -object filter-dump,id=f1,netdev=n0,file=nettrace.pcap
```

## 检查产物

```bash
# 引导扇区必须是 512 字节且以 55 aa 结尾
stat -c %s build/boot.bin
xxd build/boot.bin | tail -1

# 检查是否混入 SSE/FPU 指令（应无匹配）
objdump -d build/kernel.elf | grep -E '%xmm|%st\(|%ymm'

# 查看段布局
i686-elf-objdump -h build/kernel.elf
```
