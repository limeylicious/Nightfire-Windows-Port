"""Differential production observer events, enabled watch and context faults."""
from pathlib import Path
import os,subprocess,json
root=Path(__file__).resolve().parents[1]
exe=root/'build-windows/runtime/renderer/RelWithDebInfo/nightfire_observer75_test.exe'
out=root/'analysis/checkpoint-75';rows=[]
for watch in (None,'0','1'):
 for fault in (0,1,2):
  results=[]
  for mode in ('0','1'):
   env={k.upper():v for k,v in os.environ.items()}
   for key in ('NIGHTFIRE_VIDEO_WATCH','NIGHTFIRE_RENDER_RETURN_WATCH','NIGHTFIRE_INPUT_TEST'):env.pop(key,None)
   env['NIGHTFIRE_VIDEO_DISPATCH']=mode
   if watch is not None:env['NIGHTFIRE_VIDEO_WATCH']=watch
   result=subprocess.run([str(exe),str(fault)],env=env,cwd=root,capture_output=True)
   name=f'watch-{watch}-fault-{fault}-dispatch-{mode}'
   (out/(name+'.stdout')).write_bytes(result.stdout);(out/(name+'.stderr')).write_bytes(result.stderr)
   assert result.returncode==(2 if fault else 0),(name,result.stderr[-1500:])
   results.append(result)
  assert results[0].stdout==results[1].stdout,(watch,fault,'stdout')
  assert results[0].stderr==results[1].stderr,(watch,fault,'stderr')
  if watch is not None:assert b'00ABC123/1 and 00ABC123/2' in results[0].stderr
  rows.append(dict(watch=watch,fault=fault,exact_output=True,exit=results[0].returncode))
(out/'observer-comparisons.json').write_text(json.dumps(rows,indent=2))
print('PASS nine differential cases /18 processes: exact callbacks, diagnostics, history and state hashes; watch disabled/present0/present1, unarmed/armed, phases0/1/2 and deliberate context faults.')
