from pathlib import Path
root=Path(__file__).resolve().parents[1]
p=root/'src/recomp/gen/recomp_0017.c'
t=p.read_text()
decl='extern void driving_gpu_probe143(unsigned int);\n'
if decl not in t:t=decl+t
for label in ('0016CA21','0016CBE0'):
    anchor=f'loc_{label}: ;\n'
    block=f'    {{ static int seen143; if (!seen143) {{ seen143=1; driving_gpu_probe143(0x{label}u); }} }}\n'
    assert t.count(anchor)==1
    if anchor+block not in t:t=t.replace(anchor,anchor+block)
p.write_text(t)
print('Installed two one-shot read-only GPU snapshots')
