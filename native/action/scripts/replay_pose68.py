"""Replay an actual captured AnimFrameCopy call with original PAL instructions.
No substituted animation/math helpers. CPU instruction model is test-only.
"""
from pathlib import Path
import argparse,struct,sys,json,math
root=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(root/'analysis/checkpoint-35'))
from disassemble import image # validates PAL XBE hash
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import *
p=argparse.ArgumentParser();p.add_argument('index',type=int);p.add_argument('--directory',type=Path,default=root/'analysis/checkpoint-68');a=p.parse_args()
folder=a.directory;data=(folder/f'pose-{a.index}-before.bin').read_bytes();after=(folder/f'pose-{a.index}-after.bin').read_bytes()
r=struct.unpack_from('<13I',data);assert r[0]==0x504f5345
u=Uc(UC_ARCH_X86,UC_MODE_32);at=116
while at<len(data):
 address,size=struct.unpack_from('<II',data,at);at+=8
 u.mem_map(address,size);u.mem_write(address,data[at:at+size]);at+=size
registers=[UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_ECX,UC_X86_REG_EDX,UC_X86_REG_ESI,UC_X86_REG_EDI,UC_X86_REG_EBP,UC_X86_REG_ESP]
for reg,v in zip(registers,r[2:10]):u.reg_write(reg,v)
u.reg_write(UC_X86_REG_FPCW,r[11]);u.reg_write(UC_X86_REG_FPSW,r[10]<<11);u.reg_write(UC_X86_REG_FPTAG,0xffff)
end=struct.unpack('<I',u.mem_read(r[9],4))[0]
output=struct.unpack_from('<I',after)[0];expected=after[40:]
trace=[];seen=set()
def hook(u,va,size,_):
 seen.add(va)
 if len(trace)<10000:trace.append(va)
 if not 0x11000<=va<0x15d000:raise AssertionError(hex(va))
u.hook_add(UC_HOOK_CODE,hook)
u.emu_start(r[1],end,count=300000)
assert u.reg_read(UC_X86_REG_EIP)==end,'instruction bound'
pal=bytes(u.mem_read(output,len(expected)));diff=[]
for i in range(0,len(pal),4):
 x,y=struct.unpack_from('<f',expected,i)[0],struct.unpack_from('<f',pal,i)[0]
 if expected[i:i+4]!=pal[i:i+4] and not (math.isnan(x) and math.isnan(y)) and not math.isclose(x,y,rel_tol=3e-6,abs_tol=3e-6):diff.append(dict(offset=hex(i),native=x,pal=y))
report=dict(index=a.index,address=hex(r[1]),output=hex(output),differences=diff,unique_instructions=len(seen),trace=[hex(x) for x in trace],registers_native=list(struct.unpack_from('<9I',after,4)),registers_pal=[u.reg_read(x) for x in registers]+[(u.reg_read(UC_X86_REG_FPSW)>>11)&7])
(folder/f'pose-{a.index}-comparison.json').write_text(json.dumps(report,indent=2))
print('Actual game pose',a.index,'differences',len(diff),'instructions',len(seen));print(diff[:8])
sys.exit(bool(diff))
