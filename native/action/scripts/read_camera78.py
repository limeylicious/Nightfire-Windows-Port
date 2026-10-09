"""Inspect bounded raw camera/player history. Field labels are PAL research leads.
Does not classify intentional input or claim every float is an angle.
"""
from pathlib import Path
import argparse,struct,json,math,collections
p=argparse.ArgumentParser();p.add_argument('capture',type=Path);a=p.parse_args()
data=a.capture.read_bytes();magic,version,size,count,latest,updates=struct.unpack_from('<6I',data)
assert magic==0x4e464352 and version==1 and size==7808,(hex(magic),version,size)
assert len(data)==24+size*count and 0<count<=4096
block_size=(size-64)//2
offsets=dict(input=0,mapped=0xa8,camera=0xa8+0x158,player=0xa8+0x158+0x220,
             state=0xa8+0x158+0x220+0x100,weapon=0xa8+0x158+0x220+0x100+0x900)
fields={'camera':{'f4':0xf4,'100':0x100,'1dc':0x1dc,'1e0':0x1e0},
        'player':{'3c':0x3c,'40':0x40,'44':0x44,'4c':0x4c},
        'state':{'e0':0xe0,'e4':0xe4,'838':0x838,'860':0x860,'86c':0x86c,'870':0x870,'874':0x874,'878':0x878},
        'weapon':{'3c':0x3c,'40':0x40,'44':0x44}}
rows=[];previous=0
for i in range(count):
 start=24+i*size
 before_ms,after_ms,*meta=struct.unpack_from('<QQ12I',data,start)
 seq,update,camera,index,pb,sb,wb,pa,sa,wa,vb,va=meta
 assert seq>previous and after_ms>=before_ms and index==0
 previous=seq
 row=dict(sequence=seq,update=update,before_ms=before_ms,after_ms=after_ms,camera=hex(camera),
          player=hex(pa),state=hex(sa),weapon=hex(wa),valid_before=vb,valid_after=va)
 for side,delta in [('before',0),('after',block_size)]:
  base=start+64+delta;values={}
  for section,names in fields.items():
   for name,off in names.items():values[section+'_'+name]=struct.unpack_from('<f',data,base+offsets[section]+off)[0]
  values['camera_matrix_108']=struct.unpack_from('<12f',data,base+offsets['camera']+0x108)
  values['player_matrix_70']=struct.unpack_from('<12f',data,base+offsets['player']+0x70)
  values['processed_input_axes']=struct.unpack_from('<4f',data,base+0x1c)
  row[side]=values
 rows.append(row)
assert previous==latest
stats={}
for section,names in fields.items():
 for name in names:
  key=section+'_'+name;mask={'camera':4,'player':8,'state':16,'weapon':32}[section]
  values=[r['after'][key] for r in rows if r['valid_after']&mask];finite=[v for v in values if math.isfinite(v)]
  stats[key]=dict(min=min(finite) if finite else None,max=max(finite) if finite else None,nonfinite=len(values)-len(finite))
changes=[]
for first,second in zip(rows,rows[1:]):
 if not(first['valid_after']&4 and second['valid_after']&4):continue
 x=first['after']['camera_matrix_108'];y=second['after']['camera_matrix_108']
 # PAL uses three XYZ rows at16-byte strides; fourth slots are padding.
 delta=max(abs(x[i]-y[i]) for i in (0,1,2,4,5,6,8,9,10))
 if math.isfinite(delta):changes.append(dict(delta=delta,sequence=second['sequence'],ms=second['after_ms']))
report=dict(file=str(a.capture.resolve()),records=count,span_ms=rows[-1]['after_ms']-rows[0]['before_ms'],
 first_sequence=rows[0]['sequence'],last_sequence=latest,latest_update=updates,
 validity_counts=dict(collections.Counter((r['valid_before'],r['valid_after']) for r in rows)),
 field_ranges=stats,largest_matrix_component_steps=sorted(changes,key=lambda r:r['delta'],reverse=True)[:12])
report['validity_counts']={str(k):v for k,v in report['validity_counts'].items()}
a.capture.with_suffix('.summary.json').write_text(json.dumps(report,indent=2))
a.capture.with_suffix('.samples.json').write_text(json.dumps(rows,indent=2))
print(json.dumps(report,indent=2))
