"""Re-translate one guest function with the pinned toolkit over an explicit address range.

This is the method the Action tree's recover_*.py scripts used: the function database is
the stored function list with every entry strictly inside (start, upper) removed, the
control-flow graph is recovered up to `upper`, and the function is translated with the
end found by that recovery. Nothing here contains translated code; it only drives the
toolkit, which reads the player's own default.xbe.
"""
import json
from pathlib import Path

_CACHE = {}


def load_db(functions_json):
    key = str(functions_json)
    if key not in _CACHE:
        _CACHE[key] = {int(f['start'], 16): dict(f, end=int(f['end'], 16))
                       for f in json.loads(Path(functions_json).read_text())}
    return _CACHE[key]


def relift(xbe_bytes, functions_json, start, upper, strip_interior=True, label=None):
    """Return (code, end, instruction_count, table_count)."""
    from tools.recomp.translator import FunctionTranslator
    original = load_db(functions_json)
    if upper is None:
        upper = min(a for a in original if a > start)
    if strip_interior:
        db = {a: dict(f) for a, f in original.items() if not start < a < upper}
    else:
        db = {a: dict(f) for a, f in original.items()}
    db[start] = dict(start=hex(start), end=upper, name=label or f'sub_{start:08X}', size=upper - start, section='game')
    t = FunctionTranslator(xbe_bytes, db)
    ins, tables, _ = t._recover_cfg(start, upper, set(), set(db) - {start})
    end = max(i.address + i.size for i in ins)
    t._recovered_cfg[start] = dict(end=end, instructions=ins, jump_tables=tables)
    code = t.translate_function(start, dict(db[start], end=end))
    return code, end, len(ins), len(tables)
