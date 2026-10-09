"""Refresh the test-only math experiment; game compilation remains unchanged."""
from pathlib import Path
import re,hashlib,json
root=Path(__file__).resolve().parents[1]
addresses=(0xd4e50,0xd50f0,0xd7340,0xd7370)
def extract(text,va):
 m=re.search(r'/\*\*\n \* sub_%08X\n.*?\nvoid sub_%08X\(void\)\n\{.*?\n\}'%(va,va),text,re.S)
 if not m:raise ValueError(hex(va))
 return m.group()
if __name__=='__main__':
 source=root/'src/recomp/gen/recomp_0005.c';text=source.read_text()
 bodies=[extract(text,va) for va in addresses]
 for body in bodies:assert 'RECOMP_ABI_CALL' not in body and 'goto ' not in body
 result='#define RECOMP_GENERATED_CODE\n#include "../src/recomp/gen/recomp_funcs.h"\n\n'+'\n\n'.join(bodies)+'\n'
 (root/'tests/math48_functions.inc').write_text(result)
 # Fixture directly includes production source, compiled with each flag set.
 report=[dict(address=f'{va:08X}',body_sha256=hashlib.sha256(b.encode()).hexdigest()) for va,b in zip(addresses,bodies)]
 (root/'analysis/checkpoint-48/math-bodies.json').write_text(json.dumps(report,indent=2))
