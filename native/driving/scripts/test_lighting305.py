"""Bounded Paris visual check for the grouped301/304 original-material repairs.

Usage: python scripts/test_lighting305.py LABEL [SECONDS]
Retains the existing optimized depth path; no experimental302 or RAM capture.
"""
from pathlib import Path
import os
import runpy
import sys

if len(sys.argv) not in (2, 3):
    raise SystemExit(__doc__)
label = sys.argv[1]
seconds = sys.argv[2] if len(sys.argv) == 3 else '180'
os.environ['DRIVING_LIGHTING301'] = '1'
os.environ['DRIVING_LIGHTING304'] = '1'
for name in ('DRIVING_LIGHTING_CAPTURE298', 'DRIVING_GPU_SAMPLE297',
             'DRIVING_GPU_SAMPLE254', 'DRIVING_NATIVE_DEPTH302',
             'DRIVING_WORLD_CAPTURE223'):
    os.environ[name] = '0'
sys.argv = [str(Path(__file__).with_name('test_performance_batch296.py')), label, 'off', seconds]
runpy.run_path(sys.argv[0], run_name='__main__')
