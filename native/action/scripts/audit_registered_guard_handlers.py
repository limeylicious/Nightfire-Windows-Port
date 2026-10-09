"""Read-only PAL guard state-table inventory; registration is not correctness proof."""
from pathlib import Path
import sys,re,struct,json
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'analysis/checkpoint-35'))
from disassemble import image
# PAL ProcessStateMachine4E180 bounds stateID<FA and indexes177CA0+ID*4.
dispatch=(root/'src/recomp/gen/recomp_dispatch.c').read_text()
manual=(root/'src/recomp_manual.c').read_text()
registered={int(x,16) for x in re.findall(r'\{ 0x([0-9A-F]+)u, \(recomp_func_t\)',dispatch)}
registered|={int(x,16) for x in re.findall(r'case 0x([0-9A-F]+)u: return sub_',manual)}
labels={int(r['address'],16):r['research_name'] for r in json.loads((root/'analysis/nightfire-research-labels.json').read_text())['entries']}
rows=[]
for state in range(0xfa):
 va=struct.unpack_from('<I',image,0x177ca0+state*4)[0]
 rows.append(dict(state=f'{state:02X}',address=f'{va:08X}',registered=va in registered,label=labels.get(va)))
out=root/'analysis/checkpoint-40';out.mkdir(exist_ok=True)
(out/'registered-guard-handlers.json').write_text(json.dumps(rows,indent=2))
print('Unregistered PAL guard table entries:')
print(json.dumps([r for r in rows if not r['registered']],indent=2))
