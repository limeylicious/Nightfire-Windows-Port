"""Reproduce the byte-backed CFG repair for Nightfire's CRT copy routine."""
import hashlib
import json
import struct
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
start, end = 0xee4b0, 0xee7ed
database = {}
for f in functions:
    address = int(f['start'], 16)
    if address == start:
        f.update(end=hex(end), size=end-start, detection_method='nightfire_verified_memmove_boundary')
    database[address] = dict(f, end=int(f['end'], 16))
(root / 'analysis/functions.json').write_text(json.dumps(functions, indent=2))
t = FunctionTranslator(binary, database)
instructions, tables, targets = t._recover_cfg(start, end, set(), set())
# The alignment branches use indices 1..3: the nominal base isn't an entry.
for table in (0xee510, 0xee69c):
    tables[table] = list(struct.unpack_from('<3I', binary, table-0x10000+4))
seeds = {start} | {target for table in tables.values() for target in table}
assert all(start <= target < end for target in seeds)
instructions = t.disasm.disassemble_cfg(binary[start-0x10000:end-0x10000], start, end, seeds)
t._recovered_cfg[start] = dict(end=end, instructions=instructions, jump_tables=tables)
code = t.translate_function(start, database[start]).replace('uint32_t ebp;', 'uint32_t ebp = g_ebp;')
assert 'TODO' not in code
(root / 'analysis/memmove-cfg.c').write_text(code)
chunk = root / 'src/recomp/gen/recomp_0006.c'
text = chunk.read_text()
a = text.index('void sub_000EE4B0(void)\n')
b = text.index('\n/**', a)
chunk.write_text(text[:a] + code[code.index('void sub_000EE4B0(void)\n'):] + '\n' + text[b:])
print('Recovered copy routine with seven jump tables; rerun its regression test.')
