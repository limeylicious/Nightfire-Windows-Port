"""Decode the bounded published portion of a captured original pushbuffer."""
from pathlib import Path
import json,re,struct,sys
run=Path(sys.argv[1]); index=sys.argv[2] if len(sys.argv)>2 else '1'
t=(run/f'gpu143-{index}.txt').read_text()
fields={k:int(v,16) for k,v in re.findall(r'(\w+)=([0-9A-F]{8})',t)}
b=(run/f'gpu143-{index}-pushbuffer.bin').read_bytes()
base=fields['begin'];limit=fields['submitted']-base
assert 0<=limit<=len(b)
offset=0;events=[]
while offset<limit:
    at=base+offset;w=struct.unpack_from('<I',b,offset)[0];offset+=4
    if (w&0xe0030003) not in (0,0x40000000):raise ValueError(f'control/header {at:08x}: {w:08x}')
    n=(w>>18)&0x7ff;m=w&0x1ffc;sub=(w>>13)&7;inc=0 if w&0x40000000 else 4
    assert offset+n*4<=limit
    for j in range(n):
        value=struct.unpack_from('<I',b,offset)[0];offset+=4
        events.append(dict(at=f'{at:08X}',sub=sub,method=f'{m+j*inc:04X}',value=f'{value:08X}'))
(run/f'gpu143-{index}-methods.json').write_text(json.dumps(events,indent=2))
print(f'{len(events)} methods; {limit} published bytes; no unknown headers')
for e in events:
    if int(e['method'],16) in (0,0x180,0x184,0x188,0x18c,0x190,0x194,0x198,0x19c,0x1a0,0x1a4,0x1d6c,0x1d70,0x208,0x210,0x214): print(e)
