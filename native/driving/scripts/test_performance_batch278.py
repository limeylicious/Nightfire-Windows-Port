"""Exact depth seed278 comparison, direct initialized Paris only.
Usage: python scripts/test_performance_batch278.py LABEL on|off [SECONDS]
Retains276/277 and per-profile counters;272on,268/269off.
"""
from pathlib import Path
import os,runpy,sys
if len(sys.argv) not in (3,4) or sys.argv[2] not in ('on','off'):
    raise SystemExit(__doc__)
label,mode=sys.argv[1:3]
seconds=sys.argv[3] if len(sys.argv)==4 else '120'
os.environ['DRIVING_DEPTH_SEED278']='1' if mode=='on' else '0'
sys.argv=[str(Path(__file__).with_name('test_performance_batch277.py')),label,'on',seconds]
runpy.run_path(sys.argv[0],run_name='__main__')
