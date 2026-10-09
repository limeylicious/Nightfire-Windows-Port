"""Reapply only verified AC97/DSP write boundaries after regeneration; dry-run default."""
from pathlib import Path
import hashlib, struct, sys
root=Path(__file__).resolve().parents[1]
raw=(root.parent/'nightfire-port/game_files/Driving.xbe').read_bytes()
assert hashlib.sha256(raw).hexdigest()=='0f4c50e4f84b1edeedab4667933b36c9c228da463cc89180e4c70cf3a88bfcef'
base=struct.unpack_from('<I',raw,0x104)[0]
count,table=struct.unpack_from('<II',raw,0x11C)
def original(va,n):
    for i in range(count):
        _,start,size,offset,length=struct.unpack_from('<5I',raw,table-base+i*56)
        if start<=va and va+n<=start+min(size,length):
            return raw[offset+va-start:offset+va-start+n]
    raise AssertionError(hex(va))
assert original(0x17C8ED,2)==bytes.fromhex('8903')
assert original(0x18213A,7)==bytes.fromhex('c6800b01c0fe02')
assert original(0x183214,8)==struct.pack('<II',0x10,0x70)
path=root/'src/recomp/gen/recomp_0018.c'
old=path.read_bytes();text=old.decode('utf-8').replace('\r\n','\n')
patches=[(
    '    MEM8(eax + -20971253) = 2;',
    '    extern void driving_ac97_write148(unsigned int,unsigned int);\n    driving_ac97_write148(eax + 0xFEC0010Bu,2);'),(
    '    MEM32(ebx) = eax;\n\nloc_0017C8EF: ;',
    '    MEM32(ebx) = eax;\n    { extern void driving_dsp_command149(unsigned int,unsigned int);\n      driving_dsp_command149(MEM32(ebp - 20),ebx); } /* original17C8ED diagnostic handshake */\n\nloc_0017C8EF: ;')]
for before,after in patches:
    if after in text:assert text.count(after)==1
    else:
        assert text.count(before)==1, before
        text=text.replace(before,after)
if '--apply' in sys.argv and text!=old.decode('utf-8').replace('\r\n','\n'):
    folder=root/'analysis/audio-boundaries149';folder.mkdir(exist_ok=True)
    backup=folder/('before-'+hashlib.sha256(old).hexdigest()+'.c.txt')
    if not backup.exists():backup.write_bytes(old)
    path.write_text(text,encoding='utf-8')
    print('Applied original AC97/DSP write boundaries; rebuild required.')
else: print('Verified PAL bytes and both boundary hooks; no source changes.')
