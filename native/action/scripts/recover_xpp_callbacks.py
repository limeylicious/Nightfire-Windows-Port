"""Recover five callback entries proven by the XBE's XPP registration table."""
import hashlib
import json
import sys
from pathlib import Path

root = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(root.parent / 'xboxrecomp'))
from tools.recomp import config
from tools.recomp.translator import FunctionTranslator

path = root / 'game_files/default.xbe'
binary = path.read_bytes()
assert hashlib.sha256(binary).hexdigest() == 'b464b787250ffc0ea7443a0e0848689c03a6cdaf6eb593fa0dba509bd2641aa1'
config.configure_from_xbe(str(path))
functions = json.loads((root / 'analysis/functions.json').read_text())
db = {int(f['start'], 16): dict(f, end=int(f['end'], 16)) for f in functions}
# Each bound is the exclusive end of the callback's final RET 4.
bounds = {0x157334: 0x15753d, 0x157c64: 0x157ccc,
          0x1588a0: 0x1588db, 0x158e8e: 0x158fe2, 0x15b5d5: 0x15b6b7}
for start, end in bounds.items():
    db[start] = dict(start=hex(start), end=end, name=f'sub_{start:08X}',
                     size=end-start, section='XPP', detection_method='xbe_callback_table')
t = FunctionTranslator(binary, db)
output = ['#define RECOMP_GENERATED_CODE\n#include "recomp_funcs.h"\n#include "../../nightfire_diagnostics.h"\n']
for start, end in bounds.items():
    ins, tables, targets = t._recover_cfg(start, end, set(), set(db)-{start})
    assert ins[-1].mnemonic == 'ret' and ins[-1].address + ins[-1].size == end
    assert not tables
    for i in ins:
        if i.mnemonic == 'call' and i.op_str.startswith('0x'):
            assert int(i.op_str, 16) in db, i.op_str
    t._recovered_cfg[start] = dict(end=end, instructions=ins, jump_tables=tables)
    code = t.translate_function(start, db[start]).replace('uint32_t ebp;', 'uint32_t ebp = g_ebp;')
    # LEAVE restores the caller's frame in both local and shared guest state.
    code = code.replace('POP32(esp, ebp); /* leave */', 'POP32(esp, ebp); /* leave */\n    g_ebp = ebp;\n    g_seh_ebp = ebp;')
    assert 'TODO' not in code
    output.append(code)
    print(f'{start:08X}: {len(ins)} instructions, ends {end:08X}')
(root / 'src/recomp/gen/nightfire_xpp_callbacks.c').write_text('\n'.join(output))
manual = root / 'src/recomp_manual.c'
text = manual.read_text()
a = text.index('recomp_func_t recomp_lookup_manual(')
b = text.index('\nvoid nightfire_diagnostic_stop', a)
decls = '\n'.join(f'extern void sub_{start:08X}(void);' for start in bounds)
lookup = '\nrecomp_func_t recomp_lookup_manual(uint32_t address)\n{\n    switch (address) {\n'
lookup += ''.join(f'    case 0x{start:08X}u: return sub_{start:08X};\n' for start in bounds)
lookup += '    default: return NULL;\n    }\n}\n'
# Replace previous generated declarations too when rerunning.
previous = text.find('extern void sub_00157334(void);')
if previous >= 0:
    a = previous
manual.write_text(text[:a] + decls + lookup + text[b:])
