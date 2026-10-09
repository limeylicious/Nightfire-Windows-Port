"""Summarize the bounded original command journal; no pixel-replay claim."""
from pathlib import Path
from collections import Counter
import json,struct,sys
p=Path(sys.argv[1]);p=p/'gpu143-methods.bin' if p.is_dir() else p
data=p.read_bytes()
assert data[:8]==b'NFGPU143' and (len(data)-8)%16==0, 'Invalid or torn journal'
records=list(struct.iter_unpack('<4I',data[8:]))
assert len(records)<=262144
assert all(r[0] in (1,2,3,4,5) for r in records)
methods=Counter();binding={};classes={};formats=Counter();events=Counter();draws=[];blits=[]
for tag,a,b,c in records:
    events[tag]+=1
    if tag==5:classes[a]=c
    if tag!=1:continue
    assert a<8 and b<0x2000 and b%4==0
    if b==0:binding[a]=c
    obj=binding.get(a)
    methods[(obj,b)]+=1
    if b==0x208:formats[(obj,c)]+=1
    if b==0x17fc and c:draws.append(dict(object=obj,primitive=c))
    if b==0x308 and classes.get(obj)==0x9f:blits.append(dict(object=obj,value=f'{c:08X}'))
result=dict(records=len(records),methods=events[1],drains_started=events[2],
    drains_finished=events[3],failures=events[4],limit_reached=len(records)==262144,
    formats=[dict(object=obj,value=f'{fmt:08X}',count=n) for (obj,fmt),n in formats.items()],
    begin_draw_method_count=len(draws),draw_samples=draws[:16],copy_size_samples=blits[:16],
    classes={str(k):f'{v:03X}' for k,v in classes.items()},
    common_methods=[dict(object=obj,method=f'{m:04X}',count=n) for (obj,m),n in methods.most_common(20)],
    limitation='Ordered original method values only; older journals lack class records; referenced RAM is not recorded.')
p.with_suffix('.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
print(json.dumps(result,indent=2))
