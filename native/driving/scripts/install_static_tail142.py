"""Install the PAL-checked remaining callback from the original CRT tables."""
from pathlib import Path
import hashlib, json
root = Path(__file__).resolve().parents[1]
candidate = root / 'analysis/static-tail142/candidate.c'
expected = 'a901d560843e8b6deaade16c094f5611abbee9fca576565cccda711da477dbff'
assert hashlib.sha256(candidate.read_bytes()).hexdigest() == expected
report = json.loads((candidate.parent / 'comparison.json').read_text())
assert report['source_sha256'] == expected and report['matches'] == report['cases'] == 12
manual = root / 'src/recomp_manual.c'
text = manual.read_text(encoding='utf-8')
declaration = 'extern void sub_00158EF0(void);'
if declaration not in text:
    anchor = 'extern void sub_0002BD40(void);'
    assert text.count(anchor) == 1
    text = text.replace(anchor, anchor + '\n' + declaration)
entry = '    if (xbox_va == 0x00158EF0u) return sub_00158EF0;'
if entry not in text:
    anchor = 'recomp_func_t recomp_lookup_manual(uint32_t xbox_va)\n{'
    assert text.count(anchor) == 1
    text = text.replace(anchor, anchor + '\n' + entry)
(root / 'src/recomp/gen/driving_static_tail142.c').write_bytes(candidate.read_bytes())
manual.write_text(text, encoding='utf-8')
print('Installed complete original CRT initializer158EF0')
