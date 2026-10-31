#include "vga.h"
#include "font8x16.h"

#define TEXT_MEM   ((volatile u16 *)0xB8000)
#define MAX_COLS   128
#define MAX_ROWS   50
#define CHAR_W     8
#define CHAR_H     16

/* 主题：黑底，浅灰正文，青绿/琥珀/珊瑚色作强调色 */
#define THEME_FG   VGA_LIGHT_GRAY
#define THEME_BG   VGA_BLACK

/* 自定义调色板（在标准 VGA 色基础上略作调整，更柔和） */
static const u32 palette16[16] = {
    0x0A0E14, /* 0  black       深墨底    */
    0x1F5FA8, /* 1  blue        靛蓝      */
    0x2E9E5B, /* 2  green       森绿      */
    0x2FA8A0, /* 3  cyan        松石青    */
    0xC0443E, /* 4  red         砖红      */
    0xA3569B, /* 5  magenta     紫罗兰    */
    0xB97F2E, /* 6  brown       赭黄      */
    0xC9D3DF, /* 7  light gray  雾灰      */
    0x5A6874, /* 8  dark gray   石墨      */
    0x5B9BE8, /* 9  light blue  天蓝      */
    0x3FD08A, /* 10 light green 翡翠绿    */
    0x40D9D0, /* 11 light cyan  水青      */
    0xF06A6A, /* 12 light red   珊瑚红    */
    0xE07BD8, /* 13 light magenta 品红    */
    0xF0C24B, /* 14 yellow      琥珀      */
    0xF2F6FA  /* 15 white       雪白      */
};

static u8  sbuf[MAX_COLS * MAX_ROWS];   /* 字符影子缓冲 */
static u8  abuf[MAX_COLS * MAX_ROWS];   /* 属性影子缓冲 */
static int cols = 80, rows = 25;
static u8  attr = VGA_ATTR(THEME_FG, THEME_BG);
static int row = 0, col = 0;
static int cursor_on = 1;
static int cur_drawn_x = -1, cur_drawn_y = -1;

/* framebuffer 后端 */
static u8 *fb = 0;
static u32 fb_pitch = 0, fb_w = 0, fb_h = 0;
static u32 fb_off_x = 0, fb_off_y = 0;
static u8  fb_bpp = 0;

static inline u16 text_cell(char c, u8 a) { return (u16)((u8)c | ((u16)a << 8)); }

/* ---------------- framebuffer 绘制 ---------------- */

static inline void put_px(u32 x, u32 y, u32 rgb)
{
    if (!fb || x >= fb_w || y >= fb_h) return;
    u8 *p = fb + (u32)y * fb_pitch + (u32)x * (fb_bpp >> 3);
    if (fb_bpp == 32) {
        p[0] = (u8)(rgb & 0xFF);
        p[1] = (u8)((rgb >> 8) & 0xFF);
        p[2] = (u8)((rgb >> 16) & 0xFF);
        p[3] = 0;
    } else if (fb_bpp == 24) {
        p[0] = (u8)(rgb & 0xFF);
        p[1] = (u8)((rgb >> 8) & 0xFF);
        p[2] = (u8)((rgb >> 16) & 0xFF);
    } else if (fb_bpp == 16) {
        u16 v = (u16)(((rgb >> 19) & 0x1F) << 11 | ((rgb >> 10) & 0x3F) << 5 | ((rgb >> 3) & 0x1F));
        *(u16 *)p = v;
    }
}

static void fb_fill_rect(u32 x, u32 y, u32 w, u32 h, u32 rgb)
{
    for (u32 j = 0; j < h; j++)
        for (u32 i = 0; i < w; i++)
            put_px(x + i, y + j, rgb);
}

static void fb_draw_cell(int cx, int cy, char ch, u8 a)
{
    if (!fb) return;
    u32 fg = palette16[a & 0x0F];
    u32 bg = palette16[(a >> 4) & 0x0F];
    u32 px = fb_off_x + (u32)cx * CHAR_W;
    u32 py = fb_off_y + (u32)cy * CHAR_H;
    const u8 *glyph = 0;
    if (ch >= 32 && ch < 127) glyph = font8x16[(int)ch - 32];

    for (u32 y = 0; y < CHAR_H; y++) {
        u8 bits = glyph ? glyph[y] : 0;
        for (u32 x = 0; x < CHAR_W; x++)
            put_px(px + x, py + y, (bits & (0x80 >> x)) ? fg : bg);
    }
}

