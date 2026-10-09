"""Grouped262..265 direct-Paris comparison, with the same retained foundation.

Usage: python scripts/test_performance_batch264.py LABEL on|off [SECONDS]
Each invocation uses the existing build/run scripts and saves its own evidence.
This is a development shortcut and diagnostic cadence, not a user playtest.
"""
from pathlib import Path
import os
import runpy
import sys

if len(sys.argv) not in (3, 4) or sys.argv[2] not in ('on', 'off'):
    raise SystemExit(__doc__)
label, mode = sys.argv[1:3]
seconds = int(sys.argv[3]) if len(sys.argv) == 4 else 120
if not 30 <= seconds <= 180:
    raise SystemExit('Use a bounded30..180 second diagnostic.')
for name in ('DRIVING_DESCRIPTOR252', 'DRIVING_OWNED253', 'DRIVING_DEPTH_SRV256',
             'DRIVING_MAPPING259', 'DRIVING_BATCH_CENSUS255', 'DRIVING_BARRIER242',
             'DRIVING_BATCH32_260', 'DRIVING_HOST_SPANS261'):
    os.environ[name] = '1'
os.environ['DRIVING_GPU_SAMPLE254'] = '0'
os.environ.pop('DRIVING_GPU_SAMPLE254_BATCH', None)
for name in ('DRIVING_COMMAND262', 'DRIVING_UNHANDLED263', 'DRIVING_BATCH_MAP264',
             'DRIVING_COMMAND_READ265'):
    os.environ[name] = '1' if mode == 'on' else '0'
sys.argv = [str(Path(__file__).with_name('test_scene_profile250.py')), label, str(seconds)]
runpy.run_path(sys.argv[0], run_name='__main__')
