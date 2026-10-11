"""Rebuild both Nightfire engines' translated code from the player's own game files.

    python regen.py --game "Game Files" --toolkit tools\\xboxrecomp --out regen-20261011
    options: --engines action,driving   --python-deps DIR   --compare-action GEN   --compare-driving GEN

For each engine this makes a build tree under --out (a NEW folder; nothing outside it is
written), runs the pinned xboxrecomp toolkit on the engine's XBE, applies the engine's
recipe (native/<engine>/regen/: addresses, our names, our code, SHA-256 guards) and checks
every resulting file against the recipe's manifest of the tested build. --compare-* also
compares with an existing gen folder, read-only.

Nothing generated from the game is stored in this repository: the translated code only
ever exists on the player's PC, made from their disc.
"""
from pathlib import Path
import argparse, hashlib, json, os, shutil, subprocess, sys, time

HERE = Path(__file__).resolve().parent
NATIVE = HERE.parent
PIN = '051a128df5ec27ef14f1ceaaead11c5457321eef'
XBE = {
    'action': ('default.xbe', 'b464b787250ffc0ea7443a0e0848689c03a6cdaf6eb593fa0dba509bd2641aa1',
               'Nightfire diagnostic build'),
    'driving': ('Driving.xbe', '0f4c50e4f84b1edeedab4667933b36c9c228da463cc89180e4c70cf3a88bfcef',
                'Nightfire PAL Driving'),
}


def sha256(p):
    h = hashlib.sha256()
    with open(p, 'rb') as f:
        for b in iter(lambda: f.read(1 << 20), b''):
            h.update(b)
    return h.hexdigest()


def norm(text):
    return '\n'.join(l.rstrip() for l in text.replace('\r\n', '\n').replace('\r', '\n').split('\n'))


def norm_sha(p):
    return hashlib.sha256(norm(Path(p).read_text(encoding='utf-8', errors='surrogateescape')).encode('utf-8', 'surrogateescape')).hexdigest()


def find_xbe(game, name, want):
    for p in sorted(game.rglob('*')):
        if p.is_file() and p.name.lower() == name.lower() and sha256(p) == want:
            return p
    raise SystemExit(f'{name} (PAL, sha256 {want[:12]}...) not found under {game}')


def toolkit_info(tk):
    head = (tk / '.git' / 'HEAD').read_text().strip() if (tk / '.git' / 'HEAD').exists() else None
    files = {p.relative_to(tk).as_posix(): sha256(p) for p in sorted((tk / 'tools').rglob('*.py')) if '__pycache__' not in p.parts}
    return {'head': head, 'pinned': head == PIN, 'tools_py_files': len(files),
            'tools_tree_sha256': hashlib.sha256(json.dumps(files, sort_keys=True).encode()).hexdigest()}


def run(cmd, cwd, env, log, info, name, allow_fail=False):
    t = time.time()
    with open(log, 'w', encoding='utf-8') as fh:
        r = subprocess.run([str(c) for c in cmd], cwd=cwd, env=env, stdout=fh, stderr=subprocess.STDOUT)
    info[name] = {'seconds': round(time.time() - t, 1), 'exit': r.returncode}
    print(f'  {name}: exit {r.returncode} in {time.time() - t:.0f} s', flush=True)
    if r.returncode:
        print(Path(log).read_text(encoding='utf-8', errors='replace')[-4000:])
        if not allow_fail:
            raise SystemExit(f'{name} failed; see {log}')


def expand_action_inputs(fl, idl):
    """The toolkit's three input lists, rebuilt from the compact recipe files."""
    sec, names = fl['sections'], fl['names']
    def name(a):
        return names.get('%08X' % a, 'sub_%08X' % a)
    fn = [dict(start='0x%08X' % a, end='0x%08X' % (a + n), size=n, name=name(a), section=sec[s], has_prologue=bool(p))
          for a, n, s, p in fl['functions']]
    fsec = {a: s for a, _, s, _ in fl['functions']}
    lab = fl['labels']
    labels = [dict(address='0x%08X' % a, name='sub_%08X' % a, type='function',
                   section=sec[fsec[a] if a in fsec else lab['sections']['%X' % a]]) for a in lab['functions']]
    labels += [dict(address='0x%08X' % a, name=n, type=t, section=sec[s]) for a, n, t, s in lab['other']]
    labels.sort(key=lambda l: l['address'])
    cats = idl['categories']
    idf = [dict(start='0x%08X' % a, end='0x%08X' % (a + n), size=n, name=name(a), section=sec[s], category=cats[c])
           for a, n, s, c in idl['functions']]
    return fn, labels, idf


