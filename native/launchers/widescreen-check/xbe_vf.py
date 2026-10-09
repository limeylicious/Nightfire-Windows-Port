import struct, sys, hashlib, re
def load(p):
    d = open(p, 'rb').read()
    base = struct.unpack_from('<I', d, 0x104)[0]
    entry_enc = struct.unpack_from('<I', d, 0x128)[0]
    thunk_enc = struct.unpack_from('<I', d, 0x158)[0]
    nsec, secaddr = struct.unpack_from('<II', d, 0x11C)
    secs = []
    for i in range(nsec):
        o = secaddr - base + i * 56
        fl, va, vs, ra, rs, nameaddr = struct.unpack_from('<IIIIII', d, o)
        n = d[nameaddr - base:nameaddr - base + 16].split(b'\0')[0].decode('latin-1')
        secs.append((n, va, vs, ra, rs, fl))
    return d, base, entry_enc, thunk_enc, secs
def va2off(secs, base, va):
    for n, sva, vs, ra, rs, fl in secs:
        if sva <= va < sva + rs: return ra + va - sva
    if base <= va < base + 0x1000: return va - base
    return None
def off2va(secs, off):
    for n, sva, vs, ra, rs, fl in secs:
        if ra <= off < ra + rs: return sva + off - ra
for p in sys.argv[1:]:
    d, base, ee, te, secs = load(p)
    print('==', p, hashlib.sha256(d).hexdigest())
    thunk = None
    for key in (0x5B6D40B6, 0xEFB1F152, 0x46437DCD):
        t = te ^ key
        o = va2off(secs, base, t)
        if o is not None: thunk = t; break
    print('thunk table VA %08X' % thunk)
    o = va2off(secs, base, thunk); slots = {}
    i = 0
    while True:
        v = struct.unpack_from('<I', d, o + 4 * i)[0]
        if v == 0: break
        slots[v & 0x7FFFFFFF] = thunk + 4 * i; i += 1
    want = {24: 'ExQueryNonVolatileSetting', 29: 'ExSaveNonVolatileSetting', 1: 'AvGetSavedDataAddress', 3: 'AvSetDisplayMode', 4: 'AvSetSavedDataAddress'}
    for ordn, nm in want.items():
        if ordn not in slots: print(' ordinal %d (%s) not imported' % (ordn, nm)); continue
        s = slots[ordn]; sb = struct.pack('<I', s)
        print(' ordinal %d %s slot %08X' % (ordn, nm, s))
        for m in re.finditer(re.escape(b'\xff\x25' + sb) + b'|' + re.escape(b'\xff\x15' + sb), d):
            va = off2va(secs, m.start())
            kind = 'jmp' if d[m.start() + 1] == 0x25 else 'call'
            print('   %s [slot] at %08X' % (kind, va))
            if kind == 'jmp':  # stub: find direct callers
                for c in re.finditer(b'\xe8', d):
                    pass
    # direct E8 callers of given targets
    def callers(tgt):
        out = []
        for n, sva, vs, ra, rs, fl in secs:
            if not (fl & 4): continue  # executable
            blob = d[ra:ra + rs]
            for k in range(len(blob) - 5):
                if blob[k] == 0xE8:
                    rel = struct.unpack_from('<i', blob, k + 1)[0]
                    if (sva + k + 5 + rel) & 0xFFFFFFFF == tgt: out.append(sva + k)
        return out
    globals()['callers_' + str(len(sys.argv))] = callers
    if len(sys.argv) > 0:
        import os
        for t in os.environ.get('TGT', '').split(','):
            if t: print(' callers of %s:' % t, ' '.join('%08X' % x for x in callers(int(t, 16))))
    print(' sections:', ', '.join('%s %08X+%X' % (s[0], s[1], s[4]) for s in secs))
