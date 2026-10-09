"""Summarize saved native evidence; presentation cadence is not simulation speed.

Usage: python scripts/summarize_scene_run.py analysis/runs/NAME
Writes scene-summary.json within that run. Does not launch or alter the game.
"""
from pathlib import Path
import json
import re
import sys

run = Path(sys.argv[1]).resolve()
log = (run / 'driving-startup.log').read_text(encoding='utf-8', errors='replace')
result = json.loads((run / 'result.json').read_text(encoding='utf-8'))
environment = json.loads((run / 'diagnostic-environment.json').read_text(encoding='utf-8'))
intervals = [dict(frame=int(f), elapsed_ms=int(e), last30_ms=int(t),
                  presentations_per_second=round(30000 / int(t), 3) if int(t) else None)
             for f, e, t in re.findall(r'\[PRESENT227\] frame=(\d+) elapsed_ms=(\d+) last30_ms=(\d+)', log)]
markers = ('BATCH236-TIME', 'GPU243', 'BATCH244-WALL', 'DEPTH244', 'COMMAND245', 'COMMAND262', 'BATCH-MAP264', 'COMMAND-READ265', 'VALIDATION267', 'DEPTH-IMPORT268', 'DEPTH-CLEAN269', 'DEPTH-PING272', 'DRAIN-MAP274', 'LANE-FLUSH275', 'COLOR-SEED276', 'NATIVE-INDEXED277', 'DEPTH-SEED278', 'BEGIN280', 'FRAGMENT291', 'SEMAPHORE281', 'NATIVE-PROFILE277', 'MATERIAL-RING246', 'NATIVE247', 'PAIR-BATCH248', 'TIMING250', 'OFFSCREEN250', 'SOFTWARE250', 'QUEUE251', 'DESCRIPTOR252', 'OWNED253', 'GPU-SAMPLE254', 'DEPTH-SRV256', 'MAPPING259', 'BARRIER242', 'HOST-SPANS261')
markers += ('READBACK294', 'ASYNC295', 'GPU297', 'GPU297-SUM', 'NATIVE-DEPTH302')
last = {}
for line in log.splitlines():
    for marker in markers:
        if line.startswith('[' + marker + ']'):
            bucket = (re.search(r'\bresult=(accepted|refused)\b', line) if marker == 'QUEUE251' else
                      re.search(r'\bprofile=(\d+)\b', line) if marker == 'NATIVE-PROFILE277' else None)
            last[marker + ('-' + bucket.group(1) if bucket else '')] = line
report = dict(result=result, environment=environment, presentation_intervals=intervals,
              final_diagnostics=last,
              captures=[p.name for p in sorted(run.glob('gpu201-frame*.bmp'))],
              limitations='Cadence is recorded presentation throughput, not game speed. '
                           'First interval includes initial fade/loading. '
                           'Different runs are not matched trials unless inputs/state are controlled. '
                           'Diagnostic timings include CPU work and waits, not isolated GPU duration.')
(run / 'scene-summary.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
print(json.dumps(report, indent=2))
