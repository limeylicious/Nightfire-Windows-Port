"""Narrow register preservation plus explicit unsupported-instruction stops.

Apply only after the startup-abi142 PAL comparisons and a recovery snapshot.
Does not regenerate, replace assets or introduce action-engine address hooks.
"""
from pathlib import Path
import json, re
root = Path(__file__).resolve().parents[1]
header = root / 'src/recomp/gen/recomp_types.h'
text = header.read_text(encoding='utf-8')
if '#elif defined(DRIVING_CALL_HISTORY)' not in text:
    anchor = '#include "recomp_icall_feedback.h"\n#else'
    assert text.count(anchor) == 1
    text = text.replace(anchor, '#include "recomp_icall_feedback.h"\n#elif defined(DRIVING_CALL_HISTORY)\nvoid driving_trace_icall(uint32_t va);\n#define RECOMP_ICALL_OBSERVE(va, flags) driving_trace_icall(va)\n#else')
    header.write_text(text, encoding='utf-8')
counts = dict(ebp_entry=0, ebp_restore=0, unsupported=0)
for path in (root / 'src/recomp/gen').glob('*.c'):
    text = path.read_text(encoding='utf-8')
    before = text
    for address in ('0010E703', '0005A1B0', '00059920', '000596A0', '001306D0'):
        anchor = f'void sub_{address}(void)\n{{\n'
        if anchor in text and anchor + '    driving_trace_entry' not in text:
            text = text.replace(anchor, anchor + f'    driving_trace_entry(0x{address}u);\n')
    if '    driving_trace_entry(' in text and 'extern void driving_trace_entry' not in text:
        text = 'extern void driving_trace_entry(unsigned int);\n' + text
    counts['ebp_entry'] += text.count('uint32_t ebp;')
    text = text.replace('uint32_t ebp;', 'uint32_t ebp = g_ebp; /* preserve original incoming EBP */')
    pattern = r'POP32\(esp, ebp\);(?! g_ebp = ebp;)'
    text, n = re.subn(pattern, 'POP32(esp, ebp); g_ebp = ebp; g_seh_ebp = ebp; /* publish restored register */', text)
    counts['ebp_restore'] += n
    def stop(match):
        counts['unsupported'] += 1
        return 'driving_translation_stop(__func__, ' + json.dumps(match[1]) + ');'
    text = re.sub(r'/\* TODO: (.*?) \*/', stop, text)
    if 'driving_translation_stop(' in text and 'extern void driving_translation_stop' not in text:
        text = 'extern void driving_translation_stop(const char *, const char *);\n' + text
    if text != before:
        path.write_text(text, encoding='utf-8')
(root / 'analysis/hardening142.json').write_text(json.dumps(counts, indent=2), encoding='utf-8')
print(counts)
