"""Check the PAL Action DSOUND 3D calculator against a Python reference model.

Static check only: the XBE's own code (0x1155FE, the full-HRTF calculate
routine, and 0x116E53, the voice volume packer) is run under Unicorn on
synthetic objects and compared with model() below.  See ACTION-3D-CALC.md.

Usage:  python -I dsound_3d_check.py <project-root> [cases] [seed]
"""
import sys, os, struct, math, random, hashlib

ROOT = sys.argv[1]
CASES = int(sys.argv[2]) if len(sys.argv) > 2 else 4000
SEED = int(sys.argv[3]) if len(sys.argv) > 3 else 7
sys.path.insert(0, os.path.join(ROOT, 'nightfire-port', 'analysis', 'python-deps'))
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_ECX, UC_X86_REG_CR0, UC_X86_REG_CR4

XBE = os.path.join(ROOT, 'nightfire-port', 'game_files', 'default.xbe')
data = open(XBE, 'rb').read()
assert hashlib.sha256(data).hexdigest() == 'b464b787250ffc0ea7443a0e0848689c03a6cdaf6eb593fa0dba509bd2641aa1'
xbase = struct.unpack_from('<I', data, 0x104)[0]
nsec = struct.unpack_from('<I', data, 0x11C)[0]
shdr = struct.unpack_from('<I', data, 0x120)[0] - xbase
secs = [struct.unpack_from('<6I', data, shdr + i * 56)[1:5] for i in range(nsec)]   # va, vsize, raw, rawsize

def xoff(va):
    for v, vs, ra, rs in secs:
        if v <= va < v + rs:
            return ra + va - v
def xf32(va): return struct.unpack_from('<f', data, xoff(va))[0]
def xu16(va): return struct.unpack_from('<H', data, xoff(va))[0]

# ---------------------------------------------------------------- emulator
mu = Uc(UC_ARCH_X86, UC_MODE_32)
mu.mem_map(0x10000, 0x300000)
for v, vs, ra, rs in secs:
    if v + rs <= 0x310000:
        mu.mem_write(v, data[ra:ra + rs])
HEAP, STK, RET = 0x400000, 0x600000, 0x700000
mu.mem_map(HEAP, 0x100000); mu.mem_map(STK, 0x10000); mu.mem_map(RET, 0x1000)
mu.mem_write(RET, b'\xf4')
mu.reg_write(UC_X86_REG_CR0, (mu.reg_read(UC_X86_REG_CR0) & ~4) | 2)
mu.reg_write(UC_X86_REG_CR4, mu.reg_read(UC_X86_REG_CR4) | 0x600)   # SSE for cvttss2si
def wf(a, *v): mu.mem_write(a, struct.pack('<%df' % len(v), *v))
def wd(a, *v): mu.mem_write(a, struct.pack('<%dI' % len(v), *[x & 0xffffffff for x in v]))
def rf(a): return struct.unpack('<f', mu.mem_read(a, 4))[0]
def ri(a): return struct.unpack('<i', mu.mem_read(a, 4))[0]
def ru(a): return struct.unpack('<I', mu.mem_read(a, 4))[0]
def call(fn, args=(), ecx=0):
    sp = STK + 0x8000
    for a in reversed(list(args)):
        sp -= 4; mu.mem_write(sp, struct.pack('<I', a & 0xffffffff))
    sp -= 4; mu.mem_write(sp, struct.pack('<I', RET))
    mu.reg_write(UC_X86_REG_ESP, sp); mu.reg_write(UC_X86_REG_ECX, ecx)
    mu.emu_start(fn, RET, count=2000000)

