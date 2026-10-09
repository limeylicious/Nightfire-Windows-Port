"""Install the scoped guest submission boundary; preserve the pinned toolkit."""
from pathlib import Path
import hashlib,json
root=Path(__file__).resolve().parents[1]
source=root.parent/'xboxrecomp/src/kernel/xbox_memory_layout.c'
t=source.read_text()
anchor='            if (*get != *put) {\n                *get = *put;\n            }'
assert t.count(anchor)==1
t=t.replace(anchor,'            if (!getenv("DRIVING_PB_SYNC") && *get != *put) {\n                *get = *put;\n            }')
(root/'runtime/xbox_memory_layout.c').write_text(t)
p=root/'src/recomp/gen/recomp_0017.c';t=p.read_text()
decl='extern void driving_gpu_drain143(void);\n'
if decl not in t:t=decl+t
anchor='    MEM32(eax + 0x40) = edx;\n    eax = MEM32(0x17541C);'
new='    MEM32(eax + 0x40) = edx;\n    driving_gpu_drain143(); /* consume published commands before guest reuse */\n    eax = MEM32(0x17541C);'
if new not in t:
    assert t.count(anchor)==1;t=t.replace(anchor,new)
p.write_text(t)
(root/'analysis/gpu-consumer-provenance143.json').write_text(json.dumps({
 'toolkit_memory_layout_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),
 'consumer_reference':'nightfire-port/runtime/nv2a_pb_scan.c',
 'consumer_reference_sha256':hashlib.sha256((root.parent/'nightfire-port/runtime/nv2a_pb_scan.c').read_bytes()).hexdigest(),
 'method_reference':'xboxrecomp/src/nv2a/nv2a_regs.h',
 'boundary':'Original16C9A0 PUT store in16C940; host registers unchanged',
 'limits':'Synchronous experimental software executor, not complete renderer or hardware fence proof'
},indent=2))
print('Installed synchronous driving submission boundary and disabled early GET mirroring when enabled')
