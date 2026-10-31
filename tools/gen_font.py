"""从 Windows 等宽字体渲染 ASCII 8x16 位图，生成内核用 C 头文件"""
import os
from PIL import Image, ImageDraw, ImageFont

CANDIDATES = [
    r"C:\Windows\Fonts\consola.ttf",
    r"C:\Windows\Fonts\lucon.ttf",
    r"C:\Windows\Fonts\cour.ttf",
    r"C:\Windows\Fonts\Courier New.ttf",
]

W, H = 8, 16
chars = [chr(c) for c in range(32, 127)]


def render(font_path, size):
    try:
        font = ImageFont.truetype(font_path, size)
    except Exception:
        return None
    glyphs = []
    for ch in chars:
        im = Image.new('L', (W, H), 0)
        d = ImageDraw.Draw(im)
        d.text((0, 0), ch, fill=255, font=font)
        px = im.load()
        rows = []
        for y in range(H):
            b = 0
            for x in range(W):
                if px[x, y] >= 110:
                    b |= (0x80 >> x)
            rows.append(b)
        glyphs.append(rows)
    return glyphs


def score(glyphs):
    """亮度统计：非空格字符不应全空"""
    total = sum(bin(r).count('1') for g in glyphs for r in g)
    empty = sum(1 for i, g in enumerate(glyphs) if chars[i] != ' ' and all(r == 0 for r in g))
    return total, empty


best = None
for path in CANDIDATES:
    if not os.path.exists(path):
        continue
    for size in range(11, 17):
        gl = render(path, size)
        if not gl:
            continue
        total, empty = score(gl)
        if empty == 0:
            print("candidate", os.path.basename(path), "size", size, "ink", total)
            if best is None or abs(size - 13) < abs(best[2] - 13):
                best = (path, gl, size)

if not best:
    raise SystemExit("没有可用字体")

path, glyphs, size = best
print("使用字体:", path, "size", size)

# 生成 C 文件
lines = []
lines.append('/* 由 tools/gen_font.py 自动生成：8x16 点阵 ASCII 字体 (32..126) */')
lines.append('')
lines.append('#include "font8x16.h"')
lines.append('')
lines.append('const unsigned char font8x16[95][16] = {')
for i, g in enumerate(glyphs):
    ch = chars[i]
    disp = ch if 32 < ord(ch) < 127 else '?'
    vals = ','.join('0x%02X' % b for b in g)
    lines.append('    {%s},  /* %d %s */' % (vals, ord(ch), disp.replace('\\', '\\\\').replace("'", '')))
lines.append('};')
lines.append('')

open(r'C:\cunos\src\font8x16.c', 'w', encoding='utf-8', newline='\n').write('\n'.join(lines))
open(r'C:\cunos\src\font8x16.h', 'w', encoding='utf-8', newline='\n').write(
    '/* 8x16 点阵 ASCII 字体 (32..126) */\n'
    '#ifndef FONT8X16_H\n#define FONT8X16_H\n\n'
    'extern const unsigned char font8x16[95][16];\n\n'
    '#endif\n')

# 预览图
preview = Image.new('RGB', (W * 63, H * 3 * 4), (0, 0, 0))
sample = "cunos 0.3.0 FAT32 ACPI PS/2 AMD NVIDIA 0123456789 ()[]{}<>:;,.!?/_+-=*#@&%$^~|"
for k in range(4):
    off = k * 63
    for i, ch in enumerate(sample[off:off + 63]):
        g = glyphs[ord(ch) - 32]
        for y in range(H):
            for x in range(W):
                if g[y] & (0x80 >> x):
                    preview.putpixel((i * W + x, k * H + y), (0, 255, 180))
preview = preview.resize((preview.width * 3, preview.height * 3), Image.NEAREST)
preview.save(r'C:\cunos\shots\font_preview.png')
print("preview saved")