SRC, LOBJ, LP, SP, VFLAGS, CURVE = (HEAP + 0x1000 * i for i in range(1, 7))
def setup(spos, svel, lpos, lvel, front, top, mode, mn, mx, curve, surround, centre,
          sdist, sroll, sdop, ldist, lroll, ldop):
    mu.mem_write(SRC, b'\0' * 0x100)
    wd(SRC + 0xcc, centre); wd(SRC + 0xd0, LOBJ); wd(SRC + 0xd4, VFLAGS); wd(SRC + 0xd8, SP)
    wd(VFLAGS, 0x10)
    mu.mem_write(LOBJ, b'\0' * 0x80); wd(LOBJ + 4, LP)
    r = (top[1]*front[2]-top[2]*front[1], top[2]*front[0]-top[0]*front[2], top[0]*front[1]-top[1]*front[0])
    wf(LOBJ + 8, *r); mu.mem_write(LOBJ + 0x78, bytes([surround]))         # as 0x114CE5
    mu.mem_write(LP, b'\0' * 0x40); wd(LP, 0x40); wf(LP + 4, *lpos); wf(LP + 0x10, *lvel)
    wf(LP + 0x1c, *front); wf(LP + 0x28, *top); wf(LP + 0x34, ldist, lroll, ldop)
    mu.mem_write(SP, b'\0' * 0x7c); wd(SP, 0x4c); wf(SP + 4, *spos); wf(SP + 0x10, *svel)
    wd(SP + 0x1c, 360, 360); wf(SP + 0x24, 0, 0, 1); wf(SP + 0x34, mn, mx); wd(SP + 0x3c, mode)
    wf(SP + 0x40, sdist, sroll, sdop)
    if curve:
        wf(CURVE, *curve); wd(SP + 0x70, CURVE, len(curve))
def emu_out():
    return dict(az=rf(SRC+0x10), el=rf(SRC+0x14), distatt=ri(SRC+0x28), front=ri(SRC+0x30),
                back=ri(SRC+0x34), frontC=ri(SRC+0x38), centre=ri(SRC+0x3c), pitch=ri(SRC+0x40),
                fl=ru(SRC+0x44), fr=ru(SRC+0x48))

# ---------------------------------------------------------------- reference model
def f32(x): return struct.unpack('<f', struct.pack('<f', x))[0]
def tr(x): return int(f32(x))                       # fstp dword + cvttss2si (0x11A608)
T1 = [xf32(0x11CA20 + 4*i) for i in range(45)]
T2 = [xf32(0x11CAD8 + 4*i) for i in range(45)]
def mb_amp(g): return -10000 if g <= 0 else 0 if g >= 1 else tr(2000*math.log10(g))   # 0x114C4F
def mb_pow(g): return -10000 if g <= 0 else 0 if g >= 1 else tr(1000*math.log10(g))   # 0x114C08
def model(spos, svel, lpos, lvel, front, top, mode, mn, mx, curve, surround, centre,
          sdist, sroll, sdop, ldist, lroll, ldop, mute_at_max=False):
    o = {}
    dot = lambda a, b: a[0]*b[0] + a[1]*b[1] + a[2]*b[2]
    if mode == 1:                                   # head-relative: default axes, no listener pos
        d = list(spos); front, top, right = (0, 0, 1), (0, 1, 0), (1, 0, 0)
    else:
        d = [spos[i] - lpos[i] for i in range(3)]
        right = (top[1]*front[2]-top[2]*front[1], top[2]*front[0]-top[0]*front[2], top[0]*front[1]-top[1]*front[0])
    dist = math.sqrt(dot(d, d))
    if dist > 0: d = [x / dist for x in d]
    F, R, U = dot(front, d), dot(right, d), dot(top, d)
    if dist == 0: az = el = 0.0                     # 0x115490
    else:
        H = math.hypot(R, F)
        el = 45*abs(U)/H if abs(U) < H else 90 - 45*H/abs(U)
        if U < 0: el = -el
        az = 45*abs(R)/abs(F) if abs(F) > abs(R) else (90 - 45*abs(F)/abs(R) if R != 0 else 0.0)
        if F < 0: az = 180 - az
        if R < 0: az = -az
    o['az'], o['el'] = az, el
    if dist <= mn: da = 0                           # 0x114E6B
    elif mute_at_max and dist >= mx: da = -10000
    else:
        x = min(dist, mx)
        if curve:
            n = len(curve); step = (mx - mn)/n; t = x - mn; idx = min(int(t*(1/step)), n - 1)
            left = 1.0 if idx == 0 else curve[idx-1]; t -= idx*step
            da = mb_amp(left + (curve[idx] - left)*(t*(1/step)))
        else:
            xx = (x/mn - 1)*(lroll*sroll)
            da = tr(-2000*math.log10(1 + xx)) if xx >= 0 else 0
    o['distatt'] = da
    if surround:                                    # 0x1150CD
        A = ldist*sdist*dist
        B = min(1.0, max(0.0, f32((abs(az)/90 - 1)*(1 - abs(el)/90) + 0.5)))
        if A < 0.5: B = f32(0.5*((B - 0.5)*A + 1))
        o['front'], o['back'] = mb_pow(1 - B), mb_pow(B)
    else:
        o['front'], o['back'] = 0, -10000
    a, b = int(f32(abs(az))), int(f32(abs(el)))     # 0x1151B9
    if centre and a < 45 and b < 45:
        o['frontC'] = max(-10000, min(0, mb_amp(1 - T2[a]*T2[b])))
        o['centre'] = max(-10000, min(0, mb_amp(T1[a]*T1[b])))
    else:
        o['frontC'], o['centre'] = 0, -10000
    v = dot(svel, d) if mode == 1 else dot([svel[i] - lvel[i] for i in range(3)], d)   # 0x115297
    X = ldist*sdist*v*sdop*ldop
    o['pitch'] = 0 if X == 0 else -32767 if X >= 342 else 4096 if X <= -342 else round(4096*math.log2(1 - X/342))
    ie = tr(el + 3) if el >= 0 else tr(el - 3)      # 0x115359
    el6 = int(ie / 6) * 6
    if abs(el6) == 90: q = 0
    elif abs(el6) > 60: q = int(tr(abs(az) + 6) / 12) * 12
    elif abs(el6) > 30: q = int(tr(abs(az) + 3) / 6) * 6
    else: q = int(tr(abs(az) + 1.5) / 3) * 3
    if surround and q > 90: q = 180 - q
    ai, ei = int(q / 3), int((el6 + 90) / 6)
    ai = 0 if ai >= 61 else ai; ei = 0 if ei >= 31 else ei
    p0 = 0x11CD00 + 32 * xu16(0x12E2C0 + 2 * (ai*31 + ei)); p1 = p0 + 32
    o['fl'], o['fr'] = (p0, p1) if az >= 0 else (p1, p0)
    return o

