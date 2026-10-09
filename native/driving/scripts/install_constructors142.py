"""Install the checked missing constructor and SEH frame publication repair."""
from pathlib import Path
import hashlib, json
root = Path(__file__).resolve().parents[1]
candidate = root / 'analysis/constructors142/candidate.c'
expected = 'b9f021cddb675764d6c550bbdf1647ff6666c42a1682e8d57405ae1194362e63'
assert hashlib.sha256(candidate.read_bytes()).hexdigest() == expected
report = json.loads((candidate.parent / 'comparison-1.json').read_text())
assert report['source_sha256'] == expected and report['matches'] == report['cases'] == 136
gen = root / 'src/recomp/gen'
(gen / 'driving_constructors142.c').write_bytes(candidate.read_bytes())
path = gen / 'recomp_0011.c'
text = path.read_text(encoding='utf-8')
start = text.index('void sub_00134424(void)')
end = text.index('\n}\n', start) + 3
body = text[start:end]
old = 'g_seh_ebp = ebp; esp += 4; return; /* ret */'
assert body.count(old) == 1
body = body.replace(old, 'g_ebp = ebp; g_seh_ebp = ebp; esp += 4; return; /* publish new SEH frame */')
path.write_text(text[:start] + body + text[end:], encoding='utf-8')
print('Installed constructor2BD40 and checked SEH prolog134424 publication')
