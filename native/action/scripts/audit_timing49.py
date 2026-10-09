"""Read-only PAL timing evidence; no game or executable modification."""
from pathlib import Path
import sys,struct,json
root=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(root/'analysis/checkpoint-35'))
from disassemble import image,Cs,CS_ARCH_X86,CS_MODE_32
md=Cs(CS_ARCH_X86,CS_MODE_32)
ranges=((0xdd1d0,0xdd1ee),(0xe5fb0,0xe5fb6),(0x6b040,0x6b097),(0x6aca0,0x6aceb),(0xe8ee0,0xe8ee7))
lines=[]
for lo,hi in ranges:
 lines.append(f'\n{lo:08X}..{hi:08X}')
 lines.extend(f'{i.address:08X} {i.bytes.hex():24} {i.mnemonic} {i.op_str}' for i in md.disasm(image[lo:hi],lo))
constants={f'{va:08X}':struct.unpack_from('<f',image,va)[0] for va in (0x15d334,0x15d438,0x15d47c)}
lines.append('\nFloat constants: '+json.dumps(constants))
out='\n'.join(lines)+'\n'
(root/'analysis/checkpoint-49/pal-timing.txt').write_text(out)
print(out)
