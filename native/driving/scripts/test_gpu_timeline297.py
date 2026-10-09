"""One bounded, initialized Paris GPU timeline; not an FPS benchmark.

Usage: python scripts/test_gpu_timeline297.py LABEL BATCH [off|both] [SECONDS]
The optional 'both' mode retains294/295 to inspect that exact path. Default off.
All gameplay/presentation initialization comes from the verified296 driver.
"""
from pathlib import Path
import os
import runpy
import sys

if len(sys.argv) not in (3, 4, 5):
    raise SystemExit(__doc__)
label, ordinal = sys.argv[1:3]
mode = sys.argv[3] if len(sys.argv) > 3 else 'off'
seconds = sys.argv[4] if len(sys.argv) > 4 else '90'
assert ordinal.isdigit() and 1 <= int(ordinal) <= 1000000
assert mode in ('off', 'both')
os.environ['DRIVING_GPU_SAMPLE297'] = '1'
os.environ['DRIVING_GPU_SAMPLE297_BATCH'] = ordinal
os.environ['DRIVING_GPU_SAMPLE254'] = '0'
sys.argv = [str(Path(__file__).with_name('test_performance_batch296.py')), label, mode, seconds]
runpy.run_path(sys.argv[0], run_name='__main__')
