#include "kbd.h"

/* PS/2 键盘：轮询 8042 输出缓冲区（无需 IDT/PIC，稳定可靠） */

static const char map_lower[128] = {
    0,   27,  '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b', '\t',
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', 0,  'a', 's',
    'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0,  '\\', 'z', 'x', 'c', 'v',
    'b', 'n', 'm', ',', '.', '/', 0,   '*', 0,   ' ', 0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   '7', '8', '9', '-', '4', '5', '6', '+', '1',
    '2', '3', '0', '.', 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0
};

static const char map_upper[128] = {
    0,   27,  '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b', '\t',
    'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n', 0,  'A', 'S',
    'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~', 0,  '|', 'Z', 'X', 'C', 'V',
    'B', 'N', 'M', '<', '>', '?', 0,   '*', 0,   ' ', 0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   '7', '8', '9', '-', '4', '5', '6', '+', '1',
    '2', '3', '0', '.', 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0
};

static int shift = 0;
static int ctrl = 0;
static int e0_prefix = 0;

static void kbd_write(u8 cmd, u8 data)
{
    /* 等输入缓冲空 */
    u32 guard = 100000;
    while ((inb(0x64) & 0x02) && --guard) cpu_pause();
    outb(0x60, cmd);
    if (data) {
        guard = 100000;
        while ((inb(0x64) & 0x02) && --guard) cpu_pause();
        outb(0x60, data);
    }
}

void kbd_init(void)
{
    /* 清空残留输出，然后启用扫描 */
    u32 guard = 1000;
    while ((inb(0x64) & 0x01) && --guard) inb(0x60);
    kbd_write(0xF4, 0);        /* Enable Scanning */
    shift = ctrl = e0_prefix = 0;
}

int kbd_shift_down(void) { return shift; }
int kbd_ctrl_down(void) { return ctrl; }

/* 8042 控制器存在性检测：无控制器时状态口读到 0xFF */
int kbd_present(void)
{
    u8 st = inb(0x64);
    return (st != 0xFF) ? 1 : 0;
}

/* 读取一个原始扫描码，无数据返回 -1 */
static int read_scancode(void)
{
    if ((inb(0x64) & 0x01) == 0) return -1;
    return (int)inb(0x60);
}

int kbd_poll(void)
{
    for (;;) {
        int sc = read_scancode();
        if (sc < 0) return K_NONE;

        if (sc == 0xE0) { e0_prefix = 1; continue; }
        if (sc == 0xE1) { e0_prefix = 0; continue; }   /* Pause 等，忽略 */

        int released = (sc & 0x80) != 0;
        int code = sc & 0x7F;

        if (e0_prefix) {
            e0_prefix = 0;
            if (released) {
                if (code == 0x1D) ctrl = 0;
                continue;
            }
            switch (code) {
            case 0x1D: ctrl = 1; return K_NONE;
            case 0x48: return K_UP;
            case 0x50: return K_DOWN;
            case 0x4B: return K_LEFT;
            case 0x4D: return K_RIGHT;
            case 0x53: return K_DELETE;
            case 0x47: return K_HOME;
            case 0x4F: return K_END;
            case 0x49: return K_PGUP;
            case 0x51: return K_PGDN;
            default:   return K_NONE;
            }
        }

        if (code == 0x2A || code == 0x36) { shift = !released; continue; }
        if (code == 0x1D) { ctrl = !released; continue; }
        if (released) continue;

        char c = shift ? map_upper[code] : map_lower[code];
        if (code == 0x01) return K_ESC;
        if (c == 0) continue;
        return (int)(u8)c;
    }
}

int kbd_getkey(void)
{
    for (;;) {
        int k = kbd_poll();
        if (k != K_NONE) return k;
        cpu_pause();
    }
}