def check(gen, manifest, compare, info):
    res = {'manifest': None, 'compare': None}
    names = sorted(p.name for p in gen.iterdir() if p.suffix in ('.c', '.h')) if gen.exists() else []
    if manifest and manifest.exists():
        want = json.loads(manifest.read_text())['files']
        bad = [n for n in sorted(set(want) | set(names)) if want.get(n) != (norm_sha(gen / n) if n in names else None)]
        res['manifest'] = {'files': len(want), 'mismatched': bad}
        print(f'  manifest: {len(want) - len(bad)} of {len(want)} files match' + (f'; MISMATCH {bad[:10]}' if bad else ''))
    if compare:
        compare = Path(compare)
        other = sorted(p.name for p in compare.iterdir() if p.suffix in ('.c', '.h'))
        bad = [n for n in sorted(set(other) | set(names))
               if n not in names or n not in other or norm_sha(gen / n) != norm_sha(compare / n)]
        res['compare'] = {'folder': str(compare), 'files': len(set(other) | set(names)), 'mismatched': bad}
        print(f'  compared with {compare}: {len(set(other) | set(names)) - len(bad)} identical' + (f'; DIFFERENT {bad[:10]}' if bad else ''))
    info['check'] = res
    return res


def tree_for(kind, out, xbe):
    root = out / kind
    for d in ('scripts', 'runtime/native', 'game_files', 'analysis', 'src/recomp/gen', 'logs'):
        (root / d).mkdir(parents=True, exist_ok=True)
    src = NATIVE / kind
    for p in (src / 'scripts').glob('*.py'):
        shutil.copy2(p, root / 'scripts' / p.name)
    shutil.copy2(xbe, root / 'game_files' / XBE[kind][0])
    return root


def action(out, tk, xbe, env, info, compare):
    rd = NATIVE / 'action/regen'
    root = tree_for('action', out, xbe)
    shutil.copy2(NATIVE / 'action/runtime/native/nd3d-action-routines.json', root / 'runtime/native')
    an = root / 'analysis'
    (an / 'disasm').mkdir(); (an / 'func_id').mkdir()
    fn, labels, idf = expand_action_inputs(json.loads((rd / 'inputs/function-list.json').read_text()),
                                           json.loads((rd / 'inputs/classification.json').read_text()))
    for path, data in ((an / 'disasm/functions.json', fn), (an / 'disasm/labels.json', labels),
                       (an / 'func_id/identified_functions.json', idf), (an / 'functions.json', fn)):
        path.write_text(json.dumps(data, indent=1))
    py = sys.executable
    run([py, '-m', 'tools.abi_analysis', root / 'game_files/default.xbe', '--functions', an / 'disasm/functions.json',
         '--func-id-dir', an / 'func_id', '--output-dir', an / 'abi'], tk, env, root / 'logs/abi.log', info, 'abi')
    run([py, '-m', 'tools.recomp', root / 'game_files/default.xbe', '--functions', an / 'disasm/functions.json',
         '--disasm-dir', an / 'disasm', '--func-id-dir', an / 'func_id', '--abi-dir', an / 'abi', '--all',
         '--split', '500', '--game-name', XBE['action'][2], '--trace-functions', rd / 'inputs/trace-functions.json',
         '--gen-dir', root / 'src/recomp/gen', '-o', an / 'recomp'], tk, env, root / 'logs/recomp.log', info, 'recomp')
    run([py, HERE / 'build_action.py', root, '--recipe-dir', rd, '--toolkit', tk], root, env,
        root / 'logs/repair.log', info, 'repair')
    return check(root / 'src/recomp/gen', rd / 'manifest.json', compare, info)


