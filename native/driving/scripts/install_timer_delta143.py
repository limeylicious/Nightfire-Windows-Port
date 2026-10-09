"""Install the reviewed PAL timer delta correction; not run during recovery.

Requires matching93-case original-XBE comparison and exact prior function body.
Does not replace any other routine, dispatcher, kernel or timer policy.
"""
from pathlib import Path
import hashlib,json
root=Path(__file__).resolve().parents[1]
out=root/'analysis/timer_delta143'
source=root/'src/recomp/gen/recomp_0010.c'
recovery=json.loads((out/'recovery.json').read_text())
comparison=json.loads((out/'comparison.json').read_text())
candidate=(out/'candidate.c').read_bytes()
digest=hashlib.sha256(candidate).hexdigest()
assert digest==comparison['candidate_sha256']==recovery['candidate_sha256']
assert comparison['cases']==93 and comparison['candidate_matches']==93
assert comparison['retained_matches']==86
text=source.read_text(encoding='utf-8')
signature='void sub_0010ECE5(void)\n{'
a=text.index(signature);z=text.index('\n}',a)+2
old=text[a:z]
# Validate the exact tested bytes above, then normalize only for source matching.
new=candidate.decode('utf-8').replace('\r\n','\n').replace('\r','\n')
new=new[new.index(signature):].rstrip('\n')
if old!=new:
    assert hashlib.sha256(old.encode()).hexdigest()==recovery['original_sha256']
    text=text[:a]+new+text[z:]
    source.write_text(text,encoding='utf-8')
print('Installed PAL10EE71 SBB flags for JG10EE76 in original10ECE5 only.')
