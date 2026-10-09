"""Install the PAL-checked graphics initializer's diagnostic port adapter."""
from pathlib import Path
import hashlib, json
root = Path(__file__).resolve().parents[1]
candidate = root / 'analysis/graphics-init142/candidate.c'
expected = '0779bcd68a2fa41eb6fe7b83f59cccd24849bb329c09a1f0be129f0f657e5b4f'
assert hashlib.sha256(candidate.read_bytes()).hexdigest() == expected
report = json.loads((candidate.parent / 'comparison.json').read_text())
assert report['source_sha256'] == expected
assert report['matches'] == report['cases'] == report['planned'] == 40
gen = root / 'src/recomp/gen'
source = gen / 'recomp_0017.c'
text = source.read_text(encoding='utf-8')
anchor = 'void sub_0016FCAD(void)'
if anchor in text:
    start = text.index(anchor)
    end = text.index('\n}\n', start) + 3
    recovery = json.loads((candidate.parent / 'recovery.json').read_text())
    assert hashlib.sha256(text[start:end].encode()).hexdigest() == recovery['retained_sha256']
    assert text.count(anchor) == 1
    source.write_text(text.replace(anchor, 'void sub_0016FCAD_retained142(void)'), encoding='utf-8')
else:
    assert 'void sub_0016FCAD_retained142(void)' in text
(gen / 'driving_graphics_init142.c').write_bytes(candidate.read_bytes())
print('Installed checked16FCAD port output adapter; prior body retained')
