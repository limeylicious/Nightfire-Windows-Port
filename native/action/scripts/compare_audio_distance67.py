"""Bounded native custom-distance curve comparison with original PAL114E6B.
Intercept only the final gain-to-decibel helper to observe its float argument.
This checks the custom attenuation curve, not general 3D/panning or audio fidelity.
"""
from pathlib import Path
import struct,sys,hashlib,subprocess,json
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'analysis/python-deps'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import *
b=(root/'game_files/default.xbe').read_bytes()
assert hashlib.sha256(b).hexdigest()=='b464b787250ffc0ea7443a0e0848689c03a6cdaf6eb593fa0dba509bd2641aa1'
u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(0,0x1000000)
base=struct.unpack_from('<I',b,0x104)[0];n,table=struct.unpack_from('<II',b,0x11c)
for i in range(n):
 _,va,vs,raw,size=struct.unpack_from('<5I',b,table-base+i*56)
 if size:u.mem_write(va,b[raw:raw+size])
def w(a,*v):u.mem_write(a,struct.pack('<'+'I'*len(v),*v))
def f(a,*v):u.mem_write(a,struct.pack('<'+'f'*len(v),*v))
def rd(a):return struct.unpack('<I',u.mem_read(a,4))[0]
obj,settings,voice,device,listener,curve=0x800000,0x801000,0x802000,0x803000,0x804000,0x805000
w(obj+0xd0,device,voice,settings);w(device+4,listener)
f(settings+0x34,10,100);w(settings+0x70,curve,5);f(curve,1,.5,.25,.125,0)
sentinel=0xf00000;stack=0xe00000;w(stack,sentinel,obj)
u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_EBP,0xe01000)
u.reg_write(UC_X86_REG_FPCW,0x37f);snapshot=u.context_save();observed=[]
def hook(uc,addr,size,data):
 if addr==sentinel:uc.emu_stop()
 elif addr==0x114c4f:
  sp=uc.reg_read(UC_X86_REG_ESP);observed.append(struct.unpack('<f',uc.mem_read(sp+4,4))[0])
  uc.reg_write(UC_X86_REG_EIP,rd(sp));uc.reg_write(UC_X86_REG_ESP,sp+8);uc.reg_write(UC_X86_REG_EAX,0)
u.hook_add(UC_HOOK_CODE,hook)
rows=subprocess.check_output([str(root/'build-windows/RelWithDebInfo/nightfire_game_audio_test.exe'),'--distance-report'],text=True)
cases=[]
for row in rows.splitlines():
 distance,native=map(float,row.split());u.context_restore(snapshot);w(stack,sentinel,obj);f(obj+12,distance);w(obj+0x28,0);observed.clear()
 try:u.emu_start(0x114e6b,sentinel,count=10000)
 except Exception:
  print('PAL exception',distance,hex(u.reg_read(UC_X86_REG_EIP)),hex(u.reg_read(UC_X86_REG_ESP)));raise
 if u.reg_read(UC_X86_REG_EIP)!=sentinel:raise AssertionError('PAL did not return')
 pal=observed[0] if observed else 1.0
 error=abs(native-pal);assert error<2e-6,(distance,native,pal,error)
 cases.append(dict(distance=distance,native=native,pal_gain_argument=pal,error=error))
result=dict(cases=len(cases),max_absolute_gain_error=max(x['error'] for x in cases),scope='custom five-point distance curve; PAL helper argument before dB conversion',results=cases)
out=root/'analysis/checkpoint-67';out.mkdir(exist_ok=True);(out/'distance-comparisons.json').write_text(json.dumps(result,indent=2))
print(json.dumps({k:v for k,v in result.items() if k!='results'},indent=2))
