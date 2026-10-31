#include "editor.h"
#include "vga.h"
#include "kbd.h"

/* ============================================================
 * cunos edit -- GNU nano style full screen editor
 *   ^O save   ^X exit   arrows move   Enter newline   Backspace delete
 * ============================================================ */

#define TEXT_MAX   8192
#define TOP_ROW    1

static char text[TEXT_MAX];
static u32  len;
static u32  cursor;
static int  dirty;
static int  scroll_line;
static char fname[32];
static char msg[64];

static int ed_rows(void) { return vga_fb_active() ? disp_rows() - 4 : 21; }
static int ed_cols(void) { return disp_cols(); }
static int stat_row(void) { return disp_rows() - 2; }
static int help_row(void) { return disp_rows() - 1; }

static int line_index(u32 pos)
{
    int line = 0;
    for (u32 i = 0; i < pos && i < len; i++)
        if (text[i] == '\n') line++;
    return line;
}

static u32 line_start(u32 pos)
{
    while (pos > 0 && text[pos - 1] != '\n') pos--;
    return pos;
}

static int line_len(u32 start)
{
    int n = 0;
    while (start + (u32)n < len && text[start + n] != '\n') n++;
    return n;
}

static void set_msg(const char *s)
{
    strncpy(msg, s, sizeof(msg) - 1);
    msg[sizeof(msg) - 1] = '\0';
}

static void ed_render(void)
{
    int ROWS = ed_rows();
    int COLS = ed_cols();
    int cur_line = line_index(cursor);
    u32 ls = line_start(cursor);
    int cur_col = (int)(cursor - ls);

    if (cur_line < scroll_line) scroll_line = cur_line;
    if (cur_line >= scroll_line + ROWS) scroll_line = cur_line - ROWS + 1;
    if (scroll_line < 0) scroll_line = 0;

    vga_clear();

    /* 标题栏 */
    vga_fill_line(0, VGA_ATTR(VGA_BLACK, VGA_CYAN), ' ');
    vga_gotoxy(0, 1);
    vga_set_color(VGA_BLACK, VGA_CYAN);
    vga_printf(" cunos edit   %s    %s", fname, dirty ? "[modified]" : "");

    /* 正文 */
    vga_set_color(VGA_LIGHT_GRAY, VGA_BLACK);
    u32 pos = 0;
    int line = 0;
    while (pos < len && line < scroll_line) {
        if (text[pos] == '\n') line++;
        pos++;
    }

    int r = TOP_ROW;
    while (pos < len && r < TOP_ROW + ROWS) {
        vga_gotoxy(r, 0);
        int c = 0;
        while (pos < len && text[pos] != '\n' && c < COLS - 1) {
            vga_putc(text[pos]);
            pos++;
            c++;
        }
        if (pos < len && text[pos] == '\n') pos++;
        r++;
    }

    /* 状态栏 */
    vga_fill_line(stat_row(), VGA_ATTR(VGA_BLACK, VGA_LIGHT_GRAY), ' ');
    vga_gotoxy(stat_row(), 1);
    vga_set_color(VGA_BLACK, VGA_LIGHT_GRAY);
    vga_printf(" line %d / col %d    %u bytes    %s",
               cur_line + 1, cur_col + 1, len, msg);

    /* 快捷键栏 */
    vga_fill_line(help_row(), VGA_ATTR(VGA_WHITE, VGA_BLUE), ' ');
    vga_gotoxy(help_row(), 1);
    vga_set_color(VGA_WHITE, VGA_BLUE);
    vga_puts(" ^O save   ^X exit   arrows move   Enter newline   Backspace delete ");

    vga_set_color(VGA_LIGHT_GRAY, VGA_BLACK);
    vga_gotoxy(TOP_ROW + (cur_line - scroll_line), cur_col);
}

static void insert_char(char c)
{
    if (len + 1 >= TEXT_MAX) { set_msg("buffer full"); return; }
    for (u32 i = len; i > cursor; i--) text[i] = text[i - 1];
    text[cursor] = c;
    len++;
    cursor++;
    dirty = 1;
}

