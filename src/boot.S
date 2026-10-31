/* ============================================================
 * cunos - boot.S
 * Multiboot 头部 + 引导入口，由 GRUB 加载后跳转到 _start
 * ============================================================ */

.set MB_MAGIC,    0x1BADB002          /* Multiboot 1 魔数 */
.set MB_FLAGS,    0x00000007          /* bit0 对齐 / bit1 内存信息 / bit2 请求显示模式 */
.set MB_CHECKSUM, -(MB_MAGIC + MB_FLAGS)

/* ---- Multiboot 头部：必须位于文件前 8KB 内 ---- */
.section .multiboot, "a"
.align 4
    .long MB_MAGIC
    .long MB_FLAGS
    .long MB_CHECKSUM
/* a.out kludge 五个字段：bit16 未置位时被忽略，但必须占位，
 * 否则 video 字段会落在 GRUB 期望的偏移(32)之外，导致解析错位 */
    .long 0                     /* header_addr  */
    .long 0                     /* load_addr    */
    .long 0                     /* load_end_addr*/
    .long 0                     /* bss_end_addr */
    .long 0                     /* entry_addr   */
/* video 模式请求：mode_type = 0 表示线性图形 */
    .long 0                     /* mode_type */
    .long 1024                  /* width  */
    .long 768                   /* height */
    .long 32                    /* depth  */

/* ---- 内核栈：16KB，放在 .bss 中不占镜像体积 ---- */
.section .bss
.align 16
stack_bottom:
    .skip 16384
stack_top:

/* ---- 代码段 ---- */
.section .text
.global _start
.type _start, @function
_start:
    cli                         /* 关中断，此时 IDT 尚未建立 */
    mov $stack_top, %esp        /* 建立栈 */
    mov %esp, %ebp

    push %ebx                   /* ebx = multiboot_info 指针，作为参数传给 C */
    push %eax                   /* eax = multiboot 魔数 */
    call kernel_main

1:  cli                         /* 理论上不会返回；返回则死循环挂住 */
    hlt
    jmp 1b
.size _start, . - _start
