"""Recover PAL StepAimRight's complete switch CFG, replacing the truncated body."""
from pathlib import Path
import hashlib,json,sys,re
root=Path(__file__).resolve().parents[1]
sys.path[:0]=[str(root/'analysis/python-deps'),str(root.parent/'xboxrecomp')]
from tools.recomp import config
from tools.recomp.translator import FunctionTranslator
p=root/'game_files/default.xbe';b=p.read_bytes()
assert hashlib.sha256(b).hexdigest()=='b464b787250ffc0ea7443a0e0848689c03a6cdaf6eb593fa0dba509bd2641aa1'
config.configure_from_xbe(str(p))
start=0x5a020 if '--left' in sys.argv else 0x5a160
upper=start+0x140
db={int(f['start'],16):dict(f,end=int(f['end'],16)) for f in json.loads((root/'analysis/functions.json').read_text())}
for a in list(db):
 if start<a<upper:del db[a]
db[start]=dict(start=hex(start),end=upper,name=f'sub_{start:08X}',size=upper-start,section='game')
t=FunctionTranslator(b,db);ins,tables,_=t._recover_cfg(start,upper,set(),set(db)-{start})
end=max(i.address+i.size for i in ins)
t._recovered_cfg[start]=dict(end=end,instructions=ins,jump_tables=tables)
code=t.translate_function(start,dict(db[start],end=end)).replace('uint32_t ebp;','uint32_t ebp = g_ebp;')
assert 'TODO' not in code
assert len(tables)==1 and len(ins)==95 and end==start+0xf2
suffix='-left' if '--left' in sys.argv else ''
(root/f'analysis/checkpoint-36/recovered-step-aim{suffix}.c').write_text(code)
(root/f'analysis/checkpoint-36/pal-step-aim{suffix}.txt').write_text('\n'.join(f'{i.address:08X} {i.mnemonic} {i.op_str}' for i in ins))
if '--apply' in sys.argv:
 p=root/'src/recomp/gen/recomp_0002.c';s=p.read_text();a=s.index(f'void sub_{start:08X}(void)\n{{');z=s.index('\n}\n',a)+3
 begin=code.index(f'void sub_{start:08X}(void)\n{{');s=s[:a]+code[begin:]+s[z:]
 s=re.sub(rf'0x{start:08X} - 0x[0-9A-F]+ \(\d+ bytes, \d+ insns\)(?:; CP36 full switch CFG)?',f'0x{start:08X} - 0x{end:08X} ({end-start} bytes, {len(ins)} insns); CP36 full switch CFG',s)
 p.write_text(s)
print(hex(start),hex(end),len(ins),'instructions',len(tables),'tables')
