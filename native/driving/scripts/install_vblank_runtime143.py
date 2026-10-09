"""Scope the existing runtime IRQ delivery to enabled PCRTC vblank requests."""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
p=root/'runtime/kernel_bridge.c';t=p.read_text(encoding='utf-8')
marker='/* VBLANK143: only deliver an enabled PCRTC source. */'
if marker not in t:
    anchor='    if (!xbox_GetConnectedInterrupt(NV2A_VECTOR))\n        return;'
    assert t.count(anchor)==1
    t=t.replace(anchor,anchor+'\n\n    '+marker+'\n    if (!(BRIDGE_MEM32(XBOX_NV2A_REG_BASE + 0x600140u) & 1u)\n        || !(BRIDGE_MEM32(XBOX_NV2A_REG_BASE + 0x140u) & 1u)) return;')
    t=t.replace('    next_ms = now + 16;                       /* ~60 Hz */',
                '    next_ms = now + 20; /* Diagnostic 50Hz; mode/field-accurate timing remains pending. */')
    p.write_text(t,encoding='utf-8')
print('Installed enabled-source vblank gating; 20ms diagnostic tick, not display timing proof')
