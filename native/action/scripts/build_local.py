"""Invoke the existing Windows build with a clean environment in Codex.
No global Git configuration or prerequisite installation is changed.
"""
import os,argparse
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[1]
parser=argparse.ArgumentParser()
parser.add_argument('--config',choices=('Debug','RelWithDebInfo'),default='Debug')
args=parser.parse_args()
env = {k.upper(): v for k, v in os.environ.items()}
env['NIGHTFIRE_BUILD_CONFIG']=args.config
env['PATH'] = 'C:/Program Files/CMake/bin;' + env.get('PATH', '')
env.update(GIT_CONFIG_COUNT='1', GIT_CONFIG_KEY_0='safe.directory',
           GIT_CONFIG_VALUE_0=(root.parent / 'xboxrecomp').as_posix(),
           MSBUILDDISABLENODEREUSE='1', MSBUILDNOINPROCNODE='1')
raise SystemExit(subprocess.call(['cmd.exe', '/d', '/c', 'build-windows.cmd'], cwd=root, env=env))