static void fb_erase_cursor(void)
{
    if (!fb || cur_drawn_x < 0) return;
    int i = cur_drawn_y * cols + cur_drawn_x;
    fb_draw_cell(cur_drawn_x, cur_drawn_y, (char)sbuf[i], abuf[i]);
    cur_drawn_x = -1;
}

static void fb_draw_cursor(void)
{
    if (!fb || !cursor_on) return;
    u32 px = fb_off_x + (u32)col * CHAR_W;
    u32 py = fb_off_y + (u32)row * CHAR_H + CHAR_H - 2;
    u32 c = palette16[abuf[row * cols + col] & 0x0F];
    fb_fill_rect(px, py, CHAR_W, 2, c);
    cur_drawn_x = col;
    cur_drawn_y = row;
}

static void fb_redraw_all(void)
{
    if (!fb) return;
    fb_erase_cursor();
    for (int r = 0; r < rows; r++)
        for (int c = 0; c < cols; c++)
            fb_draw_cell(c, r, (char)sbuf[r * cols + c], abuf[r * cols + c]);
}

/* ---------------- 后端选择 ---------------- */

int vga_fb_active(void) { return fb != 0; }
int disp_cols(void) { return cols; }
int disp_rows(void) { return rows; }
u32 vga_fb_width(void) { return fb_w; }
u32 vga_fb_height(void) { return fb_h; }
u32 vga_fb_bpp(void) { return fb_bpp; }

void vga_cursor_enable(int on) { cursor_on = on; if (!on) fb_erase_cursor(); }

void vga_init(u32 mb_info)
{
    attr = VGA_ATTR(THEME_FG, THEME_BG);
    row = col = 0;

    /* Multiboot 信息：flags(0) ... framebuffer_addr(88) */
    if (mb_info) {
        u8 *m = (u8 *)(u32)mb_info;
        u32 flags = *(u32 *)m;
        if (flags & (1u << 12)) {
            u64 addr = *(u64 *)(m + 88);
            u32 pitch = *(u32 *)(m + 96);
            u32 width = *(u32 *)(m + 100);
            u32 height = *(u32 *)(m + 104);
            u8  bpp = *(u8 *)(m + 108);
            if (addr && width && height && (bpp == 16 || bpp == 24 || bpp == 32)) {
                fb = (u8 *)(u32)addr;
                fb_pitch = pitch;
                fb_w = width;
                fb_h = height;
                fb_bpp = bpp;
            }
        }
    }

    if (fb) {
        cols = (int)(fb_w / CHAR_W);
        rows = (int)(fb_h / CHAR_H);
        if (cols > MAX_COLS) cols = MAX_COLS;
        if (rows > MAX_ROWS) rows = MAX_ROWS;
        fb_off_x = (fb_w - (u32)cols * CHAR_W) / 2;
        fb_off_y = (fb_h - (u32)rows * CHAR_H) / 2;
    } else {
        cols = 80;
        rows = 25;
    }
}

/* ---------------- 通用输出 ---------------- */

void vga_set_color(u8 fg, u8 bg) { attr = VGA_ATTR(fg, bg); }
u8   vga_get_attr(void) { return attr; }

void vga_fill_line(int r, u8 a, char fill)
{
    if (r < 0 || r >= rows) return;
    for (int c = 0; c < cols; c++) {
        sbuf[r * cols + c] = (u8)fill;
        abuf[r * cols + c] = a;
        if (fb) fb_draw_cell(c, r, fill, a);
        else TEXT_MEM[r * 80 + c] = text_cell(fill, a);
    }
}

void vga_clear(void)
{
    u8 blank = VGA_ATTR(VGA_BLACK, THEME_BG);
    fb_erase_cursor();
    for (int r = 0; r < rows; r++) {
        for (int c = 0; c < cols; c++) {
            sbuf[r * cols + c] = ' ';
            abuf[r * cols + c] = blank;
        }
    }
    if (fb) {
        fb_fill_rect(0, 0, fb_w, fb_h, palette16[THEME_BG]);
        fb_redraw_all();
    } else {
        for (int i = 0; i < 80 * 25; i++) TEXT_MEM[i] = text_cell(' ', blank);
    }
    row = col = 0;
    attr = VGA_ATTR(THEME_FG, THEME_BG);
}

