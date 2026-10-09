"""Install the two PAL-checked missing XPP initializers, without bypassing them."""
from pathlib import Path
import hashlib, json
root = Path(__file__).resolve().parents[1]
candidate = root / 'analysis/xpp142/candidate.c'
expected = '51a2458d922a2d6914c052cce97e9242d45c6a692bbecfdd4d69a8b755d29c96'
assert hashlib.sha256(candidate.read_bytes()).hexdigest() == expected
report = json.loads((candidate.parent / 'comparison.json').read_text())
assert report['source_sha256'] == expected
assert report['matches'] == report['cases'] == report['planned'] == 157
manual = root / 'src/recomp_manual.c'
text = manual.read_text(encoding='utf-8')
for va in ('00183BF4', '00184502'):
    declaration = f'extern void sub_{va}(void);'
    if declaration not in text:
        anchor = 'extern void sub_0002BD40(void);'
        assert text.count(anchor) == 1
        text = text.replace(anchor, anchor + '\n' + declaration)
    entry = f'    if (xbox_va == 0x{va}u) return sub_{va};'
    if entry not in text:
        anchor = 'recomp_func_t recomp_lookup_manual(uint32_t xbox_va)\n{'
        assert text.count(anchor) == 1
        text = text.replace(anchor, anchor + '\n' + entry)
(root / 'src/recomp/gen/driving_xpp142.c').write_bytes(candidate.read_bytes())
manual.write_text(text, encoding='utf-8')
print('Installed original XPP initializers183BF4 and184502 with dispatch')
