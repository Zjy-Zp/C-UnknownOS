#ifndef COMMON_H
#define COMMON_H

typedef unsigned char      u8;
typedef unsigned short     u16;
typedef unsigned int       u32;
typedef unsigned long long u64;

typedef signed char        i8;
typedef signed short       i16;
typedef signed int         i32;

#define NULL ((void *)0)
#define TRUE 1
#define FALSE 0

/* 极简 varargs：直接用编译器内建，避开标准库 */
typedef __builtin_va_list va_list;
#define va_start(v, l) __builtin_va_start(v, l)
#define va_arg(v, t)   __builtin_va_arg(v, t)
#define va_end(v)      __builtin_va_end(v)

void *memset(void *dst, int c, u32 n);
void *memcpy(void *dst, const void *src, u32 n);
int   memcmp(const void *a, const void *b, u32 n);
u32   strlen(const char *s);
int   strcmp(const char *a, const char *b);
int   strncmp(const char *a, const char *b, u32 n);
char *strcpy(char *dst, const char *src);
char *strncpy(char *dst, const char *src, u32 n);
int   strnicmp(const char *a, const char *b, u32 n);
char  to_upper(char c);
int   atoi(const char *s);
int   is_space(char c);

/* 简易堆：从内核结尾向后 bump 分配 */
void *kmalloc(u32 size);
u32   kheap_used(void);

/* 端口 I/O */
static inline void outb(u16 port, u8 v)
{
    __asm__ volatile("outb %0, %1" : : "a"(v), "Nd"(port));
}
static inline u8 inb(u16 port)
{
    u8 r;
    __asm__ volatile("inb %1, %0" : "=a"(r) : "Nd"(port));
    return r;
}
static inline void outw(u16 port, u16 v)
{
    __asm__ volatile("outw %0, %1" : : "a"(v), "Nd"(port));
}
static inline u16 inw(u16 port)
{
    u16 r;
    __asm__ volatile("inw %1, %0" : "=a"(r) : "Nd"(port));
    return r;
}
static inline void outl(u16 port, u32 v)
{
    __asm__ volatile("outl %0, %1" : : "a"(v), "Nd"(port));
}
static inline u32 inl(u16 port)
{
    u32 r;
    __asm__ volatile("inl %1, %0" : "=a"(r) : "Nd"(port));
    return r;
}
static inline void outsw(u16 port, const void *buf, u32 count)
{
    __asm__ volatile("rep outsw" : "+S"(buf), "+c"(count) : "d"(port));
}
static inline void insw(u16 port, void *buf, u32 count)
{
    __asm__ volatile("rep insw" : "+D"(buf), "+c"(count) : "d"(port));
}
static inline void io_wait(void) { outb(0x80, 0); }
static inline void cli(void) { __asm__ volatile("cli"); }
static inline void sti(void) { __asm__ volatile("sti"); }
static inline void hlt(void) { __asm__ volatile("hlt"); }
static inline void cpu_pause(void) { __asm__ volatile("pause"); }

static inline void serial_init(void)
{
    outb(0x3F8 + 1, 0x00);
    outb(0x3F8 + 3, 0x80);
    outb(0x3F8 + 0, 0x01);
    outb(0x3F8 + 1, 0x00);
    outb(0x3F8 + 3, 0x03);
    outb(0x3F8 + 2, 0xC7);
    outb(0x3F8 + 4, 0x0B);
}
static inline void serial_putc(char c)
{
    for (int i = 0; i < 100000; i++) {
        if (inb(0x3F8 + 5) & 0x20) break;
        cpu_pause();
    }
    outb(0x3F8, (u8)c);
}
static inline void serial_puts(const char *s) { while (*s) serial_putc(*s++); }
static inline void serial_hex(u32 v)
{
    char b[11]; b[0]='0'; b[1]='x';
    for (int i = 0; i < 8; i++) {
        u32 d = (v >> (28 - i * 4)) & 0xF;
        b[2 + i] = (char)(d < 10 ? '0' + d : 'a' + d - 10);
    }
    b[10] = '\0';
    serial_puts(b);
}
static inline void serial_dec(int v)
{
    char b[12]; int n = 0;
    if (v < 0) { serial_putc('-'); v = -v; }
    if (!v) { serial_putc('0'); return; }
    while (v > 0) { b[n++] = (char)('0' + v % 10); v /= 10; }
    while (n) serial_putc(b[--n]);
}

#endif