# ---------------------------------------------------------------- random comparison
random.seed(SEED)
def rv(s): return tuple(random.uniform(-s, s) for _ in range(3))
def norm(v): l = math.sqrt(sum(x*x for x in v)); return tuple(x/l for x in v)
bad = {}
for k in range(CASES):
    fr_ = norm(rv(1)); tp = rv(1)
    dd = sum(fr_[i]*tp[i] for i in range(3)); tp = norm(tuple(tp[i] - dd*fr_[i] for i in range(3)))
    if k % 5 == 0: fr_, tp = (0, 0, 1), (0, 1, 0)
    sc = random.choice([0.3, 1, 5, 50, 500])
    kw = dict(spos=rv(sc), svel=rv(random.choice([0, 5, 50, 400])), lpos=rv(sc),
              lvel=rv(random.choice([0, 5, 50])), front=fr_, top=tp, mode=random.choice([0, 0, 0, 1]),
              mn=random.choice([0.5, 1, 2, 5]), mx=random.choice([10, 30, 100, 1e9]),
              curve=random.choice([None, [1, 0.5, 0.25, 0.125, 0]]), surround=random.choice([0, 1]),
              centre=random.choice([0, 1]), sdist=random.choice([1, 1, 0.5]), sroll=random.choice([1, 1, 2]),
              sdop=random.choice([1, 1, 3]), ldist=random.choice([1, 1, 2]), lroll=1.0, ldop=random.choice([1, 0.5]))
    if k % 7 == 0:
        kw['spos'] = tuple(float(round(x)) for x in kw['spos']); kw['lpos'] = (0, 0, 0)
    setup(**kw); call(0x1155FE, (0xFF, SRC)); e = emu_out(); m = model(**kw)
    for key in ('distatt', 'front', 'back', 'frontC', 'centre', 'pitch', 'fl', 'fr'):
        if abs(e[key] - m[key]) > (0 if key in ('fl', 'fr') else 1):
            # at/after the last curve point the gain is a float residue near 0: both are silent
            if key == 'distatt' and e[key] <= -10000 and m[key] <= -10000: continue
            # B exactly 0 or 1 (e.g. az 45, el 45): x87 leaves a ~1e-8 residue -> about -7900 mB
            if key in ('front', 'back') and min(e[key], m[key]) == -10000 and max(e[key], m[key]) < -6000: continue
            bad.setdefault(key, []).append((e[key], m[key], round(e['el'], 4)))
    for key in ('az', 'el'):
        if abs(e[key] - m[key]) > 1e-3: bad.setdefault(key, []).append((e[key], m[key]))
