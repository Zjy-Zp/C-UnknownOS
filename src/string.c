#include "common.h"

void *memset(void *dst, int c, u32 n)
{
    u8 *d = (u8 *)dst;
    while (n--) *d++ = (u8)c;
    return dst;
}

void *memcpy(void *dst, const void *src, u32 n)
{
    u8 *d = (u8 *)dst;
    const u8 *s = (const u8 *)src;
    while (n--) *d++ = *s++;
    return dst;
}

int memcmp(const void *a, const void *b, u32 n)
{
    const u8 *x = (const u8 *)a, *y = (const u8 *)b;
    while (n--) {
        if (*x != *y) return (int)*x - (int)*y;
        x++; y++;
    }
    return 0;
}

u32 strlen(const char *s)
{
    u32 n = 0;
    while (s[n]) n++;
    return n;
}

int strcmp(const char *a, const char *b)
{
    while (*a && *a == *b) { a++; b++; }
    return (int)(u8)*a - (int)(u8)*b;
}

int strncmp(const char *a, const char *b, u32 n)
{
    while (n && *a && *a == *b) { a++; b++; n--; }
    if (!n) return 0;
    return (int)(u8)*a - (int)(u8)*b;
}

char *strcpy(char *dst, const char *src)
{
    char *d = dst;
    while ((*d++ = *src++)) { }
    return dst;
}

char *strncpy(char *dst, const char *src, u32 n)
{
    u32 i = 0;
    for (; i < n && src[i]; i++) dst[i] = src[i];
    for (; i < n; i++) dst[i] = '\0';
    return dst;
}

char to_upper(char c)
{
    if (c >= 'a' && c <= 'z') return (char)(c - 'a' + 'A');
    return c;
}

/* 大小写不敏感比较（用于命令/文件名） */
int strnicmp(const char *a, const char *b, u32 n)
{
    while (n--) {
        char ca = to_upper(*a), cb = to_upper(*b);
        if (ca != cb) return (int)(u8)ca - (int)(u8)cb;
        if (!ca) return 0;
        a++; b++;
    }
    return 0;
}

int atoi(const char *s)
{
    int v = 0, neg = 0;
    while (is_space(*s)) s++;
    if (*s == '-') { neg = 1; s++; }
    while (*s >= '0' && *s <= '9') v = v * 10 + (*s++ - '0');
    return neg ? -v : v;
}

int is_space(char c)
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}
