"""Recover missing handlers proven by the XBE's render-state dispatch table."""
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
entries=[struct.unpack_from('<I',binary,config.va_to_file_offset(a))[0] for a in range(0x10d948,0x10d9c0,4)]
missing=sorted(set(entries)-set(db));starts=sorted(set(db)|set(refs))
for start in missing:
 assert start in refs and refs[start]['name'].startswith('D3DDevice_SetRenderState_')
 end=min(x for x in starts if x>start)
 db[start]=dict(start=hex(start),end=end,name=f'sub_{start:08X}',size=end-start,section='D3D')
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
(root/'src/recomp/gen/nightfire_render_states.c').write_text('\n'.join(output))
# Keep the XPP manual lookup, add a separate lookup for this new table.
output=['#include "recomp_types.h"']+[f'extern void sub_{a:08X}(void);' for a in missing]
output+=['recomp_func_t nightfire_lookup_render_state(uint32_t va) { switch (va) {']+[f'case 0x{a:08X}u: return sub_{a:08X};' for a in missing]+['default: return NULL; } }']
(root/'src/recomp/gen/nightfire_render_state_dispatch.c').write_text('\n'.join(output)+'\n')
p=root/'src/recomp_manual.c';s=p.read_text()
if 'extern recomp_func_t nightfire_lookup_render_state' not in s:
 s=s.replace('recomp_func_t recomp_lookup_manual(uint32_t address)','extern recomp_func_t nightfire_lookup_render_state(uint32_t va);\n\nrecomp_func_t recomp_lookup_manual(uint32_t address)').replace('default: return NULL;','default: return nightfire_lookup_render_state(address);')
p.write_text(s)
(root/'analysis/render-state-recovery.json').write_text(json.dumps(dict(table_start='0x10d948',table_end='0x10d9c0',entries=len(entries),recovered=report),indent=2))
print(json.dumps(report,indent=2))