print('0x1155FE vs model: %d random cases' % CASES)
for key, v in bad.items(): print('  mismatch', key, len(v), v[:4])

# ---------------------------------------------------------------- volume packer 0x116E53
VC, ST, I3, OUT = HEAP + 0x8000, HEAP + 0x9000, HEAP + 0xA000, HEAP + 0xB000
def vols(lvol, head, bins, srcvals, direct, room, centreflag=1):
    for a, n in ((VC, 0x100), (ST, 0x100), (I3, 0x20), (SRC, 0x100)): mu.mem_write(a, b'\0' * n)
    wd(VC + 0x78, ST); wd(VC + 0x70, SRC); wd(VC + 0x74, I3); mu.mem_write(VC + 0x64, b'\x01')
    wd(ST + 8, 0x10); wd(ST + 0xb4, SP); wd(SP + 0x3c, 0); wd(ST + 0x1c, lvol - head); wd(ST + 0x20, head)
    wd(ST + 0x24, len(bins)); mu.mem_write(ST + 0x28, bytes(bins))
    for off, v in srcvals.items(): wd(SRC + off, v)
    wd(SRC + 0xcc, centreflag); wd(I3 + 4, direct); wd(I3 + 8, room)
    call(0x116E53, (OUT,), ecx=VC)
    a, b, c = ru(OUT), ru(OUT + 0xc), ru(OUT + 0x18)
    return [(a >> 4) & 0xfff, a >> 20, (b >> 4) & 0xfff, b >> 20, (c >> 4) & 0xfff, c >> 20,
            (a & 0xf) | ((b & 0xf) << 4) | ((c & 0xf) << 8),
            ((a >> 16) & 0xf) | (((b >> 16) & 0xf) << 4) | (((c >> 16) & 0xf) << 8)]
def vols_model(lvol, head, bins, s, direct, room, centreflag=1):
    g = lambda k: s.get(k, 0)
    base = g(0x28) + g(0x2c)
    front = min(0, g(0x38) + direct + g(0x30) + base); back = min(0, g(0x34) + direct + base)
    ctr = min(0, g(0x3c) + direct + g(0x30) + base); rm = min(0, room + base)
    out = []
    for i, bn in enumerate(bins):
        add = front if bn in (6, 7) else back if bn in (8, 9) else rm if bn == 10 else \
              ctr if (bn == 2 and centreflag) else base
        att = head - lvol - add
        out.append(min(0xfff, att*64//100) if att >= 0 else 0xfff)
    return out + [0xfff] * (8 - len(out))
tests = [(-500, 0, [6, 8, 7, 9, 2, 10], {0x28: -2000, 0x2c: -100, 0x30: -301, 0x34: -302, 0x38: -602, 0x3c: -301}, -50, -1000, 1),
         (-500, 0, [6, 8, 7, 9, 2, 10], {0x28: -2000, 0x30: -301, 0x34: -302, 0x38: -602, 0x3c: -301}, -50, -1000, 0),
         (0, 0, [6, 8, 7, 9, 2, 10], {0x34: -10000, 0x38: -602, 0x3c: -301}, 0, -1500, 1),
         (-100, 600, [0, 1, 2, 3, 4, 5], {0x28: -700}, 0, 0, 0),
         (200, 0, [6, 8, 7, 9, 2, 10], {}, 0, 0, 1)]
ok = sum(vols(*t) == vols_model(*t) for t in tests)
print('0x116E53 vs model: %d/%d crafted cases equal' % (ok, len(tests)))
