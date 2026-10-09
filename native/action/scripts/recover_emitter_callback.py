"""CP39: recover the observed missing PAL emitter update callback; review by default."""
from pathlib import Path
import hashlib,json,sys,re
root=Path(__file__).resolve().parents[1]
sys.path[:0]=[str(root/'analysis/python-deps'),str(root.parent/'xboxrecomp')]
from tools.recomp import config
from tools.recomp.translator import FunctionTranslator
p=root/'game_files/default.xbe';binary=p.read_bytes()
assert hashlib.sha256(binary).hexdigest()=='b464b787250ffc0ea7443a0e0848689c03a6cdaf6eb593fa0dba509bd2641aa1'
config.configure_from_xbe(str(p))
db={int(f['start'],16):dict(f,end=int(f['end'],16)) for f in json.loads((root/'analysis/functions.json').read_text())}
start=0x68510;upper=0x68930
assert start not in db and min(a for a in db if a>start)==upper
db[start]=dict(start=hex(start),end=upper,name=f'sub_{start:08X}',size=upper-start,section='game')
t=FunctionTranslator(binary,db);ins,tables,_=t._recover_cfg(start,upper,set(),set(db)-{start})
end=max(i.address+i.size for i in ins)
t._recovered_cfg[start]=dict(end=end,instructions=ins,jump_tables=tables)
code=t.translate_function(start,dict(db[start],end=end)).replace('uint32_t ebp;','uint32_t ebp = g_ebp;')
assert 'TODO' not in code
out=root/'analysis/checkpoint-39';out.mkdir(exist_ok=True)
(out/'pal-00068510.txt').write_text('\n'.join(f'{i.address:08X} {i.mnemonic} {i.op_str}' for i in ins))
(out/'candidate-00068510.c').write_text(code)
print(hex(start),hex(end),len(ins),'instructions',len(tables),'tables')
if '--apply' in sys.argv:
 assert end==0x6892f and len(ins)==330 and not tables
 code=re.sub(r'(    PUSH32\(esp, 0x[0-9A-F]+u\); RECOMP_ABI_CALL)',r'    g_ebp = ebp; g_seh_ebp = ebp;\n\1',code)
 code=code.replace('POP32(esp, ebp);','POP32(esp, ebp);\n    g_ebp = ebp; g_seh_ebp = ebp;')
 (root/'src/recomp/gen/nightfire_emitter_callback.c').write_text('#define RECOMP_GENERATED_CODE\n#include "recomp_funcs.h"\n'+code)
 p=root/'src/recomp_manual.c';s=p.read_text()
 if 'case 0x00068510u:' not in s:
  s=s.replace('extern void sub_000324D0(void);','extern void sub_00068510(void);\nextern void sub_000324D0(void);').replace('    switch (address) {','    switch (address) {\n    case 0x00068510u: return sub_00068510;')
 p.write_text(s)
