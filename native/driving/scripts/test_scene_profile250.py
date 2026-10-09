"""Bounded direct-Paris attribution; not a clean FPS benchmark or user launcher."""
from pathlib import Path
import os
import runpy
import sys

label = sys.argv[1] if len(sys.argv) > 1 else 'scene-profile250'
seconds = sys.argv[2] if len(sys.argv) > 2 else '120'
assert len(sys.argv) <= 3
for name in ('DRIVING_ROAD_WHITE242', 'DRIVING_OFFSCREEN_GPU243',
             'DRIVING_COMMAND245', 'DRIVING_MATERIAL_RING246',
             'DRIVING_NATIVE247', 'DRIVING_PAIR_BATCH248',
             'DRIVING_BATCH_TIMING244', 'DRIVING_NATIVE_TIMING247',
             'DRIVING_TIMING250', 'DRIVING_QUEUE_TIMING251'):
    os.environ[name] = '1'
sys.argv = [str(Path(__file__).with_name('test_direct_paris240.py')),
            label, seconds, 'scene']
runpy.run_path(sys.argv[0], run_name='__main__')
