import os, re, sys
root = sys.argv[1]
words = [b'widescreen', b'wide screen', b'16:9', b'16x9', b'letterbox', b'letter box', b'aspect']
pats = []
for w in words:
    pats.append((w.decode(), re.compile(re.escape(w), re.I)))
    u = b''.join(bytes([c, 0]) for c in w)
    pats.append((w.decode() + ' (utf16)', re.compile(b''.join(re.escape(bytes([c])) + b'\x00' for c in w), re.I)))
for dp, dn, fn in os.walk(root):
    for f in fn:
        p = os.path.join(dp, f)
        with open(p, 'rb') as fh:
            off = 0; tail = b''
            while True:
                chunk = fh.read(64 << 20)
                if not chunk: break
                buf = tail + chunk; base = off - len(tail)
                for name, rx in pats:
                    for m in rx.finditer(buf):
                        s = max(0, m.start() - 40); e = m.end() + 40
                        ctx = buf[s:e].replace(b'\x00', b'').decode('latin-1')
                        ctx = ''.join(c if 32 <= ord(c) < 127 else '.' for c in ctx)
                        print('%s @0x%X [%s]: %s' % (os.path.relpath(p, root), base + m.start(), name, ctx))
                tail = buf[-64:]; off += len(chunk)
