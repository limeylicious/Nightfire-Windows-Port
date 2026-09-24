"""Isolated PAL driving analysis. Never writes the action/toolkit databases."""
from pathlib import Path
import hashlib, json, os, subprocess, sys

ROOT = Path(__file__).resolve().parents[1]
WORKSPACE = ROOT.parent
TOOLKIT = WORKSPACE / 'xboxrecomp'
XBE = WORKSPACE / 'nightfire-port/game_files/Driving.xbe'
EXPECTED = '0f4c50e4f84b1edeedab4667933b36c9c228da463cc89180e4c70cf3a88bfcef'
assert hashlib.sha256(XBE.read_bytes()).hexdigest() == EXPECTED, 'Driving XBE changed'
analysis = ROOT / 'analysis'
logs = ROOT / 'logs'
logs.mkdir(parents=True, exist_ok=True)
analysis.mkdir(exist_ok=True)
env = os.environ.copy()
env['PYTHONPATH'] = str(TOOLKIT) + os.pathsep + str(WORKSPACE / 'nightfire-port/analysis/python-deps')
env['PYTHONIOENCODING'] = 'utf-8'
env['PYTHONDONTWRITEBYTECODE'] = '1'
stages = [
    ('parse', ['tools.xbe_parser', str(XBE), '--json', str(analysis / 'xbe.json')]),
    ('disasm', ['tools.disasm', str(XBE), '--analysis-json', str(analysis / 'xbe.json'), '--output', str(analysis / 'disasm'), '-v']),
    ('identify', ['tools.func_id', str(XBE), '--functions', str(analysis / 'disasm/functions.json'), '--strings', str(analysis / 'disasm/strings.json'), '--xrefs', str(analysis / 'disasm/xrefs.json'), '--output', str(analysis / 'func_id'), '-v']),
    ('abi', ['tools.abi_analysis', str(XBE), '--disasm-dir', str(analysis / 'disasm'), '--func-id-dir', str(analysis / 'func_id'), '--output-dir', str(analysis / 'abi'), '-v']),
    ('recomp', ['tools.recomp', str(XBE), '--game-name', 'Nightfire PAL Driving', '--all', '--split', '500', '--disasm-dir', str(analysis / 'disasm'), '--func-id-dir', str(analysis / 'func_id'), '--abi-dir', str(analysis / 'abi'), '--output-dir', str(analysis / 'recomp'), '--gen-dir', str(ROOT / 'src/recomp/gen')]),
]
for name, args in stages:
    if len(sys.argv) > 1 and name not in sys.argv[1:]:
        continue
    print(f'{name}: starting', flush=True)
    with (logs / f'generate-{name}.log').open('w', encoding='utf-8') as log:
        result = subprocess.run([sys.executable, '-m', *args], cwd=ROOT, env=env, stdout=log, stderr=subprocess.STDOUT)
    if result.returncode:
        print((logs / f'generate-{name}.log').read_text(encoding='utf-8')[-6000:])
        sys.exit(result.returncode)
    print(f'{name}: complete', flush=True)
