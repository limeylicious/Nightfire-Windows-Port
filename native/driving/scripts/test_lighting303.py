"""Original Paris with the verified301 material and delayed fog capture.

Usage: python scripts/test_lighting303.py LABEL [SECONDS]
Resource captures make this a visual diagnostic, not an FPS benchmark.
"""
from pathlib import Path
import os
import runpy
import sys

if len(sys.argv) not in (2, 3):
    raise SystemExit(__doc__)
label = sys.argv[1]
seconds = sys.argv[2] if len(sys.argv) == 3 else '120'
os.environ['DRIVING_LIGHTING301'] = '1'
os.environ['DRIVING_LIGHTING_CAPTURE298'] = '1'
os.environ['DRIVING_GPU_SAMPLE297'] = '0'
os.environ['DRIVING_GPU_SAMPLE254'] = '0'
sys.argv = [str(Path(__file__).with_name('test_performance_batch296.py')), label, 'off', seconds]
runpy.run_path(sys.argv[0], run_name='__main__')
