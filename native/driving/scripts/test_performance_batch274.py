"""Combined depth272, drain permission274 and lane scheduling275 comparison.

Usage: python scripts/test_performance_batch274.py LABEL on|off [SECONDS]
Retains262..267, disables268/269; development direct-Paris shortcut only.
"""
from pathlib import Path
import os
import runpy
import sys

if len(sys.argv) not in (3,4) or sys.argv[2] not in ('on','off'):
    raise SystemExit(__doc__)
label,mode=sys.argv[1:3]
seconds=sys.argv[3] if len(sys.argv)==4 else '120'
for name in ('DRIVING_DEPTH_PING272','DRIVING_DRAIN_MAP274','DRIVING_NO_LANE_FLUSH275'):
    os.environ[name]='1' if mode=='on' else '0'
sys.argv=[str(Path(__file__).with_name('test_performance_batch269.py')),label,'off',seconds]
runpy.run_path(sys.argv[0],run_name='__main__')
