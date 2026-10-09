"""Direct Paris comparison for the optional per-fragment depth experiment.

Usage: python scripts/test_performance_batch293.py LABEL optimized|depth-off|depth-on [SECONDS]
optimized retains the measured272/278 baseline. depth-off and depth-on both
use fresh depth import, differing only in291. All retain283/292 coverage.
Neither an initialized shortcut nor a speed gain proves original depth fidelity.
"""
from pathlib import Path
import os
import runpy
import sys

if len(sys.argv) not in (3, 4) or sys.argv[2] not in ('optimized', 'depth-off', 'depth-on'):
    raise SystemExit(__doc__)
label, mode = sys.argv[1:3]
seconds = sys.argv[3] if len(sys.argv) == 4 else '120'
for name in ('DRIVING_DRAIN_MAP274', 'DRIVING_NO_LANE_FLUSH275',
             'DRIVING_COLOR_SEED276', 'DRIVING_NATIVE_INDEXED277',
             'DRIVING_NATIVE_PROFILE277', 'DRIVING_BEGIN280',
             'DRIVING_ADMISSION283', 'DRIVING_MATERIAL292'):
    os.environ[name] = '1'
os.environ['DRIVING_SEMAPHORE281'] = '0'
os.environ['DRIVING_DEPTH_PING272'] = '1' if mode == 'optimized' else '0'
os.environ['DRIVING_DEPTH_SEED278'] = '1' if mode == 'optimized' else '0'
os.environ['DRIVING_FRAGMENT_DEPTH291'] = '1' if mode == 'depth-on' else '0'
for name in ('DRIVING_PALETTE_CAPTURE288', 'DRIVING_REMAINING_CAPTURE290'):
    os.environ.pop(name, None)
sys.argv = [str(Path(__file__).with_name('test_performance_batch269.py')),
            label, 'off', seconds]
runpy.run_path(sys.argv[0], run_name='__main__')
