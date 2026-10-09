"""Read-only bounded pose/input survey of the current standalone test process.
Samples are asynchronous snapshots, not atomic simulation frames.
"""
from pathlib import Path
import argparse,ctypes as C,struct,time,json,re
p=argparse.ArgumentParser();p.add_argument('pid',type=int);a=p.parse_args()
root=Path(__file__).resolve().parents[1]
archive=Path((root/'analysis/development/current-run.txt').read_text().strip())
mapping=re.search(r'Xbox memory mapped\. Offset: 0x([0-9a-fA-F]+)',(root/'logs/nightfire-startup.log').read_text(errors='replace'))
if not mapping:raise ValueError('missing guest mapping')
offset=int(mapping[1],16);k=C.WinDLL('kernel32',use_last_error=True)
k.OpenProcess.argtypes=[C.c_uint32,C.c_int,C.c_uint32];k.OpenProcess.restype=C.c_void_p
k.ReadProcessMemory.argtypes=[C.c_void_p,C.c_void_p,C.c_void_p,C.c_size_t,C.POINTER(C.c_size_t)]
k.CloseHandle.argtypes=[C.c_void_p]
k.QueryFullProcessImageNameW.argtypes=[C.c_void_p,C.c_uint32,C.c_wchar_p,C.POINTER(C.c_uint32)]
h=k.OpenProcess(0x1010,False,a.pid)
if not h:raise C.WinError(C.get_last_error())
def read(address,n):
 b=C.create_string_buffer(n);got=C.c_size_t()
 if not k.ReadProcessMemory(h,address+offset,b,n,C.byref(got)) or got.value!=n:raise C.WinError(C.get_last_error())
 return b.raw
def word(address):return struct.unpack('<I',read(address,4))[0]
samples=[]
try:
 path=C.create_unicode_buffer(32768);n=C.c_uint32(len(path))
 if not k.QueryFullProcessImageNameW(h,0,path,C.byref(n)):raise C.WinError(C.get_last_error())
 if Path(path.value).resolve()!=Path(json.loads((archive/'run-metadata.json').read_text())['executable']).resolve():raise ValueError('wrong executable')
 for i in range(100):
  players=[]
  for obj in struct.unpack('<4I',read(0x1f6654,16)):
   if not 0x80000000<=obj<0x84000000:continue
   state=word(obj+0xbc)
   if not 0x80000000<=state<0x84000000:continue
   data=read(state,0x900);weapon=struct.unpack_from('<I',data,0x778)[0]
   players.append(dict(object=hex(obj),state=hex(state),data=data.hex(),weapon=hex(weapon),weapon_data=read(weapon,0x100).hex() if 0x80000000<=weapon<0x84000000 else None))
  samples.append(dict(time=time.time(),input=read(0x2ff49c,0xa8).hex(),mapped_input=read(0x1fe6d0,0x158).hex(),players=players))
  time.sleep(.05)
finally:
 k.CloseHandle(h)
 output=archive/f'pose-survey-{time.strftime("%H%M%S")}.json';output.write_text(json.dumps(samples,indent=2))
print(output)
print('Samples',len(samples),'players',len(samples[-1]['players']) if samples else 0)
