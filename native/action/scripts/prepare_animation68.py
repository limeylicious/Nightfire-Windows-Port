"""Extract production animation bodies for one bounded PAL comparison batch."""
from pathlib import Path
import re
root=Path(__file__).resolve().parents[1]
addresses={0x12b40:0,0x24000:0,0xd5e40:5,0xd5e90:5}
def extract():
    bodies=[]
    for va,unit in addresses.items():
        text=(root/f'src/recomp/gen/recomp_{unit:04}.c').read_text()
        start=text.index(f'void sub_{va:08X}(void)\n{{')
        end=text.index('\n}\n',start)+3
        bodies.append(text[start:end])
    return '\n'.join(bodies)
if __name__=='__main__':
    text=extract();(root/'tests/animation68_functions.inc').write_text(text)
    print('Calls',sorted(set(re.findall(r'RECOMP_ABI_CALL\((0x[0-9A-F]+)',text))))
