"""Read-only PAL instruction inspection; never executes the game."""
from pathlib import Path
import sys, struct, hashlib
root=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(root/'analysis/python-deps'))
from capstone import Cs, CS_ARCH_X86, CS_MODE_32
b=(root/'game_files/default.xbe').read_bytes()
assert hashlib.sha256(b).hexdigest()=='b464b787250ffc0ea7443a0e0848689c03a6cdaf6eb593fa0dba509bd2641aa1'
base=struct.unpack_from('<I',b,0x104)[0]
n,table=struct.unpack_from('<II',b,0x11c)
start,end=map(lambda v:int(v,0),sys.argv[1:3])
for i in range(n):
    _,va,vs,raw,size=struct.unpack_from('<5I',b,table-base+i*56)
    if va<=start<end<=va+size:
        for ins in Cs(CS_ARCH_X86,CS_MODE_32).disasm(b[raw+start-va:raw+end-va],start):
            print(f'{ins.address:08X} {ins.bytes.hex():20} {ins.mnemonic} {ins.op_str}')
        break
else: raise ValueError('range is not within a file-backed XBE section')
