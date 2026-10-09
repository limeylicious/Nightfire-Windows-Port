"""Bounded development shortcut; ordinary initialization, without Paris intro.

Usage: python scripts/test_direct_paris240.py [label] [seconds] [scene|owned|late|road|offscreen]
Uses the current build and existing run-windows.cmd. Not normal progression proof.
"""
from pathlib import Path
import os
import runpy
import sys

label = sys.argv[1] if len(sys.argv) > 1 else 'direct-paris240'
seconds = sys.argv[2] if len(sys.argv) > 2 else '90'
capture = sys.argv[3] if len(sys.argv) > 3 else 'scene'
assert len(sys.argv) <= 4 and capture in ('scene', 'owned', 'late', 'road', 'offscreen')
# Do not inherit another checkpoint's frozen executable or diagnostic selectors.
for name in list(os.environ):
    if name.startswith('DRIVING_WORLD_CAPTURE') or name in (
        'DRIVING_EXECUTABLE', 'DRIVING_PRODUCER235', 'DRIVING_CADENCE204',
        'DRIVING_MOVIE_TRACE180', 'DRIVING_MOVIE_CAPTURE198',
        'DRIVING_POSTMOVIE_TRACE207', 'DRIVING_TEXTURE_CAPTURE167',
        'DRIVING_RESOLVE_CAPTURE222', 'DRIVING_LATE_CAPTURE240', 'DRIVING_OFFSCREEN_CAPTURE243',
        'DRIVING_NO_LAUNCH144', 'DRIVING_LAUNCH_PAYLOAD144',
        'RECOMP_APU_TRACE', 'RECOMP_FB_DUMP'):
        os.environ.pop(name, None)
for name in ('DRIVING_MOVIE_GPU201', 'DRIVING_MOVIE_WINDOW201',
             'DRIVING_FLIP204', 'DRIVING_WORLD_GPU214', 'DRIVING_WORLD_GPU216',
             'DRIVING_WORLD_GPU220', 'DRIVING_WORLD_GPU221', 'DRIVING_BATCH236',
             'DRIVING_INPUT224', 'DRIVING_DEV_SKIP_PARIS_INTRO240',
             'DRIVING_LEAN_TEST240', 'DRIVING_DIAGNOSTIC_DSP149',
             'RECOMP_AC97_READY'):
    os.environ[name] = '1'
if capture == 'owned':
    os.environ['DRIVING_WORLD_CAPTURE_BATCH221'] = '1'
elif capture == 'road':
    os.environ['DRIVING_WORLD_CAPTURE_BATCH219'] = '1'
elif capture == 'offscreen':
    os.environ['DRIVING_OFFSCREEN_CAPTURE243'] = '1'
elif capture == 'late':
    os.environ['DRIVING_LATE_CAPTURE240'] = '1'
sys.argv = [str(Path(__file__).with_name('test_boot.py')), label, seconds]
runpy.run_path(sys.argv[0], run_name='__main__')
