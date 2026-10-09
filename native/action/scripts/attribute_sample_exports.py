"""Offline nearest-export leads for unresolved samples, using local PE files.
These are address-range leads, not verified function boundaries or call stacks.
"""
from pathlib import Path
import argparse, bisect, collections, json, struct
p=argparse.ArgumentParser();p.add_argument('sample',type=Path);a=p.parse_args()
report=json.loads(a.sample.read_text());cache={};counts=collections.Counter();distances=collections.defaultdict(list)
def exports(path):
 data=Path(path).read_bytes()
 def u16(at):return struct.unpack_from('<H',data,at)[0]
 def u32(at):return struct.unpack_from('<I',data,at)[0]
 pe=u32(0x3c)
 if data[pe:pe+4]!=b'PE\0\0':raise ValueError('not PE')
 count=u16(pe+6);optional=pe+24;sections=optional+u16(pe+20)
 def raw(rva):
  for i in range(count):
   at=sections+i*40;size=max(u32(at+8),u32(at+16));base=u32(at+12)
   if base<=rva<base+size:return u32(at+20)+rva-base
  raise ValueError('RVA outside sections')
 directory=optional+(112 if u16(optional)==0x20b else 96)
 export_rva,export_size=u32(directory),u32(directory+4)
 if not export_rva:return []
 table=raw(export_rva);functions=raw(u32(table+28));names=raw(u32(table+32));ordinals=raw(u32(table+36))
 result={}
 for i in range(u32(table+24)):
  index=u16(ordinals+i*2);rva=u32(functions+index*4)
  if export_rva<=rva<export_rva+export_size:continue # forwarded export
  at=raw(u32(names+i*4));name=data[at:data.index(b'\0',at)].decode(errors='replace')
  result.setdefault(rva,[]).append(name)
 return sorted((rva,'/'.join(names[:3])+(f' (+{len(names)-3} aliases)' if len(names)>3 else '')) for rva,names in result.items())
for row in report['functions']:
 if row['name']!='unresolved':continue
 path=row.get('module_path','unknown')
 if path=='unknown':continue
 if path not in cache:cache[path]=exports(path)
 entries=cache[path];rva=int(row['address'],16)-int(row['module_base'],16)
 i=bisect.bisect_right([x[0] for x in entries],rva)-1
 if i<0:continue
 base,name=entries[i];counts[(Path(path).name,name)]+=row['samples']
 distances[(Path(path).name,name)].append(rva-base)
 row['nearest_export_lead']=dict(name=name,displacement=rva-base)
out=dict(limitation=__doc__.strip(),leads=[dict(module=m,export=n,samples=c,percent=100*c/report['samples'],
 displacement_min=min(distances[(m,n)]),displacement_max=max(distances[(m,n)])) for (m,n),c in counts.most_common()])
a.sample.with_name('export-leads.json').write_text(json.dumps(out,indent=2))
print(json.dumps(out,indent=2))
