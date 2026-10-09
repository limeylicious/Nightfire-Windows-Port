"""Condense MenuManager_Create (Action 0x093960): per tag case, the BIN_Get* reads and calls in order.
Test aid (read-only study). Usage: python blob_cases.py"""
import json, re
src = open(r'nightfire-port-native\src\recomp\gen\recomp_0003.c', encoding='latin-1').read()
s = src.index('void sub_00093960(void)'); body = src[s:src.index('\n}\n', s)]
L = {int(e['address'], 16): e['research_name'] for e in json.load(open(r'nightfire-port\analysis\nightfire-research-labels.json'))['entries']}
names = {0x6B170: 'DWORD', 0x6B150: 'WORD', 0x6B140: 'BYTE', 0x6B1A0: 'FLOAT'}
order = re.findall(r'if \(_jt == 0x([0-9A-F]{8})u\) goto', body[body.index('0x94268'):])[:15]
xbe = open(r'nightfire-port\game_files\default.xbe', 'rb').read()
import struct
base = 0x10000; nsec, sa = struct.unpack_from('<II', xbe, 0x11C)
secs = [struct.unpack_from('<IIIIII', xbe, sa - base + i * 56) for i in range(nsec)]
def u32(va):
    for fl, v, vs, ra, rs, n in secs:
        if v <= va < v + rs: return struct.unpack_from('<I', xbe, ra + va - v)[0]
tag_of = {}
for k in range(15):
    tag_of.setdefault(u32(0x94268 + 4 * k), []).append(k - 0x10)
blocks = re.split(r'\n(?=loc_[0-9A-F]{8}: ;)', body)
labels_in_order = [int(m, 16) for m in re.findall(r'^loc_([0-9A-F]{8}): ;', body, re.M)]
for tgt in sorted(tag_of):
    tags = tag_of[tgt]
    print('\n== tag %s (case %06X)' % (','.join(str(t) for t in tags), tgt))
    if tgt not in labels_in_order: print('   (default)'); continue
    i = labels_in_order.index(tgt)
    seq = []
    for blk in blocks:
        m = re.match(r'loc_([0-9A-F]{8}): ;', blk)
        if not m: continue
        a = int(m.group(1), 16)
        if a < tgt: continue
        if a != tgt and any(a == t for t in tag_of): break
        for line in blk.split('\n'):
            c = re.search(r'RECOMP_ABI_CALL\(0x([0-9A-F]{8})u', line)
            if c:
                f = int(c.group(1), 16); seq.append(names.get(f, 'call %06X %s' % (f, L.get(f, ''))))
            st = re.search(r'(MEM(8|16|32)\((e[a-z]x|esp|ebp) \+ 0x[0-9A-F]+\)) = (L[OH]\d+\()?e[a-d]x', line)
            if st and 'esp' not in st.group(1): seq.append('store ' + st.group(1))
            if 'goto loc_00093B00' in line or 'goto loc_000941FF' in line:
                seq.append('-> next record'); break
        if seq and seq[-1] == '-> next record': break
    print('   ' + ' | '.join(seq))
