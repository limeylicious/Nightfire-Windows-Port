from pathlib import Path
import hashlib,json
root=Path(__file__).resolve().parents[1]
source=root/'analysis/blit143/driving_blit143.h'
assert hashlib.sha256(source.read_bytes()).hexdigest()=='165022af9690e7e207171a27aae94e95c72e8a3984eb2ce2ca597c9d5e5d9687'
report=json.loads((source.parent/'comparison.json').read_text())
assert report['cases']==report['matches']==161
assert report['header_sha256']==hashlib.sha256(source.read_bytes()).hexdigest()
(root/'runtime/driving_blit143.h').write_bytes(source.read_bytes())
print('Installed unchanged checked SRCCOPY helper; live adapter restricts to disjoint spans')
