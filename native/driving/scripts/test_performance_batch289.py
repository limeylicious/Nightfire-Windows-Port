"""Grouped exact material-state283, blend285 and palette287 comparison.
Usage: python scripts/test_performance_batch289.py LABEL on|off [SECONDS]
Preserves276/277/278 baseline and diagnostics;281 remainsoff.
"""
from pathlib import Path
import os,runpy,sys
if len(sys.argv) not in (3,4) or sys.argv[2] not in ('on','off'):
    raise SystemExit(__doc__)
label,mode=sys.argv[1:3]
seconds=sys.argv[3] if len(sys.argv)==4 else '120'
os.environ['DRIVING_ADMISSION283']='1' if mode=='on' else '0'
sys.argv=[str(Path(__file__).with_name('test_performance_batch281.py')),label,'off',seconds]
runpy.run_path(sys.argv[0],run_name='__main__')
