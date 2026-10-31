#ifndef VGA_H
#define VGA_H

#include "common.h"

/* 16 色调色板编号（与 VGA 文本模式兼容） */
enum {
    VGA_BLACK = 0, VGA_BLUE, VGA_GREEN, VGA_CYAN,
    VGA_RED, VGA_MAGENTA, VGA_BROWN, VGA_LIGHT_GRAY,
    VGA_DARK_GRAY, VGA_LIGHT_BLUE, VGA_LIGHT_GREEN, VGA_LIGHT_CYAN,
    VGA_LIGHT_RED, VGA_LIGHT_MAGENTA, VGA_YELLOW, VGA_WHITE
};

#define VGA_ATTR(fg, bg) ((u8)(((bg) << 4) | (fg)))

/* mb_info：Multiboot 信息结构物理地址，用于取得 VBE 帧缓冲 */
void vga_init(u32 mb_info);

int  vga_fb_active(void);      /* 1 = 线性帧缓冲模式，0 = VGA 文本模式 */
int  disp_cols(void);
int  disp_rows(void);
u32  vga_fb_width(void);
u32  vga_fb_height(void);
u32  vga_fb_bpp(void);

void vga_clear(void);
void vga_set_color(u8 fg, u8 bg);
u8   vga_get_attr(void);
void vga_putc(char c);
void vga_puts(const char *s);
void vga_printf(const char *fmt, ...);
void vga_gotoxy(int row, int col);
int  vga_row(void);
int  vga_col(void);
void vga_fill_line(int row, u8 attr, char fill);
void vga_color_bar(void);      /* 打印 16 色调色板示例 */
void vga_cursor_enable(int on);

#endif
