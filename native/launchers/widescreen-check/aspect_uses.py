import os, re, sys
# usage: aspect_uses.py <gen dir> <xbe>
gen, xbe = sys.argv[1], sys.argv[2]
import struct
d = open(xbe, 'rb').read()
base = struct.unpack_from('<I', d, 0x104)[0]
nsec, secaddr = struct.unpack_from('<II', d, 0x11C)
secs = []
for i in range(nsec):
    o = secaddr - base + i * 56
    fl, va, vs, ra, rs, na = struct.unpack_from('<IIIIII', d, o)
    secs.append((va, vs, ra, rs))
def rdf(va):
    for sva, vs, ra, rs in secs:
        if sva <= va < sva + rs: return struct.unpack_from('<I', d, ra + va - sva)[0]
consts = {0x3FAAAAAB: '4/3', 0x3F400000: '0.75', 0x3FE38E39: '16/9'}
func_re = re.compile(r'^void (sub_[0-9A-F]{8})\(void\)')
imm_re = re.compile(r'0x(3FAAAAAB|3F400000|3FE38E39)u?\b', re.I)
mem_re = re.compile(r'MEMF\(0x([0-9A-F]+)\)')
memcache = {}
hits = {}
for fn in sorted(os.listdir(gen)):
    if not fn.endswith('.c'): continue
    cur = None
    for ln, line in enumerate(open(os.path.join(gen, fn), encoding='latin-1'), 1):
        m = func_re.match(line)
        if m: cur = m.group(1); continue
        for m in imm_re.finditer(line):
            v = int(m.group(1), 16)
            hits.setdefault((cur, fn), []).append((ln, consts[v], 'imm', line.strip()[:120]))
        for m in mem_re.finditer(line):
            a = int(m.group(1), 16)
            if a not in memcache: memcache[a] = rdf(a)
            v = memcache[a]
            if v in consts:
                hits.setdefault((cur, fn), []).append((ln, consts[v], 'const@%X' % a, line.strip()[:120]))
for (f, fn), lst in sorted(hits.items(), key=lambda x: x[0][0] or ''):
    kinds = sorted(set(k for _, k, _, _ in lst))
    print('%s (%s) %s' % (f, fn, ','.join(kinds)))
    for ln, k, how, txt in lst[:4]:
        print('    %d [%s %s] %s' % (ln, k, how, txt))
