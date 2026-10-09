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
source='\n'.join((root/f'src/recomp/gen/recomp_{unit:04d}.c').read_text() for unit in (1,2))
source+='\n'+'\n'.join(p.read_text() for p in (root/'src/recomp/gen').glob('nightfire_mission_callback_*.c'))
dispatch=(root/'src/recomp/gen/recomp_dispatch.c').read_text()
cover='--cover' in sys.argv
castle='--castle' in sys.argv
noise='--noise' in sys.argv
out=root/('analysis/checkpoint-40/noise-audit' if noise else 'analysis/checkpoint-38/castle-audit' if castle else 'analysis/checkpoint-38/cover-audit' if cover else 'analysis/checkpoint-36/enemy-audit');out.mkdir(parents=True,exist_ok=True)
results=[]
specs=((0x4cd60,0x4cf20),(0x4cf20,0x4d080),(0x4d080,0x4d1d0),(0x4d1d0,0x4d380),(0x5ba50,0x5bdd0),(0x5c420,0x5c880),(0x5c880,0x5cb00),(0x5cb00,0x5ce10),(0x5ce10,0x5cfd0),(0x5cfd0,0x5d0c0),(0x5d0c0,0x5d120)) if cover else ((0x591d0,0x59390),(0x59390,0x59700),(0x59c30,0x59d50),(0x59d50,0x5a020),(0x5a020,0x5a160),(0x5a160,0x5a2a0),(0x5a2a0,0x5a480),(0x5a480,0x5a660))
if castle:
 labels=json.loads((root/'analysis/nightfire-research-labels.json').read_text())['entries']
 addresses=sorted(int(row['address'],16) for row in labels)
 selected=(0x4f100,0x4f6f0,0x4f790,0x52c00,0x52da0,0x52f20,0x53430,0x536c0,0x53c00,0x53e10,0x53f80,0x54130,0x543c0,0x54650,0x546b0)
 specs=[(a,min(b for b in addresses if b>a)) for a in selected]
if noise:specs=((0x5d120,0x5d150),(0x5d150,0x5d190),(0x5d190,0x5d390),(0x5d390,0x5d790),(0x5d790,0x5dab0),(0x5dab0,0x5ddc0))
for start,upper in specs:
 db={a:dict(f) for a,f in original.items() if not start<a<upper}
 db[start]=dict(start=hex(start),end=upper,name=f'sub_{start:08X}',size=upper-start,section='game')
 t=FunctionTranslator(b,db);ins,tables,_=t._recover_cfg(start,upper,set(),set(db)-{start})
 end=max(i.address+i.size for i in ins)
 t._recovered_cfg[start]=dict(end=end,instructions=ins,jump_tables=tables)
 code=t.translate_function(start,dict(db[start],end=end))
 (out/f'candidate-{start:08x}.c').write_text(code)
 (out/f'pal-{start:08x}.txt').write_text('\n'.join(f'{i.address:08X} {i.mnemonic} {i.op_str}' for i in ins))
 signature=f'void sub_{start:08X}(void)\n{{'
 if signature in source:
  a=source.index(signature);z=source.index('\n}\n',a)+3;old=source[a:z]
 else:old=''
 local={int(x,16) for x in re.findall(r'goto loc_([\dA-F]{8});',code)}
 missing=[f'{x:08X}' for x in sorted(local) if f'loc_{x:08X}:' not in old and f'0x{x:08X}' not in dispatch and f'sub_{x:08X}(' not in old]
 results.append(dict(start=f'{start:08X}',end=f'{end:08X}',instructions=len(ins),tables=len(tables),entry_absent=not old,unresolved_internal_targets=missing,todo='TODO' in code))
(out/'report.json').write_text(json.dumps(results,indent=2))
print(json.dumps([dict(start=r['start'],end=r['end'],instructions=r['instructions'],tables=r['tables'],missing=len(r['unresolved_internal_targets']),entry_absent=r['entry_absent'],todo=r['todo']) for r in results] if cover or castle or noise else results,indent=2))