static void delete_at_cursor(void)
{
    if (cursor >= len) return;
    for (u32 i = cursor; i + 1 < len; i++) text[i] = text[i + 1];
    len--;
    dirty = 1;
}

static void backspace(void)
{
    if (cursor == 0) return;
    cursor--;
    delete_at_cursor();
}

static void save(fat_fs *fs)
{
    if (!fs || !fs->present) { set_msg("no FAT volume mounted"); return; }
    if (fat_write_file(fs, fname, text, len)) {
        dirty = 0;
        set_msg("saved to disk");
    } else {
        set_msg("save failed: disk write error");
    }
}

void editor_open(fat_fs *fs, const char *filename)
{
    u32 n = 0;

    strncpy(fname, filename, sizeof(fname) - 1);
    fname[sizeof(fname) - 1] = '\0';

    len = 0;
    cursor = 0;
    dirty = 0;
    scroll_line = 0;
    msg[0] = '\0';
    text[0] = '\0';

    if (fs && fs->present && fat_file_exists(fs, fname)) {
        if (!fat_read_file(fs, fname, text, TEXT_MAX - 1, &n)) n = 0;
        len = n;
        set_msg("file loaded");
    } else {
        set_msg("new file");
    }

    for (;;) {
        ed_render();
        int k = kbd_getkey();

        if (kbd_ctrl_down()) {
            char c = (char)(k & 0xFF);
            if (c == 'o' || c == 'O') { save(fs); continue; }
            if (c == 'x' || c == 'X') {
                if (dirty) {
                    set_msg("unsaved changes, press ^X again to quit");
                    ed_render();
                    int k2 = kbd_getkey();
                    if (!(kbd_ctrl_down() && ((char)(k2 & 0xFF) == 'x' || (char)(k2 & 0xFF) == 'X')))
                        continue;
                }
                vga_clear();
                vga_set_color(VGA_LIGHT_GRAY, VGA_BLACK);
                return;
            }
            continue;
        }

        if (k > 0 && k < 0x100) {
            char ch = (char)k;
            if (ch == '\b') { backspace(); continue; }
            if (ch == '\n' || ch == '\r') { insert_char('\n'); continue; }
            if (ch >= 32 && ch < 127) { insert_char(ch); continue; }
            continue;
        }

        switch (k) {
        case K_LEFT:
            if (cursor > 0) cursor--;
            break;
        case K_RIGHT:
            if (cursor < len) cursor++;
            break;
        case K_UP: {
            int line = line_index(cursor);
            int col = (int)(cursor - line_start(cursor));
            if (line == 0) { cursor = 0; break; }
            u32 s = line_start(cursor);
            if (s == 0) { cursor = 0; break; }
            s -= 1;
            u32 ps = line_start(s);
            int plen = line_len(ps);
            cursor = ps + (u32)(col > plen ? plen : col);
            break;
        }
        case K_DOWN: {
            int col = (int)(cursor - line_start(cursor));
            u32 s = line_start(cursor);
            int l = line_len(s);
            u32 e = s + (u32)l;
            if (e >= len) break;
            u32 ns = e + 1;
            int nlen = line_len(ns);
            cursor = ns + (u32)(col > nlen ? nlen : col);
            break;
        }
        case K_DELETE:
            delete_at_cursor();
            break;
        case K_HOME:
            cursor = line_start(cursor);
            break;
        case K_END:
            cursor = line_start(cursor) + (u32)line_len(line_start(cursor));
            break;
        case K_PGUP:
            for (int i = 0; i < ed_rows() && cursor > 0; i++) {
                u32 s = line_start(cursor);
                if (s == 0) { cursor = 0; break; }
                cursor = line_start(s - 1);
            }
            break;
        case K_PGDN:
            for (int i = 0; i < ed_rows(); i++) {
                u32 s = line_start(cursor);
                u32 e = s + (u32)line_len(s);
                if (e >= len) break;
                cursor = e + 1;
            }
            break;
        case K_ESC:
            if (dirty) { set_msg("unsaved changes, use ^X to quit"); break; }
            vga_clear();
            vga_set_color(VGA_LIGHT_GRAY, VGA_BLACK);
            return;
        default:
            break;
        }
    }
}
