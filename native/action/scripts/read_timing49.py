"""Bounded read-only game timing sample; no injection, pause or memory writes."""
from pathlib import Path
import argparse,ctypes as C,struct,time,json,re
p=argparse.ArgumentParser();p.add_argument('pid',type=int);p.add_argument('--seconds',type=int,default=20);a=p.parse_args()
if not 2<=a.seconds<=40:raise ValueError('2..40 seconds')
root=Path(__file__).resolve().parents[1]
archive=Path((root/'analysis/development/current-run.txt').read_text().strip())
mapping=re.search(r'Xbox memory mapped\. Offset: 0x([0-9a-fA-F]+)',(root/'logs/nightfire-startup.log').read_text(errors='replace'))
if not mapping:raise ValueError('run has not reported its guest-memory mapping')
memory_offset=int(mapping[1],16)
k=C.WinDLL('kernel32',use_last_error=True)
k.OpenProcess.argtypes=[C.c_uint32,C.c_int,C.c_uint32];k.OpenProcess.restype=C.c_void_p
k.ReadProcessMemory.argtypes=[C.c_void_p,C.c_void_p,C.c_void_p,C.c_size_t,C.POINTER(C.c_size_t)];k.ReadProcessMemory.restype=C.c_int
k.CloseHandle.argtypes=[C.c_void_p]
k.QueryFullProcessImageNameW.argtypes=[C.c_void_p,C.c_uint32,C.c_wchar_p,C.POINTER(C.c_uint32)]
h=k.OpenProcess(0x1010,False,a.pid)
if not h:raise C.WinError(C.get_last_error())
def read(address,n):
 buf=C.create_string_buffer(n);got=C.c_size_t()
 if not k.ReadProcessMemory(h,address+memory_offset,buf,n,C.byref(got)) or got.value!=n:raise C.WinError(C.get_last_error())
 return buf.raw
samples=[]
try:
 path=C.create_unicode_buffer(32768);size=C.c_uint32(len(path))
 if not k.QueryFullProcessImageNameW(h,0,path,C.byref(size)):raise C.WinError(C.get_last_error())
 expected=Path(json.loads((archive/'run-metadata.json').read_text())['executable']).resolve()
 if Path(path.value).resolve()!=expected:raise ValueError('PID is not this run executable')
 start=time.perf_counter()
 for i in range(a.seconds+1):
  timing=read(0x17c0f0,24);numerator,rate=struct.unpack_from('<2I',timing)
  if rate not in (50,60):raise ValueError('unexpected PAL timing scale/address mapping')
  counter,ticks,iterations=struct.unpack('<3I',read(0x1f65b4,12))
  row=dict(seconds=time.perf_counter()-start,numerator=numerator,rate=rate,
   floats=struct.unpack_from('<4f',timing,8),updates=counter,ticks=ticks,iterations=iterations,
   paused=struct.unpack('<H',read(0x1fec64,2))[0])
  samples.append(row)
  if i<a.seconds:time.sleep(1)
finally:
 k.CloseHandle(h)
 (archive/'timing-samples.json').write_text(json.dumps(samples,indent=2))
if len(samples)>1:
 first,last=samples[0],samples[-1];elapsed=last['seconds']-first['seconds'];updates=last['updates']-first['updates']
 print(json.dumps(dict(elapsed_seconds=elapsed,updates=updates,updates_per_second=updates/elapsed,
  configured_rate=last['rate'],seconds_per_update=last['floats'][3],
  accumulated_fixed_step_seconds=updates*last['floats'][3],paused_values=sorted(set(x['paused'] for x in samples))),indent=2))
