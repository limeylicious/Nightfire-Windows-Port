"""Make unsupported generated paths fail visibly during diagnostic bring-up."""
import json
import re
from pathlib import Path

root = Path(__file__).resolve().parents[1]
folder = root / 'src/recomp/gen'
counts = {'unsupported_instructions': 0, 'failed_functions': 0, 'unresolved_targets': 0}
for path in folder.glob('*.c'):
    text = path.read_text()
    text = text.replace("uint32_t ebp;", "uint32_t ebp = g_ebp; /* inherit guest frame before prologue saves it */")
    if 'nightfire_diagnostic_stop(' in text:
        raise RuntimeError('Already hardened; regenerate first')
    def unsupported(m):
        counts['unsupported_instructions'] += 1
        return 'nightfire_diagnostic_stop(__func__, ' + json.dumps(m[1]) + ');'
    text = re.sub(r'/\* TODO: (.*?) \*/', unsupported, text)
    def failed(m):
        counts['failed_functions'] += 1
        return 'void ' + m[1] + '(void) { nightfire_diagnostic_stop(__func__, "translation failed"); }'
    text = re.sub(r'void (\w+)\(void\) \{ /\* translation failed \*/ \}', failed, text)
    def unresolved(m):
        counts['unresolved_targets'] += 1
        return 'void ' + m[1] + '(void) { nightfire_diagnostic_stop(__func__, "unresolved call target"); }'
    text = re.sub(r'void (\w+)\(void\) \{[^\n]*?/\* 0x[0-9A-F]+: (?:not detected|ret \d+) \*/ \}', unresolved, text)
    text = '#include "../../nightfire_diagnostics.h"\n' + text
    path.write_text(text)
(root / 'analysis/hardening.json').write_text(json.dumps(counts, indent=2))
print(counts)
