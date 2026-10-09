"""Map Action's Handler_HandleMessage (0x8E320) IDs to handler functions + research names."""
import json, re, struct, sys
xbe = open(r'nightfire-port\game_files\default.xbe', 'rb').read()
base = 0x10000; nsec, sa = struct.unpack_from('<II', xbe, 0x11C)
secs = [struct.unpack_from('<IIIIII', xbe, sa - base + i * 56) for i in range(nsec)]
def off(va):
    for fl, v, vs, ra, rs, n in secs:
        if v <= va < v + rs: return ra + va - v
def u8(va): return xbe[off(va)]
def u32(va): return struct.unpack_from('<I', xbe, off(va))[0]
labels = {int(e['address'], 16): e['research_name'] for e in json.load(open(r'nightfire-port\analysis\nightfire-research-labels.json'))['entries']}
src = open(r'nightfire-port-native\src\recomp\gen\recomp_0003.c', encoding='latin-1').read()
start = src.index('void sub_0008E320(void)'); body = src[start:src.index('\n}\n', start)]
# case label -> first sub called after it
case_call = {}
for m in re.finditer(r'loc_([0-9A-F]{8}): ;(.*?)(?=loc_[0-9A-F]{8}: ;|\Z)', body, re.S):
    c = re.search(r'RECOMP_ABI_CALL\(0x([0-9A-F]{8})u', m.group(2))
    case_call[int(m.group(1), 16)] = int(c.group(1), 16) if c else None
# all compare constants and switch tables in the body, in order
print('compares:', ', '.join(hex(int(x, 16)) for x in re.findall(r'_fb = \(uint32_t\)\((0x[0-9A-F]+)\)', body)))
for m in re.finditer(r'edx = edx - (0x[0-9A-F]+);.*?_fb = \(uint32_t\)\((0x[0-9A-F]+)\).*?MEM8\(edx \+ (0x[0-9A-F]+)\).*?MEM32\(edx \* 4 \+ (0x[0-9A-F]+)\)', body, re.S):
    sub, rng, btab, jtab = (int(x, 16) for x in m.groups())
    print('\n== IDs %08X..%08X (byte table %X, jump table %X)' % (sub, sub + rng, btab, jtab))
    for k in range(rng + 1):
        tgt = u32(jtab + 4 * u8(btab + k)); h = case_call.get(tgt)
        if h: print('  %08X -> %06X %s' % (sub + k, h, labels.get(h, '?')))
for m in re.finditer(r'_fb = \(uint32_t\)\((0x[0-9A-F]+)\).*?if \(CMP_EQ\(_fa, _fb\)\) goto loc_([0-9A-F]{8})', body, re.S):
    tgt = int(m.group(2), 16); h = case_call.get(tgt)
    if h: print('  eq %s -> %06X %s' % (m.group(1), h, labels.get(h, '?')))
