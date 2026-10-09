"""Generated-code helper. usage:
  gen.py <tree> fn <hexaddr> [maxlines]     print function (noise filtered)
  gen.py <tree> callers <hexaddr>           functions that call / reference it
  gen.py <tree> refs <hexaddr>              any line mentioning the address
  gen.py <tree> containing <hexaddr>        function whose body contains the address (loc_)
"""
import os, re, sys
tree, cmd, addr = sys.argv[1], sys.argv[2], int(sys.argv[3], 16)
gen = os.path.join(tree, 'src', 'recomp', 'gen')
NOISE = re.compile(r'^\s*(_fas|_fbs|g_seh_ebp|_cf =|\(void\)|int _flags|uint32_t _fa|int32_t _fas|g_ebp = ebp|#define|#undef|g_fp_top =|uint32_t ebp = g_ebp|ebp = g_ebp|ebp = g_seh_ebp)')
files = sorted(f for f in os.listdir(gen) if f.endswith('.c'))
def lines_of(f):
    with open(os.path.join(gen, f), encoding='latin-1') as fh:
        return fh.read().split('\n')
func_re = re.compile(r'^void (sub_[0-9A-F]{8})\(void\)')
if cmd == 'fn':
    name = 'sub_%08X' % addr; mx = int(sys.argv[4]) if len(sys.argv) > 4 else 400
    for f in files:
        L = lines_of(f)
        for i, l in enumerate(L):
            if l.startswith('void ' + name + '(void)'):
                out = 0
                for j in range(i, len(L)):
                    t = L[j]
                    if j > i and t.startswith('}'): print(f'{j+1}: }}'); break
                    if t.strip() and not NOISE.match(t):
                        s = t.strip(); s = re.sub(r'\s*/\* (frame stays|publish frame)[^*]*\*/', '', s)
                        print(f'{j+1}: {s[:170]}'); out += 1
                        if out >= mx: print('...'); break
                print('file', f); sys.exit()
    print('not found')
elif cmd in ('callers', 'refs', 'containing'):
    pats = ['0x%08X' % addr, '0x%X' % addr, 'sub_%08X' % addr] if cmd != 'containing' else ['loc_%08X:' % addr]
    for f in files:
        cur = None
        for i, l in enumerate(lines_of(f)):
            m = func_re.match(l)
            if m: cur = m.group(1)
            if any(p.lower() in l.lower() for p in pats):
                if cmd == 'callers' and ('Original:' in l or l.startswith('void ') or '* sub_' in l): continue
                print(f'{f}:{i+1}: [{cur}] {l.strip()[:160]}')
                if cmd == 'containing': sys.exit()
