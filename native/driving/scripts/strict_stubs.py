"""Make unresolved generated routines diagnostic stops instead of silent no-ops.

Run after generation and preserve a recovery snapshot before changing output.
This does not implement or skip original game behavior.
"""
from pathlib import Path
import re
root = Path(__file__).resolve().parents[1]
path = root / 'src/recomp/gen/recomp_stubs_unresolved.c'
if path.exists():
    source = path.read_text(encoding='utf-8')
    source, count = re.subn(r'(void \w+\(void\) \{) g_esp \+= \d+; /\* 0x([0-9A-Fa-f]+): ([^*]+)\*/ \}',
        lambda m: f'{m[1]} recomp_icall_fail_log(0x{m[2]}u); /* Missing original routine: {m[3].strip()} */ }}', source)
    if count:
        source = 'extern void recomp_icall_fail_log(unsigned int va);\n' + source
        path.write_text(source, encoding='utf-8')
    print(f'{count} unresolved routines now fail closed')
