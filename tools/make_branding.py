# Generates the Babylon Infernalis launcher background (assets/Babylon.svg) and the app icon (icons/app.png,
# icons/app.ico) in a minimal retro pixel style: desert night with colour bands, a striped sun, a ziggurat and dunes.
# Usage: python tools/make_branding.py   (writes into indy64recomp/)
import math
import os
import struct
import zlib

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'indy64recomp')

# Palette: night sky to sunset glow, sun, silhouettes.
SKY = ['#140c26', '#1c1030', '#2a1440', '#43184a', '#6b1f4f', '#9a2f4a', '#c8493d', '#e8703a']
SUN_TOP, SUN_BOTTOM = '#ffd35c', '#ff9a3c'
DUNE_BACK, DUNE_FRONT, GROUND = '#4a1c40', '#2a1232', '#1a0c22'
ZIGGURAT, ZIGGURAT_EDGE = '#24102c', '#3a1838'
STAR = '#f5d7a1'
TITLE, TITLE_SHADE = '#ffcf5a', '#9a2f4a'

# 5x7 pixel font, only the letters we need.
FONT = {
    'A': ['01110', '10001', '10001', '11111', '10001', '10001', '10001'],
    'B': ['11110', '10001', '10001', '11110', '10001', '10001', '11110'],
    'E': ['11111', '10000', '10000', '11110', '10000', '10000', '11111'],
    'F': ['11111', '10000', '10000', '11110', '10000', '10000', '10000'],
    'I': ['11111', '00100', '00100', '00100', '00100', '00100', '11111'],
    'L': ['10000', '10000', '10000', '10000', '10000', '10000', '11111'],
    'N': ['10001', '11001', '10101', '10011', '10001', '10001', '10001'],
    'O': ['01110', '10001', '10001', '10001', '10001', '10001', '01110'],
    'R': ['11110', '10001', '10001', '11110', '10100', '10010', '10001'],
    'S': ['01111', '10000', '10000', '01110', '00001', '00001', '11110'],
    'Y': ['10001', '10001', '01010', '00100', '00100', '00100', '00100'],
}


def rect(x, y, w, h, fill):
    return f'<rect x="{x}" y="{y}" width="{w}" height="{h}" fill="{fill}"/>'


def pixel_text(text, cx, top, px, fill):
    width = (len(text) * 6 - 1) * px
    x0 = cx - width // 2
    out = []
    for i, ch in enumerate(text):
        for row, bits in enumerate(FONT[ch]):
            # Merge runs of set pixels in a row into one rectangle.
            col = 0
            while col < 5:
                if bits[col] == '1':
                    start = col
                    while col < 5 and bits[col] == '1':
                        col += 1
                    out.append(rect(x0 + (i * 6 + start) * px, top + row * px, (col - start) * px, px, fill))
                else:
                    col += 1
    return out


def title(text, cx, top, px):
    # Retro lettering: a dark shadow one pixel down, the face on top.
    return (pixel_text(text, cx, top + px, px, TITLE_SHADE) +
            pixel_text(text, cx, top, px, TITLE))


