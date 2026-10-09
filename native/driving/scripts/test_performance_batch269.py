"""Private depth-work elimination269 comparison.

Usage: python scripts/test_performance_batch269.py LABEL on|off [SECONDS]
Retains262..267, keeps GPU import268 off. Development shortcut only.
"""
from pathlib import Path
import os
import runpy
import sys

if len(sys.argv) not in (3,4) or sys.argv[2] not in ('on','off'):
    raise SystemExit(__doc__)
label,mode=sys.argv[1:3]
seconds=sys.argv[3] if len(sys.argv)==4 else '120'
os.environ['DRIVING_DEPTH_CLEAN269']='1' if mode=='on' else '0'
os.environ['DRIVING_VALIDATION267']='1'
os.environ['DRIVING_DEPTH_IMPORT268']='0'
sys.argv=[str(Path(__file__).with_name('test_performance_batch264.py')),label,'on',seconds]
runpy.run_path(sys.argv[0],run_name='__main__')
