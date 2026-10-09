"""CP59: recover the PAL death handlers around the confirmed5680F stop."""
from pathlib import Path
import sys,json,hashlib,struct,re
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
out=root/'analysis/checkpoint-59/death-batch';out.mkdir(parents=True,exist_ok=True)
source=root/'src/recomp/gen/recomp_0002.c';s=source.read_text();report=[]
for start in (0x567f0,0x56a30,0x56bd0):
 assert start in table
 upper=min(a for a in table if a>start)
 db={a:dict(f) for a,f in original.items() if not start<a<upper}
 db[start]=dict(start=hex(start),end=upper,name=f'sub_{start:08X}',size=upper-start,section='game')
 t=FunctionTranslator(b,db);ins,tables,_=t._recover_cfg(start,upper,set(),set(db)-{start})
 end=max(i.address+i.size for i in ins)
 assert end<=upper
 t._recovered_cfg[start]=dict(end=end,instructions=ins,jump_tables=tables)
 code=t.translate_function(start,dict(db[start],end=end)).replace('uint32_t ebp;','uint32_t ebp = g_ebp;')
 (out/f'candidate-{start:08x}.c').write_text(code)
 (out/f'pal-{start:08x}.txt').write_text('\n'.join(f'{i.address:08X} {i.mnemonic} {i.op_str}' for i in ins))
 assert 'TODO' not in code and 'RECOMP_ITAIL(MEM32' not in code
 sig=f'void sub_{start:08X}(void)\n{{';a=s.index(sig);z=s.index('\n}\n',a)+3;old=s[a:z]
 report.append(dict(start=f'{start:08X}',bound=f'{upper:08X}',end=f'{end:08X}',instructions=len(ins),tables=len(tables),states=[f'{i:02X}' for i,v in enumerate(table) if v==start],old_sha256=hashlib.sha256(old.encode()).hexdigest()))
 (out/f'candidate-{start:08x}.c').write_text(code)
 (out/f'pal-{start:08x}.txt').write_text('\n'.join(f'{i.address:08X} {i.mnemonic} {i.op_str}' for i in ins))
 if '--apply' in sys.argv:
  s=s[:a]+code[code.index(sig):]+s[z:]
  s=re.sub(rf'0x{start:08X} - 0x[0-9A-F]+ \(\d+ bytes, \d+ insns\)',f'0x{start:08X} - 0x{end:08X} ({end-start} bytes, {len(ins)} insns); CP59 complete death-handler CFG',s)
(out/'report.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2))
if '--apply' in sys.argv:source.write_text(s)
