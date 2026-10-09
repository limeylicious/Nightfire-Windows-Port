"""Combined private validation267 / GPU depth import268 comparison.

Usage: python scripts/test_performance_batch268.py LABEL on|off [SECONDS]
Retains the verified262..265 foundation in BOTH modes. Development shortcut only.
"""
from pathlib import Path
import os
import runpy
import sys

if len(sys.argv) not in (3,4) or sys.argv[2] not in ('on','off'):
    raise SystemExit(__doc__)
label,mode=sys.argv[1:3]
seconds=sys.argv[3] if len(sys.argv)==4 else '120'
for name in ('DRIVING_VALIDATION267','DRIVING_DEPTH_IMPORT268'):
    os.environ[name]='1' if mode=='on' else '0'
sys.argv=[str(Path(__file__).with_name('test_performance_batch264.py')),label,'on',seconds]
runpy.run_path(sys.argv[0],run_name='__main__')
