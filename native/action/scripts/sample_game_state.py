"""Read only known Nightfire diagnostic addresses from the named game process.
Camera values are render globals, not yet identified as the player transform.
"""
import ctypes as C,struct,sys,time
from ctypes import wintypes as W
k=C.WinDLL('kernel32',use_last_error=True)
k.OpenProcess.argtypes=[W.DWORD,W.BOOL,W.DWORD];k.OpenProcess.restype=W.HANDLE
k.ReadProcessMemory.argtypes=[W.HANDLE,C.c_void_p,C.c_void_p,C.c_size_t,C.POINTER(C.c_size_t)]
k.CloseHandle.argtypes=[W.HANDLE]
h=k.OpenProcess(0x10,False,int(sys.argv[1]))
if not h:raise C.WinError(C.get_last_error())
def read(va,n):
    buf=C.create_string_buffer(n);done=C.c_size_t()
    # Verified host offset in this project's crash diagnostics.
    if not k.ReadProcessMemory(h,va+0x10000,buf,n,C.byref(done)) or done.value!=n:
        raise C.WinError(C.get_last_error())
    return buf.raw
try:
    print(time.time(),'normalized_axes',struct.unpack('<4f',read(0x2ff4b8,16)),
          'render_position',struct.unpack('<3f',read(0x2adef8,12)),flush=True)
    print('input_layout',struct.unpack('<h',read(0x1fe6de,2))[0],
          'mapped_input_14_to_2c',struct.unpack('<7f',read(0x1fe6e4,28)),flush=True)
finally:k.CloseHandle(h)
