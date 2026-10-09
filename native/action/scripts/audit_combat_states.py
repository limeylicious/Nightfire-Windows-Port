"""Read-only audit: recover related PAL enemy state CFGs into review artifacts.
No generated game source is modified. Research names are leads, not ABI proof.
"""
from pathlib import Path
import hashlib,json,sys,re
root=Path(__file__).resolve().parents[1]
sys.path[:0]=[str(root/'analysis/python-deps'),str(root.parent/'xboxrecomp')]
from tools.recomp import config
from tools.recomp.translator import FunctionTranslator
b=(root/'game_files/default.xbe').read_bytes()
assert hashlib.sha256(b).hexdigest()=='b464b787250ffc0ea7443a0e0848689c03a6cdaf6eb593fa0dba509bd2641aa1'
config.configure_from_xbe(str(root/'game_files/default.xbe'))
original={int(f['start'],16):dict(f,end=int(f['end'],16)) for f in json.loads((root/'analysis/functions.json').read_text())}
source=(root/'src/recomp/gen/recomp_0002.c').read_text()
dispatch=(root/'src/recomp/gen/recomp_dispatch.c').read_text()
out=root/'analysis/checkpoint-37/combat-audit';out.mkdir(exist_ok=True)
results=[]
for start,upper in ((0x573f0,0x576b0),(0x576b0,0x57840),(0x57840,0x578d0),(0x578d0,0x579f0),(0x579f0,0x57a30),(0x57a30,0x57c80),(0x57c80,0x58040),(0x58040,0x58270)):
 db={a:dict(f) for a,f in original.items() if not start<a<upper}
 db[start]=dict(start=hex(start),end=upper,name=f'sub_{start:08X}',size=upper-start,section='game')
 t=FunctionTranslator(b,db);ins,tables,_=t._recover_cfg(start,upper,set(),set(db)-{start})
 end=max(i.address+i.size for i in ins)
 t._recovered_cfg[start]=dict(end=end,instructions=ins,jump_tables=tables)
 code=t.translate_function(start,dict(db[start],end=end))
 (out/f'candidate-{start:08x}.c').write_text(code)
 (out/f'pal-{start:08x}.txt').write_text('\n'.join(f'{i.address:08X} {i.mnemonic} {i.op_str}' for i in ins))
 a=source.index(f'void sub_{start:08X}(void)\n{{');z=source.index('\n}\n',a)+3;old=source[a:z]
 local={int(x,16) for x in re.findall(r'goto loc_([\dA-F]{8});',code)}
 missing=[f'{x:08X}' for x in sorted(local) if f'loc_{x:08X}:' not in old and f'0x{x:08X}' not in dispatch]
 results.append(dict(start=f'{start:08X}',end=f'{end:08X}',instructions=len(ins),tables=len(tables),unresolved_internal_targets=missing,todo='TODO' in code))
(out/'report.json').write_text(json.dumps(results,indent=2))
print(json.dumps(results,indent=2))

