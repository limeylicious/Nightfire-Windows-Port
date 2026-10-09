"""CP37: recover four related PAL enemy aim/fire switches as one audited batch."""
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
cover='--cover' in sys.argv
castle='--castle' in sys.argv
noise='--noise' in sys.argv
checkpoint=40 if noise else 38 if cover or castle else 37
out=root/f'analysis/checkpoint-{checkpoint}';out.mkdir(exist_ok=True)
report=[];sources={}
combat='--combat' in sys.argv
specs=((0x573f0,0x576b0,0x5765d,231),(0x576b0,0x57840,0x577fb,107),(0x57c80,0x58040,0x57ffb,315)) if combat else ((0x591d0,0x59390,0x5933d,150),(0x59390,0x59700,0x596ae,290),(0x59c30,0x59d50,0x59d05,80),(0x59d50,0x5a020,0x59fda,240))
if cover:specs=((0x4cf20,0x4d080,0x4d025,89),(0x5ba50,0x5bdd0,0x5bd51,277),(0x5c420,0x5c880,0x5c824,332))
if castle:specs=((0x52f20,0x53110,0x530cb,149),(0x536c0,0x53c00,0x53b84,380),(0x54130,0x543c0,0x54357,187))
if noise:specs=((0x5d390,0x5d790,0x5d726,318),)
for start,upper,end_expected,count in specs:
 source=root/f'src/recomp/gen/recomp_{1 if start==0x4cf20 else 2:04d}.c'
 s=sources.get(source,source.read_text())
 db={a:dict(f) for a,f in original.items() if not start<a<upper}
 db[start]=dict(start=hex(start),end=upper,name=f'sub_{start:08X}',size=upper-start,section='game')
 t=FunctionTranslator(b,db);ins,tables,_=t._recover_cfg(start,upper,set(),set(db)-{start})
 end=max(i.address+i.size for i in ins)
 assert end==end_expected and len(ins)==count and len(tables)==(2 if start in (0x5ba50,0x536c0) else 1)
 t._recovered_cfg[start]=dict(end=end,instructions=ins,jump_tables=tables)
 code=t.translate_function(start,dict(db[start],end=end)).replace('uint32_t ebp;','uint32_t ebp = g_ebp;')
 assert 'TODO' not in code
 (out/f'recovered-{start:08x}.c').write_text(code)
 (out/f'pal-{start:08x}.txt').write_text('\n'.join(f'{i.address:08X} {i.mnemonic} {i.op_str}' for i in ins))
 signature=f'void sub_{start:08X}(void)\n{{'
 if signature in s:
  a=s.index(signature);z=s.index('\n}\n',a)+3
  s=s[:a]+code[code.index(signature):]+s[z:]
 else:
  assert noise and start==0x5d390
  source=root/'src/recomp/gen/nightfire_noise_callback.c'
  s='#define RECOMP_GENERATED_CODE\n#include "recomp_funcs.h"\n'+code
 s=re.sub(rf'0x{start:08X} - 0x[0-9A-F]+ \(\d+ bytes, \d+ insns\)(?:; CP(?:37|38) full switch CFG)?',f'0x{start:08X} - 0x{end:08X} ({end-start} bytes, {count} insns); CP{checkpoint} full switch CFG',s)
 report.append(dict(start=hex(start),end=hex(end),instructions=count))
 sources[source]=s
if '--apply' in sys.argv:
 for source,s in sources.items():source.write_text(s)
 if noise:
  p=root/'src/recomp_manual.c';s=p.read_text()
  if 'case 0x0005D390u:' not in s:
   s=s.replace('extern void sub_000324D0(void);','extern void sub_0005D390(void);\nextern void sub_000324D0(void);').replace('    switch (address) {','    switch (address) {\n    case 0x0005D390u: return sub_0005D390;')
  p.write_text(s)
(out/('noise-recovery.json' if noise else 'castle-recovery.json' if castle else 'combat-recovery.json' if combat else 'recovery.json')).write_text(json.dumps(report,indent=2))
print(json.dumps(report,indent=2))
