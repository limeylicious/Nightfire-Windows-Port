"""Pure-Python BMP reading, PNG writing and side-by-side panels (no PIL).

Usage:
  python scripts/lean_img.py png in.bmp out.png [crop x0 y0 x1 y1]
  python scripts/lean_img.py side out.png a.bmp b.bmp [c.bmp ...]   (horizontal strip)
  python scripts/lean_img.py grid out.png cols a.bmp b.bmp ...        (rows of `cols`)
  python scripts/lean_img.py stats a.bmp                               (mean luma, % dark)
"""
import struct, sys, zlib


def read_bmp(path):
    data = open(path, 'rb').read()
    off = struct.unpack_from('<I', data, 10)[0]
    w, h = struct.unpack_from('<ii', data, 18)
    bpp = struct.unpack_from('<H', data, 28)[0]
    top_down = h < 0
    h = abs(h)
    bpr = (w * bpp // 8 + 3) & ~3
    rows = []
    for y in range(h):
        sy = y if top_down else h - 1 - y
        row = data[off + sy * bpr: off + sy * bpr + w * bpp // 8]
        step = bpp // 8
        px = bytearray()
        for x in range(w):
            b, g, r = row[x * step], row[x * step + 1], row[x * step + 2]
            px += bytes((r, g, b))
        rows.append(px)
    return w, h, rows


def write_png(path, w, h, rows):
    raw = b''.join(b'\x00' + bytes(r) for r in rows)
    def chunk(t, d):
        c = struct.pack('>I', len(d)) + t + d
        return c + struct.pack('>I', zlib.crc32(t + d) & 0xffffffff)
    png = b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0))
    png += chunk(b'IDAT', zlib.compress(raw, 6)) + chunk(b'IEND', b'')
    open(path, 'wb').write(png)


def grid(images, cols, gap=6):
    cw = max(i[0] for i in images); ch = max(i[1] for i in images)
    n = len(images); rws = (n + cols - 1) // cols
    W = cols * cw + (cols - 1) * gap; H = rws * ch + (rws - 1) * gap
    out = [bytearray(b'\x20' * (W * 3)) for _ in range(H)]
    for k, (w, h, rows) in enumerate(images):
        ox = (k % cols) * (cw + gap); oy = (k // cols) * (ch + gap)
        for y in range(h):
            out[oy + y][ox * 3: ox * 3 + w * 3] = rows[y]
    return W, H, out


def main():
    cmd = sys.argv[1]
    if cmd == 'png':
        w, h, rows = read_bmp(sys.argv[2])
        if len(sys.argv) > 4 and sys.argv[4] == 'crop':
            x0, y0, x1, y1 = map(int, sys.argv[5:9])
            rows = [r[x0 * 3:x1 * 3] for r in rows[y0:y1]]; w, h = x1 - x0, y1 - y0
        write_png(sys.argv[3], w, h, rows)
    elif cmd in ('side', 'grid'):
        if cmd == 'side':
            out, files = sys.argv[2], sys.argv[3:]; cols = len(files)
        else:
            out, cols, files = sys.argv[2], int(sys.argv[3]), sys.argv[4:]
        W, H, rows = grid([read_bmp(f) for f in files], cols)
        write_png(out, W, H, rows)
    elif cmd == 'stats':
        for f in sys.argv[2:]:
            w, h, rows = read_bmp(f)
            tot = 0; dark = 0
            for r in rows:
                for x in range(0, len(r), 3):
                    l = (r[x] * 299 + r[x + 1] * 587 + r[x + 2] * 114) // 1000
                    tot += l; dark += l < 16
            print(f, 'mean_luma=%.1f dark%%=%.1f' % (tot / (w * h), 100.0 * dark / (w * h)))


if __name__ == '__main__':
    main()
