"""Decode raw DXT1/DXT3/DXT5 mip 0 dumps (tex-ADDR-WxH-fmtXX.raw from
LEAN_DUMP_TEX) into a PNG sheet: colour on top, alpha below as grey.
NV2A format codes: 0x0C DXT1, 0x0E DXT3, 0x0F DXT5.
Usage: python scripts/dxt_view.py out.png file.raw [file.raw ...]"""
import os, re, struct, sys
sys.path.insert(0, os.path.dirname(__file__))
import lean_img


def c565(v):
    r, g, b = (v >> 11) & 31, (v >> 5) & 63, v & 31
    return (r << 3 | r >> 2, g << 2 | g >> 4, b << 3 | b >> 2)


def color_block(blk, dxt1):
    c0, c1, bits = struct.unpack('<HHI', blk)
    a, b = c565(c0), c565(c1)
    if c0 > c1 or not dxt1:
        pal = [a, b, tuple((2 * x + y) // 3 for x, y in zip(a, b)), tuple((x + 2 * y) // 3 for x, y in zip(a, b))]
        alpha3 = None
    else:
        pal = [a, b, tuple((x + y) // 2 for x, y in zip(a, b)), (0, 0, 0)]
        alpha3 = True
    out = []
    for i in range(16):
        k = (bits >> (2 * i)) & 3
        out.append((pal[k], 0 if (alpha3 and k == 3) else 255))
    return out


def decode(data, w, h, fmt):
    bs = 8 if fmt == 0x0C else 16
    rgb = [bytearray(w * 3) for _ in range(h)]
    al = [bytearray(w * 3) for _ in range(h)]
    off = 0
    for by in range(0, h, 4):
        for bx in range(0, w, 4):
            blk = data[off:off + bs]; off += bs
            if len(blk) < bs:
                return rgb, al
            if fmt == 0x0C:
                px = color_block(blk, True); alphas = [p[1] for p in px]
            else:
                px = color_block(blk[8:], False)
                if fmt == 0x0E:
                    v = int.from_bytes(blk[:8], 'little')
                    alphas = [((v >> (4 * i)) & 15) * 17 for i in range(16)]
                else:
                    a0, a1 = blk[0], blk[1]; v = int.from_bytes(blk[2:8], 'little')
                    tab = [a0, a1] + ([((6 - i) * a0 + (1 + i) * a1) // 7 for i in range(6)] if a0 > a1 else
                                      [((4 - i) * a0 + (1 + i) * a1) // 5 for i in range(4)] + [0, 255])
                    alphas = [tab[(v >> (3 * i)) & 7] for i in range(16)]
            for i in range(16):
                x, y = bx + (i & 3), by + (i >> 2)
                if x < w and y < h:
                    rgb[y][x * 3:x * 3 + 3] = bytes(px[i][0])
                    al[y][x * 3:x * 3 + 3] = bytes((alphas[i],) * 3)
    return rgb, al


def main():
    out, files = sys.argv[1], sys.argv[2:]
    imgs = []
    for f in files:
        m = re.search(r'-(\d+)x(\d+)-fmt([0-9A-F]{2})\.raw$', f)
        w, h, fmt = int(m.group(1)), int(m.group(2)), int(m.group(3), 16)
        rgb, al = decode(open(f, 'rb').read(), w, h, fmt)
        imgs.append((w, h * 2, rgb + al))
        a = [b for row in al for b in row[::3]]
        print(os.path.basename(f), 'mean_alpha=%.1f max_alpha=%d' % (sum(a) / len(a), max(a)))
    W, H, rows = lean_img.grid(imgs, len(imgs))
    lean_img.write_png(out, W, H, rows)


if __name__ == '__main__':
    main()