static void scroll_up(void)
{
    fb_erase_cursor();
    for (int r = 1; r < rows; r++)
        for (int c = 0; c < cols; c++) {
            sbuf[(r - 1) * cols + c] = sbuf[r * cols + c];
            abuf[(r - 1) * cols + c] = abuf[r * cols + c];
        }
    for (int c = 0; c < cols; c++) {
        sbuf[(rows - 1) * cols + c] = ' ';
        abuf[(rows - 1) * cols + c] = VGA_ATTR(VGA_BLACK, THEME_BG);
    }
    row = rows - 1;

    if (fb) {
        fb_redraw_all();
    } else {
        for (int r = 1; r < 25; r++)
            for (int c = 0; c < 80; c++)
                TEXT_MEM[(r - 1) * 80 + c] = TEXT_MEM[r * 80 + c];
        for (int c = 0; c < 80; c++)
            TEXT_MEM[24 * 80 + c] = text_cell(' ', VGA_ATTR(VGA_BLACK, THEME_BG));
    }
}

void vga_gotoxy(int r, int c)
{
    fb_erase_cursor();
    if (r >= 0 && r < rows) row = r;
    if (c >= 0 && c < cols) col = c;
    if (fb) fb_draw_cursor();
}

int vga_row(void) { return row; }
int vga_col(void) { return col; }

static void put_char_at(int r, int c, char ch, u8 a)
{
    int i = r * cols + c;
    sbuf[i] = (u8)ch;
    abuf[i] = a;
    if (fb) fb_draw_cell(c, r, ch, a);
    else TEXT_MEM[r * 80 + c] = text_cell(ch, a);
}

void vga_putc(char c)
{
    if (fb) fb_erase_cursor();

    if (c == '\n') {
        col = 0;
        row++;
    } else if (c == '\r') {
        col = 0;
    } else if (c == '\b') {
        if (col > 0) {
            col--;
            put_char_at(row, col, ' ', attr);
        } else if (row > 0) {
            row--;
            col = cols - 1;
            put_char_at(row, col, ' ', attr);
        }
    } else if (c == '\t') {
        int next = (col + 4) & ~3;
        while (col < next && col < cols) { put_char_at(row, col, ' ', attr); col++; }
        if (col >= cols) { col = 0; row++; }
    } else {
        put_char_at(row, col, c, attr);
        col++;
        if (col >= cols) { col = 0; row++; }
    }

    if (row >= rows) scroll_up();
    if (fb) fb_draw_cursor();
}

void vga_puts(const char *s)
{
    while (*s) vga_putc(*s++);
}

static void put_uint(u32 v, int base, int width, char pad)
{
    char tmp[34];
    int n = 0;
    if (v == 0) tmp[n++] = '0';
    while (v) {
        u32 d = v % (u32)base;
        tmp[n++] = (char)(d < 10 ? '0' + d : 'a' + d - 10);
        v /= (u32)base;
    }
    while (n < width) tmp[n++] = pad;
    while (n--) vga_putc(tmp[n]);
}

static void put_int(int v, int width, char pad)
{
    if (v < 0) {
        vga_putc('-');
        put_uint((u32)(-v), 10, width - 1, pad);
    } else {
        put_uint((u32)v, 10, width, pad);
    }
}

void vga_printf(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    while (*fmt) {
        if (*fmt != '%') { vga_putc(*fmt++); continue; }
        fmt++;
        char pad = ' ';
        int width = 0;
        if (*fmt == '0') { pad = '0'; fmt++; }
        while (*fmt >= '0' && *fmt <= '9') width = width * 10 + (*fmt++ - '0');

        switch (*fmt) {
        case 's': vga_puts(va_arg(ap, const char *)); break;
        case 'c': vga_putc((char)va_arg(ap, int)); break;
        case 'd': put_int(va_arg(ap, int), width, pad); break;
        case 'u': put_uint(va_arg(ap, u32), 10, width, pad); break;
        case 'x': put_uint(va_arg(ap, u32), 16, width, pad); break;
        case 'p': vga_puts("0x"); put_uint((u32)va_arg(ap, void *), 16, 8, '0'); break;
        case '%': vga_putc('%'); break;
        case '\0': continue;
        default:  vga_putc('%'); vga_putc(*fmt); break;
        }
        fmt++;
    }
    va_end(ap);
}

void vga_color_bar(void)
{
    static const char *nm[16] = {
        "BLK", "BLU", "GRN", "CYN", "RED", "MAG", "BRN", "GRY",
        "DGY", "LBL", "LGN", "LCY", "LRD", "LMG", "YEL", "WHT"
    };
    for (int i = 0; i < 16; i++) {
        vga_set_color((u8)i, VGA_BLACK);
        vga_printf("%s", nm[i]);
        vga_set_color(VGA_LIGHT_GRAY, (u8)i);
        vga_puts("   ");
    }
    vga_set_color(VGA_LIGHT_GRAY, VGA_BLACK);
    vga_putc('\n');
}
