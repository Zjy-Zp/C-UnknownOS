# 术语表

| 术语 | 全称 | 含义 |
|---|---|---|
| GDT | Global Descriptor Table | 全局描述符表，定义内存段及其权限 |
| IDT | Interrupt Descriptor Table | 中断描述符表，256 个中断/异常入口 |
| LDT | Local Descriptor Table | 局部描述符表，通常本项目不用 |
| TSS | Task State Segment | 任务状态段，特权级切换时用于取栈 |
| PDE / PTE | Page Directory / Table Entry | 页目录项 / 页表项 |
| CR0 / CR2 / CR3 / CR4 | Control Register | 控制寄存器；PG 位在 CR0，缺页地址在 CR2，页目录基址在 CR3 |
| VBE | VESA BIOS Extension | 实模式下设置显示模式的接口 |
| MMIO | Memory-Mapped I/O | 把设备寄存器映射到内存地址空间 |
| BAR | Base Address Register | PCI 设备在配置空间里声明资源基址 |
| DMA | Direct Memory Access | 设备直接读写内存，绕过 CPU |
| IRQ | Interrupt Request | 设备中断请求线 |
| EOI | End Of Interrupt | 中断服务结束信号，不发则不再触发 |
| AHCI | Advanced Host Controller Interface | SATA 控制器标准接口 |
| FIS | Frame Information Structure | AHCI 中主机与设备交换的数据帧 |
| TRB | Transfer Request Block | XHCI 中描述一次传输的单元 |
| DCBAA | Device Context Base Address Array | XHCI 的设备上下文基址数组 |
| VFS | Virtual File System | 统一文件接口抽象层 |
| inode | index node | 文件元数据记录 |
| LBA | Logical Block Address | 块设备逻辑块编号 |
| FIFO | First In First Out | 先进先出队列 |
| PIC / APIC | Programmable Interrupt Controller | 可编程中断控制器 |
| freestanding | — | 不依赖宿主运行时的编译环境 |
| 三重故障 | Triple Fault | 处理异常时又异常且双重故障也失败，CPU 直接复位 |
