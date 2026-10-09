"""Recover the callback reached by the diagnostic main-loop run."""
from pathlib import Path
import hashlib,json,struct,sys
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root.parent/'xboxrecomp'))
from tools.recomp import config
from tools.recomp.translator import FunctionTranslator
p=root/'game_files/default.xbe';binary=p.read_bytes()
assert hashlib.sha256(binary).hexdigest()=='b464b787250ffc0ea7443a0e0848689c03a6cdaf6eb593fa0dba509bd2641aa1'
config.configure_from_xbe(str(p))
fs=json.loads((root/'analysis/functions.json').read_text());db={int(f['start'],16):dict(f,end=int(f['end'],16)) for f in fs}
refs=json.loads((root.parent/'nightfire-research-cxbx/tools/functions_action.json').read_text());refs={int(f['address'],16):f for f in refs}
entries=[0x324d0]
missing=sorted(set(entries)-set(db));starts=sorted(set(db)|set(refs))
for start in missing:
 assert start in refs and refs[start]['name']=='DroneSpawner_Control'
 end=min(x for x in starts if x>start)
 db[start]=dict(start=hex(start),end=end,name=f'sub_{start:08X}',size=end-start,section='game')
t=FunctionTranslator(binary,db);output=['#define RECOMP_GENERATED_CODE\n#include "recomp_funcs.h"\n#include "../../nightfire_diagnostics.h"\n'];report=[]
for start in missing:
 upper=db[start]['end'];ins,tables,_=t._recover_cfg(start,upper,set(),set(db)-{start})
 end=max(i.address+i.size for i in ins)
 assert ins[-1].mnemonic in ('ret','jmp'),(hex(start),ins[-1].mnemonic)
 t._recovered_cfg[start]=dict(end=end,instructions=ins,jump_tables=tables)
 code=t.translate_function(start,dict(db[start],end=end)).replace('uint32_t ebp;','uint32_t ebp = g_ebp;')
 assert 'TODO' not in code,(hex(start),code)
 code=code.replace('POP32(esp, ebp); /* leave */','POP32(esp, ebp); /* leave */\n    g_ebp = ebp;\n    g_seh_ebp = ebp;')
 output.append(code);report.append(dict(address=hex(start),end=hex(end),name=refs[start]['name'],instructions=len(ins)))
(root/'src/recomp/gen/nightfire_object_callbacks.c').write_text('\n'.join(output))

p=root/'src/recomp_manual.c';s=p.read_text()
if 'case 0x000324D0u:' not in s:
 s=s.replace('extern void sub_00157334(void);','extern void sub_000324D0(void);\nextern void sub_00157334(void);').replace('    switch (address) {','    switch (address) {\n    case 0x000324D0u: return sub_000324D0;')
p.write_text(s)
(root/'analysis/object-callback-recovery.json').write_text(json.dumps(report,indent=2))
print(json.dumps(report,indent=2))
