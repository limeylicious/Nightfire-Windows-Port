"""Reuse host shared-memory ordering at the reached original WBINVD boundary."""
from pathlib import Path
import hashlib, json
root = Path(__file__).resolve().parents[1]
original = root.parent / 'nightfire-port/src/recomp/gen/nightfire_memory_order.c'
source = root / 'src/recomp/gen/recomp_0017.c'
text = source.read_text(encoding='utf-8')
start = text.index('void sub_0016D800(void)')
end = text.index('\n}\n', start) + 3
body = text[start:end]
old = 'driving_translation_stop(__func__, "wbinvd ");'
new = 'nightfire_memory_barrier(); /* WBINVD boundary: host ordering, not GPU completion */'
if old in body:
    assert body.count(old) == 1
    assert 'MEM32(-2147483648) = edx;\n    ' + old in body
    text = text[:start] + body.replace(old, new) + text[end:]
else:
    assert new in body
declaration = 'extern void nightfire_memory_barrier(void);\n'
if declaration not in text:
    text = declaration + text
source.write_text(text, encoding='utf-8')
(root / 'src/driving_memory_order.c').write_bytes(original.read_bytes())
(root / 'analysis/memory-order142.json').write_text(json.dumps(dict(
    source=str(original), sha256=hashlib.sha256(original.read_bytes()).hexdigest(),
    routine='0x0016D800', operation='WBINVD',
    behavior='Host shared-memory ordering only; not physical cache invalidation or GPU completion'
), indent=2), encoding='utf-8')
print('Installed original action host ordering helper at driving WBINVD boundary')
