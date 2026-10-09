"""Finite normalized rotations, frame blending and masked bones vs PAL code.
No external animation helper stubs on PAL side; native acos fixture only.
This isolates blending from animation loading, skinning and runtime timing.
"""
from pathlib import Path
import ctypes as C,struct,sys,random,math,json
from prepare_animation68 import extract
root=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(root/'analysis/checkpoint-35'))
from disassemble import image
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import *
assert (root/'tests/animation68_functions.inc').read_text()==extract()
lib=C.CDLL(str(root/'build-windows/RelWithDebInfo/nightfire_animation68_test.dll'))
lib.run.argtypes=[C.c_void_p,C.POINTER(C.c_uint32),C.c_uint32]
registers=[UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_ECX,UC_X86_REG_EDX,UC_X86_REG_ESI,UC_X86_REG_EDI,UC_X86_REG_EBP,UC_X86_REG_ESP]
sp,end,a,b,out,mask=0x3f0000,0x3e0000,0x3c0000,0x3c1000,0x3c2000,0x3c3000
rng=random.Random(68)
def quat():
 q=[rng.uniform(-1,1) for _ in range(4)];n=math.sqrt(sum(x*x for x in q));return [x/n for x in q]
pairs=[([0,0,0,1],[0,0,0,1]),([0,0,0,1],[0,0,0,-1]),([0,0,0,1],[0,1,0,0])]+[(quat(),quat()) for _ in range(40)]
cases=[]
for va in (0xd5e40,0xd5e90):
 for q1,q2 in pairs:
  for t in (0,.1,.5,.9,1):cases.append((va,1,t,1,0,q1,q2))
for n in (1,4):
 for t in (-.1,0,.1,.5,.9,1,1.1):
  for quality in (0,1):
   for masked in (0,1,2):cases.append((0x12b40,n,t,quality,masked,*pairs[3]))
failures=[];coverage=set();maximum=0
for index,(va,n,t,quality,masked,q1,q2) in enumerate(cases):
 data=image[:]
 def put(at,fmt,*v):struct.pack_into('<'+fmt,data,at,*v)
 put(sp,'I',end);put(0x300608,'I',1);put(0x1f6678,'I',quality)
 initial=[a,0x12345678,out,b,0x11223344,0x22334455,0x33445566,sp,0]
 if va==0x12b40:
  put(sp+4,'IIfI',255,n,t,mask if masked else 0)
  data[mask:mask+4]=bytes([0,0,255,0] if masked==2 else [255,0,255,0])
  for ptr,rot,sign in ((a,q1,1),(b,q2,-1)):
   put(ptr,f'{n*3}f',*[sign*(i+.25) for i in range(n*3)])
   put(ptr+n*12,f'{n*4}f',*(rot*n))
  size=n*7
 else:
  put(sp+4,'fIII',t,a,b,out);put(a,'4f',*q1);put(b,'4f',*q2);size=4
 u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(0,len(data));u.mem_write(0,bytes(data))
 for reg,v in zip(registers,initial):u.reg_write(reg,v)
 u.reg_write(UC_X86_REG_FPCW,0x27f)
 def hook(u,address,size,_):
  coverage.add(address)
  if not any(lo<=address<hi for lo,hi in ((0x12b40,0x13070),(0x24000,0x24090),(0xd5e40,0xd5f90),(0xee294,0xee35f),(0xefd0d,0xefdcd))):raise AssertionError(hex(address))
 u.hook_add(UC_HOOK_CODE,hook);u.emu_start(va,end,count=30000)
 assert u.reg_read(UC_X86_REG_EIP)==end
 ram=(C.c_ubyte*len(data)).from_buffer_copy(data);r=(C.c_uint32*9)(*initial);lib.run(ram,r,va)
 native=struct.unpack(f'<{size}f',bytes(ram[out:out+size*4]));pal=struct.unpack(f'<{size}f',u.mem_read(out,size*4))
 diffs=[(i,x,y) for i,(x,y) in enumerate(zip(native,pal)) if not (x==y or math.isclose(x,y,rel_tol=3e-6,abs_tol=3e-6))]
 maximum=max(maximum,max(abs(x-y) for x,y in zip(native,pal)))
 preserved=[i for i in (1,4,5,7) if r[i]!=u.reg_read(registers[i])]
 if diffs or preserved or r[8]:failures.append(dict(case=index,va=f'{va:08X}',t=t,n=n,quality=quality,masked=masked,diffs=diffs,registers=preserved,fp_top=r[8]))
report=dict(cases=len(cases),failures=failures,max_error=maximum,coverage=[f'{x:08X}' for x in sorted(coverage)],scope=__doc__)
folder=root/'analysis/checkpoint-68';folder.mkdir(exist_ok=True);(folder/'animation-comparisons.json').write_text(json.dumps(report,indent=2))
print(f'{len(cases)} comparisons; {len(failures)} failures; max error {maximum}',flush=True)
for failure in failures[:5]:print(failure)
sys.exit(bool(failures))
