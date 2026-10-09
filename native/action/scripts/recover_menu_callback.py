"""Recover the PAL indirect target observed after Start, using the pinned translator."""
from pathlib import Path
import hashlib,json,sys
import re
root=Path(__file__).resolve().parents[1]
sys.path[:0]=[str(root/'analysis/python-deps'),str(root.parent/'xboxrecomp')]
from tools.recomp import config
from tools.recomp.translator import FunctionTranslator
p=root/'game_files/default.xbe';binary=p.read_bytes()
assert hashlib.sha256(binary).hexdigest()=='b464b787250ffc0ea7443a0e0848689c03a6cdaf6eb593fa0dba509bd2641aa1'
config.configure_from_xbe(str(p))
db={int(f['start'],16):dict(f,end=int(f['end'],16)) for f in json.loads((root/'analysis/functions.json').read_text())}
start=int(sys.argv[1],0) if len(sys.argv)>1 else 0x6ed30
upper=min(a for a in db if a>start)
db[start]=dict(start=hex(start),end=upper,name=f'sub_{start:08X}',size=upper-start,section='game')
t=FunctionTranslator(binary,db)
ins,tables,_=t._recover_cfg(start,upper,set(),set(db)-{start})
end=max(i.address+i.size for i in ins)
assert ins[-1].mnemonic in ('ret','jmp')
t._recovered_cfg[start]=dict(end=end,instructions=ins,jump_tables=tables)
code=t.translate_function(start,dict(db[start],end=end)).replace('uint32_t ebp;','uint32_t ebp = g_ebp;')
if start==0x6f550:
 # This frameless function uses EBP as a saved general-purpose register.
 # Its bit-packing helper must see that live value when it saves EBP.
 code=re.sub(r'(    PUSH32\(esp, 0x[0-9A-F]+u\); RECOMP_ABI_CALL)',r'    g_ebp = ebp; g_seh_ebp = ebp;\n\1',code)
 code=code.replace('POP32(esp, ebp);','POP32(esp, ebp);\n    g_ebp = ebp; g_seh_ebp = ebp;')
assert 'TODO' not in code
name='nightfire_menu_callback.c' if start==0x6ed30 else f'nightfire_menu_callback_{start:08X}.c'
(root/'src/recomp/gen'/name).write_text('#define RECOMP_GENERATED_CODE\n#include "recomp_funcs.h"\n'+code)
(root/f'analysis/checkpoint-24/menu-callback-{start:08X}-disassembly.txt').write_text('\n'.join(f'{i.address:08X} {i.mnemonic} {i.op_str}' for i in ins))
p=root/'src/recomp_manual.c';s=p.read_text()
if f'case 0x{start:08X}u:' not in s:
 s=s.replace('extern void sub_000324D0(void);',f'extern void sub_{start:08X}(void);\nextern void sub_000324D0(void);').replace('    switch (address) {',f'    switch (address) {{\n    case 0x{start:08X}u: return sub_{start:08X};')
p.write_text(s)
print(hex(start),hex(end),len(ins),'instructions')
