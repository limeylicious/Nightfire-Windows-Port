"""Byte-verified repairs for three XMV control-flow joins in the PAL image."""
from pathlib import Path
import hashlib
r=Path(__file__).resolve().parents[1]
b=(r/'game_files/default.xbe').read_bytes()
assert hashlib.sha256(b).hexdigest()=='b464b787250ffc0ea7443a0e0848689c03a6cdaf6eb593fa0dba509bd2641aa1'
p=r/'src/recomp/gen/recomp_0008.c';s=p.read_text()
s=s.replace('if (_flags /* je: equal / zero */) goto loc_001318EF;',
'''if (eax == 0) goto loc_001318EF; /* ZF from initial SUB or loop DEC */''')
for jump,compare in [('0013115C','00131156'),('00131A71','00131A6B')]:
 marker='    goto loc_'+jump+';'
 if '    _flags = TEST_Z(_fa, _fb); /* preserve incoming TEST ZF */\n'+marker not in s:
  assert s.count(marker)==1
  s=s.replace(marker,'    _flags = TEST_Z(_fa, _fb); /* preserve incoming TEST ZF */\n'+marker)
 marker='\nloc_'+jump+': ;'
 if '    _flags = CMP_EQ(_fa, _fb); /* preserve incoming CMP ZF */\n'+marker not in s:
  assert s.count(marker)==1
  s=s.replace(marker,'    _flags = CMP_EQ(_fa, _fb); /* preserve incoming CMP ZF */\n'+marker)
p.write_text(s)
print('Repaired row-fill termination and two signed-coefficient control-flow joins.')