def make_svg():
    W, H, HORIZON, P = 1600, 900, 720, 10
    out = [f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {W} {H}" width="{W}" height="{H}" '
           f'preserveAspectRatio="xMidYMid slice" shape-rendering="crispEdges">']
    # Sky bands, thinner towards the horizon.
    heights = [190, 130, 100, 80, 70, 60, 50, 40]
    y = 0
    for color, h in zip(SKY, heights):
        out.append(rect(0, y, W, h, color))
        y += h
    out.append(rect(0, y, W, HORIZON - y, SKY[-1]))
    # Stars in the dark upper bands (fixed positions, no randomness so the file stays stable).
    for sx, sy in [(90, 60), (260, 150), (420, 40), (1180, 70), (1330, 160), (1500, 50), (1450, 260), (140, 300),
                   (560, 30), (1040, 30), (330, 230), (1260, 300)]:
        out.append(rect(sx, sy, 6, 6, STAR))
    # Striped sun on the horizon, right of the menu.
    cx, r = 1300, 120
    for row in range(-r, 0, P):
        yy = HORIZON + row
        half = int(math.sqrt(max(0, r * r - (row + P / 2) ** 2)) // P * P)
        if half <= 0:
            continue
        depth = (row + r) / r  # 0 at the top, 1 at the horizon
        # Retro stripes: gaps get more frequent towards the horizon.
        band = (row + r) // P
        if depth > 0.45 and band % (2 if depth > 0.75 else 3) == 0:
            continue
        color = SUN_TOP if depth < 0.5 else SUN_BOTTOM
        out.append(rect(cx - half, yy, half * 2, P, color))
    # Ziggurat (stepped temple) left of the menu.
    zx = 320
    tiers = [(440, 60), (340, 55), (250, 50), (170, 45), (90, 40)]
    yy = HORIZON
    for width, height in tiers:
        yy -= height
        out.append(rect(zx - width // 2, yy, width, height, ZIGGURAT))
        out.append(rect(zx - width // 2, yy, width, 4, ZIGGURAT_EDGE))
    out.append(rect(zx - 10, HORIZON - 250, 20, 250 - 40, ZIGGURAT_EDGE))  # central stairway
    out.append(rect(zx - 30, yy - 30, 60, 30, ZIGGURAT))  # shrine
    # Dunes: two pixelated layers and the ground.
    for col in range(0, W, P):
        back = HORIZON - 20 + int(18 * math.sin(col / 210.0) + 10 * math.sin(col / 77.0))
        out.append(rect(col, back // P * P, P, H - back // P * P, DUNE_BACK))
    for col in range(0, W, P):
        front = HORIZON + 50 + int(26 * math.sin(col / 260.0 + 1.3) + 12 * math.sin(col / 95.0))
        out.append(rect(col, front // P * P, P, H - front // P * P, DUNE_FRONT))
    out.append(rect(0, H - 60, W, 60, GROUND))
    # Title in pixel letters (the launcher's own title label is removed).
    out += title('BABYLON', W // 2, 100, 16)
    out += title('INFERNALIS', W // 2, 250, 8)
    out.append('</svg>')
    with open(os.path.join(ROOT, 'assets', 'Babylon.svg'), 'w', encoding='utf-8') as f:
        f.write('\n'.join(out) + '\n')


def hex_rgb(color):
    return tuple(int(color[i:i + 2], 16) for i in (1, 3, 5))


def make_icon_pixels():
    # 32x32 pixel art: rounded tile with sky bands, striped sun and the ziggurat.
    n = 32
    img = [[None] * n for _ in range(n)]
    bands = [(0, SKY[1]), (9, SKY[3]), (15, SKY[5]), (19, SKY[7])]
    for y in range(n):
        color = [c for start, c in bands if y >= start][-1]
        for x in range(n):
            img[y][x] = color
    for y in range(n):  # sun
        for x in range(n):
            if (x - 16) ** 2 + (y - 21) ** 2 <= 10 ** 2 and y < 24 and y not in (17, 20, 22):
                img[y][x] = SUN_TOP if y < 18 else SUN_BOTTOM
    for x0, x1, y0, y1 in [(5, 26, 26, 31), (8, 23, 22, 25), (11, 20, 18, 21), (14, 17, 15, 17)]:  # ziggurat
        for y in range(y0, y1 + 1):
            for x in range(x0, x1 + 1):
                img[y][x] = ZIGGURAT
    for y in range(24, n):  # ground beside the temple
        for x in range(n):
            if img[y][x] != ZIGGURAT:
                img[y][x] = DUNE_FRONT
    for y in range(n):  # rounded corners
        for x in range(n):
            dx = max(3 - x, x - (n - 4), 0)
            dy = max(3 - y, y - (n - 4), 0)
            if dx * dx + dy * dy > 9:
                img[y][x] = None
    return img


def png_bytes(img, scale):
    n = len(img) * scale
    raw = bytearray()
    for y in range(n):
        raw.append(0)
        for x in range(n):
            c = img[y // scale][x // scale]
            raw += bytes(hex_rgb(c) + (255,)) if c else b'\0\0\0\0'

    def chunk(kind, data):
        return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', zlib.crc32(kind + data) & 0xFFFFFFFF)
    return (b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', n, n, 8, 6, 0, 0, 0)) +
            chunk(b'IDAT', zlib.compress(bytes(raw), 9)) + chunk(b'IEND', b''))


def make_icons():
    img = make_icon_pixels()
    with open(os.path.join(ROOT, 'icons', 'app.png'), 'wb') as f:
        f.write(png_bytes(img, 8))
    # ICO with PNG-compressed entries (Windows Vista and later).
    sizes = [(16, None), (32, 1), (48, None), (256, 8)]
    images = []
    for size, scale in sizes:
        if scale is None:
            # 16 and 48: nearest-neighbour resample of the 32x32 art.
            res = [[img[y * 32 // size][x * 32 // size] for x in range(size)] for y in range(size)]
            images.append((size, png_bytes(res, 1)))
        else:
            images.append((size, png_bytes(img, scale)))
    header = struct.pack('<HHH', 0, 1, len(images))
    offset = 6 + 16 * len(images)
    entries, data = b'', b''
    for size, blob in images:
        entries += struct.pack('<BBBBHHII', size % 256, size % 256, 0, 0, 1, 32, len(blob), offset + len(data))
        data += blob
    with open(os.path.join(ROOT, 'icons', 'app.ico'), 'wb') as f:
        f.write(header + entries + data)


if __name__ == '__main__':
    make_svg()
    make_icons()
    print('wrote assets/Babylon.svg, icons/app.png, icons/app.ico')
