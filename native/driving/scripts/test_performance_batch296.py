"""Grouped readback/scheduling experiment; original initialized Paris shortcut.

Usage: python scripts/test_performance_batch296.py LABEL off|readback|async|both [SECONDS]
Keeps293's optimized272/278 baseline, with291 and281 disabled. Tests are
development-only and cannot establish playable or original-hardware fidelity.
"""
from pathlib import Path
import os
import runpy
import sys

if len(sys.argv) not in (3, 4) or sys.argv[2] not in ('off', 'readback', 'async', 'both'):
    raise SystemExit(__doc__)
label, mode = sys.argv[1:3]
seconds = sys.argv[3] if len(sys.argv) == 4 else '120'
os.environ['DRIVING_READBACK294'] = '1' if mode in ('readback', 'both') else '0'
os.environ['DRIVING_ASYNC_TAIL295'] = '1' if mode in ('async', 'both') else '0'
sys.argv = [str(Path(__file__).with_name('test_performance_batch293.py')), label, 'optimized', seconds]
runpy.run_path(sys.argv[0], run_name='__main__')
