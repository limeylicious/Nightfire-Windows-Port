"""Capture two refused fog families and one larger GPU batch in a single run.

Usage: python scripts/test_lighting_timeline300.py LABEL [SECONDS]
Not a performance benchmark: resource captures copy RAM and write files.
"""
from pathlib import Path
import os
import runpy
import sys

if len(sys.argv) not in (2,3):
    raise SystemExit(__doc__)
label = sys.argv[1]
seconds = sys.argv[2] if len(sys.argv) == 3 else '90'
os.environ['DRIVING_LIGHTING_CAPTURE298'] = '1'
os.environ['DRIVING_GPU_SAMPLE297_MIN_DRAWS'] = '8'
sys.argv = [str(Path(__file__).with_name('test_gpu_timeline297.py')),label,'1024','off',seconds]
runpy.run_path(sys.argv[0],run_name='__main__')
