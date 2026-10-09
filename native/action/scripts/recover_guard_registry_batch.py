"""CP41: review PAL guard handlers missing from the static discovery database."""
from pathlib import Path
import sys,json,hashlib,struct
root=Path(__file__).resolve().parents[1]
sys.path[:0]=[str(root/'analysis/python-deps'),str(root.parent/'xboxrecomp'),str(root/'analysis/checkpoint-35')]
from tools.recomp import config
from tools.recomp.translator import FunctionTranslator
from disassemble import image
b=(root/'game_files/default.xbe').read_bytes()
assert hashlib.sha256(b).hexdigest()=='b464b787250ffc0ea7443a0e0848689c03a6cdaf6eb593fa0dba509bd2641aa1'
config.configure_from_xbe(str(root/'game_files/default.xbe'))
original={int(f['start'],16):dict(f,end=int(f['end'],16)) for f in json.loads((root/'analysis/functions.json').read_text())}
table=struct.unpack_from('<250I',image,0x177ca0)
entries=(0x4f440,0x4fa40,0x52660,0x528c0,0x556f0,0x4bbf0,0x58190,0x59040,0x5bf10,0x5dc10,0x4e040)
out=root/'analysis/checkpoint-41/guard-batch';out.mkdir(parents=True,exist_ok=True)
report=[];sources=[]
expected=((0x4f68c,225),(0x4fc56,195),(0x52716,64),(0x529d4,101),(0x557e9,95),(0x4bca6,51),(0x58246,54),(0x59171,121),(0x5c032,96),(0x5dd8e,125),(0x4e04b,4))
for start in entries:
 assert start in table
 upper=min(a for a in table if a>start)
 db={a:dict(f) for a,f in original.items() if not start<a<upper}
 db[start]=dict(start=hex(start),end=upper,name=f'sub_{start:08X}',size=upper-start,section='game')
 t=FunctionTranslator(b,db);ins,tables,_=t._recover_cfg(start,upper,set(),set(db)-{start})
 end=max(i.address+i.size for i in ins)
 t._recovered_cfg[start]=dict(end=end,instructions=ins,jump_tables=tables)
 code=t.translate_function(start,dict(db[start],end=end)).replace('uint32_t ebp;','uint32_t ebp = g_ebp;')
 assert (end,len(ins))==expected[entries.index(start)] and len(tables)==(0 if start==0x4e040 else 1)
 assert 'TODO' not in code
 sources.append(code)
 (out/f'candidate-{start:08x}.c').write_text(code)
 (out/f'pal-{start:08x}.txt').write_text('\n'.join(f'{i.address:08X} {i.mnemonic} {i.op_str}' for i in ins))
 report.append(dict(start=f'{start:08X}',bound=f'{upper:08X}',end=f'{end:08X}',instructions=len(ins),tables=len(tables),todo='TODO' in code,states=[f'{i:02X}' for i,v in enumerate(table) if v==start]))
(out/'report.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2))
if '--apply' in sys.argv:
 (root/'src/recomp/gen/nightfire_guard_registry.c').write_text('#define RECOMP_GENERATED_CODE\n#include "recomp_funcs.h"\n/* CP41: verified external entries from the PAL guard callback table. */\n'+'\n'.join(sources))
 p=root/'src/recomp_manual.c';s=p.read_text()
 for va in entries:
  if f'case 0x{va:08X}u:' not in s:
   s=s.replace('extern void sub_000324D0(void);',f'extern void sub_{va:08X}(void);\nextern void sub_000324D0(void);').replace('    switch (address) {',f'    switch (address) {{\n    case 0x{va:08X}u: return sub_{va:08X};')
 p.write_text(s)
