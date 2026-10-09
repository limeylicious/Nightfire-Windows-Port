"""Install the frozen, 1,838-case PAL-checked complete overlap-copy routine."""
from pathlib import Path
import hashlib, json
root = Path(__file__).resolve().parents[1]
candidate = root / 'analysis/copy142/candidate.c'
expected = 'ada88ccf0654cca096796c29f3ea70cfbfd94b240162a511329aabb06b0d551e'
assert hashlib.sha256(candidate.read_bytes()).hexdigest() == expected
report = json.loads((candidate.parent / 'comparison-candidate.json').read_text())
assert report['source_sha256'] == expected and report['matches'] == report['planned'] == 1838
generated = root / 'src/recomp/gen/recomp_0011.c'
text = generated.read_text(encoding='utf-8')
old = 'void sub_00132270(void)'
assert text.count(old) == 1
generated.write_text(text.replace(old, 'void sub_00132270_retained142(void)'), encoding='utf-8')
(generated.parent / 'driving_copy142.c').write_bytes(candidate.read_bytes())
print('Installed checked copy132270; existing callers and dispatch resolve the full routine')
