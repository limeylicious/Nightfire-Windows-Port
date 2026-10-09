"""Bounded read-only music-ring survey of our own development run."""
from pathlib import Path
import argparse,ctypes as C,struct,time,json,re,hashlib
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
samples=[]
try:
 path=C.create_unicode_buffer(32768);n=C.c_uint32(len(path))
 if not k.QueryFullProcessImageNameW(h,0,path,C.byref(n)):raise C.WinError(C.get_last_error())
 if Path(path.value).resolve()!=Path(json.loads((archive/'run-metadata.json').read_text())['executable']).resolve():raise ValueError('wrong executable')
 start=time.perf_counter()
 for i in range(100):
  globals=struct.unpack('<256I',read(0x298A48,1024))
  registry=read(0x2AE8C0,64*64);rings=[]
  for slot in range(64):
   words=struct.unpack_from('<16I',registry,slot*64)
   # E1860/E18A0 identify +28 as guest data and +0 as sound interface.
   if not words[10]:continue
   if words[0]!=0x8065CF28:continue
   data=read(words[10],0x4800)
   valid=sum(all(data[at+ch*4+2]<=88 and data[at+ch*4+3]==0 for ch in range(2)) for at in range(0,len(data),72))
   rings.append(dict(slot=slot,words=[hex(x) for x in words],valid_blocks=valid,hash=hashlib.sha256(data).hexdigest(),prefix=data[:16].hex()))
   if i in (0,50,99):(archive/f'music-ring-{i}.bin').write_bytes(data)
  samples.append(dict(seconds=time.perf_counter()-start,rings=rings,globals=[hex(x) for x in globals]))
  time.sleep(.1)
finally:
 k.CloseHandle(h);(archive/'music-ring-survey.json').write_text(json.dumps(samples,indent=2))
print(json.dumps(dict(samples=len(samples),rings=samples[-1]['rings'],unique_ring_hashes=len({r['hash'] for s in samples for r in s['rings']})),indent=2))