def driving(out, tk, xbe, env, info, compare):
    rd = NATIVE / 'driving/regen'
    root = out / 'driving'
    for d in ('analysis', 'logs', 'game_files'):
        (root / d).mkdir(parents=True, exist_ok=True)
    x = root / 'game_files/Driving.xbe'
    shutil.copy2(xbe, x)
    scripts = root / 'scripts'
    shutil.copytree(NATIVE / 'driving/scripts', scripts)
    an = root / 'analysis'
    py = sys.executable
    run([py, '-m', 'tools.xbe_parser', x, '--json', an / 'xbe.json'], tk, env, root / 'logs/parse.log', info, 'parse')
    run([py, '-m', 'tools.disasm', x, '--analysis-json', an / 'xbe.json', '--output', an / 'disasm', '-v'],
        tk, env, root / 'logs/disasm.log', info, 'disasm')
    run([py, '-m', 'tools.func_id', x, '--functions', an / 'disasm/functions.json', '--strings', an / 'disasm/strings.json',
         '--xrefs', an / 'disasm/xrefs.json', '--output', an / 'func_id', '-v'], tk, env, root / 'logs/identify.log', info, 'identify')
    run([py, '-m', 'tools.abi_analysis', x, '--disasm-dir', an / 'disasm', '--func-id-dir', an / 'func_id',
         '--output-dir', an / 'abi', '-v'], tk, env, root / 'logs/abi.log', info, 'abi')
    run([py, '-m', 'tools.recomp', x, '--game-name', XBE['driving'][2], '--all', '--split', '500',
         '--disasm-dir', an / 'disasm', '--func-id-dir', an / 'func_id', '--abi-dir', an / 'abi',
         '--output-dir', an / 'recomp', '--gen-dir', root / 'fresh-gen'], tk, env, root / 'logs/recomp.log', info, 'recomp')
    gen = root / 'src/recomp/gen'
    cmd = [py, HERE / 'repair_driving.py', root / 'fresh-gen', gen, '--recipe', rd, '--scripts', scripts,
           '--toolkit', tk, '--xbe', x, '--analysis', an, '--work', root / 'work', '--report', root / 'logs/repair.json']
    run(cmd, root, env, root / 'logs/repair.log', info, 'repair', allow_fail=True)
    (root / 'src').mkdir(exist_ok=True)
    for f in sorted((rd / 'own-src').glob('*')) if (rd / 'own-src').exists() else []:
        shutil.copy2(f, root / 'src' / f.name)
    return check(gen, rd / 'manifest.json', compare, info)


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('--game', required=True, help='folder holding the extracted disc (default.xbe, Driving.xbe)')
    p.add_argument('--toolkit', required=True, help='xboxrecomp checkout at commit ' + PIN[:7])
    p.add_argument('--out', required=True, help='new folder; must not exist yet')
    p.add_argument('--engines', default='action,driving')
    p.add_argument('--python-deps', help='folder holding capstone, added to PYTHONPATH')
    p.add_argument('--compare-action'); p.add_argument('--compare-driving')
    a = p.parse_args()
    tk, game, out = Path(a.toolkit).resolve(), Path(a.game).resolve(), Path(a.out).resolve()
    if out.exists():
        raise SystemExit(f'{out} already exists; pick a new folder so nothing is overwritten.')
    out.mkdir(parents=True)
    env = os.environ.copy()
    env['PYTHONPATH'] = os.pathsep.join([str(tk), str(HERE)] + ([str(Path(a.python_deps).resolve())] if a.python_deps else []))
    env['PYTHONIOENCODING'] = 'utf-8'
    env['PYTHONUTF8'] = '1'  # same text encoding on Windows as elsewhere
    env['PYTHONDONTWRITEBYTECODE'] = '1'
    info = {'started': time.strftime('%Y-%m-%d %H:%M:%S'), 'python': sys.version.split()[0], 'toolkit': toolkit_info(tk)}
    if a.python_deps:
        sys.path.append(str(Path(a.python_deps).resolve()))
    try:
        import capstone
        info['capstone'] = capstone.__version__
    except ImportError:
        info['capstone'] = None
    print(f"toolkit {info['toolkit']['head']} (pinned: {info['toolkit']['pinned']}), python {info['python']}, capstone {info['capstone']}")
    t = time.time(); ok = True
    try:
        for kind in a.engines.split(','):
            print(f'{kind}:', flush=True)
            xbe = find_xbe(game, XBE[kind][0], XBE[kind][1])
            info[kind] = {'xbe': str(xbe)}
            res = globals()[kind](out, tk, xbe, env, info[kind], getattr(a, f'compare_{kind}'))
            if res and ((res['manifest'] and res['manifest']['mismatched']) or (res['compare'] and res['compare']['mismatched'])):
                ok = False
    finally:
        info['seconds'] = round(time.time() - t, 1)
        (out / 'REGEN-INFO.json').write_text(json.dumps(info, indent=2))
    print(f'{"OK" if ok else "DIFFERENCES FOUND"} in {info["seconds"]:.0f} s; results in {out}')
    sys.exit(0 if ok else 1)


if __name__ == '__main__':
    main()
