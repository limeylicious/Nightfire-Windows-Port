"""Preserve the full observed original first-Paris payload and its provenance."""
from pathlib import Path
import hashlib,json,shutil
root=Path(__file__).resolve().parents[1]
source=root/'analysis/launch-capture144/runs/20260914-111653'
meta=json.loads((source/'capture.json').read_text(encoding='utf-8'))
data=(source/'payload-a50.bin').read_bytes()
assert len(data)==0xa50
assert hashlib.sha256(data).hexdigest()==meta['payload_sha256']=='1d84df65b098f641e952bbb13481d34564deac9bb3e978f3ab0834dc6538c7d3'
assert meta['level']=='09000001' and meta['profile_mode']==2 and meta['mini_mission_switch']==1
dest=root/'launch-data';dest.mkdir(exist_ok=True)
p=dest/'original-first-paris144.bin'
if p.exists():assert p.read_bytes()==data
else:p.write_bytes(data)
meta['capture_directory']=str(source.relative_to(root))
meta['scope']='Full original serializer output captured before action shutdown. Fresh opening Paris; not later mission or carried progression.'
(dest/'original-first-paris144.json').write_text(json.dumps(meta,indent=2),encoding='utf-8')
print(p)
