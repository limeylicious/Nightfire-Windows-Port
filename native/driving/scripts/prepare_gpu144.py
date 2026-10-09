"""Preserve a small, attributed action-renderer foundation for driving integration.
This copies code only; it does not enable rendering or change the action build.
"""
from pathlib import Path
import hashlib,json
root=Path(__file__).resolve().parents[1]
source=root.parent/'nightfire-port/runtime';dest=root/'runtime/gpu144'
names=['nightfire_hardware.c','nightfire_hardware.h','nightfire_vertex_program.h',
 'nightfire_vertex_hlsl.h','nightfire_depth_transfer.h','nightfire_fog.h',
 'nightfire_bytes_equal.h','nightfire_constant_mask.h','nightfire_shader_cache99.h',
 'nightfire_rgba_mips129.h','nightfire_swizzled_shadow131.h','nightfire_surface_probe83.h',
 'nightfire_depth_observe83.h','nightfire_gpu_timing84.h']
payload={name:(source/name).read_bytes() for name in names}
dest.mkdir(exist_ok=True)
for name,data in payload.items():
    target=dest/name
    if target.exists():assert target.read_bytes()==data, f'Preserve locally adapted {target}'
    else:target.write_bytes(data)
(dest/'PROVENANCE.json').write_text(json.dumps({
 'source':'nightfire-port/runtime, existing project action renderer; notices preserved',
 'purpose':'Unlinked D3D11 foundation; no driving pixel/AA/scene correctness claim',
 'files':{name:hashlib.sha256(data).hexdigest() for name,data in payload.items()}
},indent=2),encoding='utf-8')
print(f'Prepared {len(payload)} unchanged action backend files for separate driving integration')
