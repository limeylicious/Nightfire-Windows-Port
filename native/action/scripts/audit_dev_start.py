"""Read-only PAL evidence for the development-only original loader entry."""
from pathlib import Path
import sys,struct,json
root=Path(__file__).resolve().parents[1]
sys.path[:0]=[str(root/'analysis/python-deps'),str(root/'analysis/checkpoint-35')]
from disassemble import image
from capstone import Cs,CS_ARCH_X86,CS_MODE_32
out=root/'analysis/checkpoint-45';out.mkdir(exist_ok=True)
md=Cs(CS_ARCH_X86,CS_MODE_32)
ranges=((0x6ad2d,0x6ad98),(0xbe080,0xbe0a1),(0xbde7d,0xbdea6),(0xbdece,0xbded8))
lines=[]
for start,end in ranges:
 lines.extend(f'{i.address:08X} {i.mnemonic} {i.op_str}' for i in md.disasm(bytes(image[start:end]),start))
entry=list(md.disasm(bytes(image[0xbe080:0xbe0a1]),0xbe080))
assert len(entry)==11 and entry[-1].mnemonic=='ret'
call=next(i for i in entry if i.mnemonic=='call')
assert call.address+call.size==0xbe09d and call.op_str=='0xbddf0'
slot=image[0xbe030+4];target=struct.unpack_from('<I',image,0xbdfcc+4*slot)[0]
assert target==0xbdece
movie=list(md.disasm(bytes(image[target:target+5]),target))[0]
assert movie.mnemonic=='mov' and movie.op_str=='esi, 0x7100005'
assert any(line=='000BDE83 jne 0xbdf5b' for line in lines)
(out/'pal-development-entry.txt').write_text('\n'.join(lines))
(out/'entry-evidence.json').write_text(json.dumps(dict(initial_caller='000BE09D',normal_menu='07000048',development_level='07000005',normal_movie='07100005',skip_movie_argument=3,engine_boot_before_request='0006AD2D..0006AD98',note='Only explicit development request changes loader arguments; original loader executes.'),indent=2))
print('PASS: PAL boot/load call site, return address, movie mapping, and skip-movie branch verified.')
