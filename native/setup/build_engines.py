"""Build both Nightfire engines on this PC from the player's own game files.

    python native\\setup\\build_engines.py --game "release\\alpha\\Game Files" --workshop workshop

The public Setup (release\\alpha\\Setup.cmd) runs this first. Every step writes only inside
--workshop:

  1. Checks Python, Git, CMake, the Visual Studio C++ build tools and the two PAL XBEs.
  2. Capstone (the disassembler the translator uses) comes from <workshop>\\python-deps; if it
     is not there yet, pip installs capstone 5.0.6 into that folder (nothing system-wide).
  3. The translator, xboxrecomp, is cloned into <workshop>\\xboxrecomp at the pinned commit
     with Windows line endings. An existing checkout at that commit is reused.
  4. native\\regen\\regen.py translates both XBEs and applies our recipes; every file must
     match the recipe's manifest of the tested build. Skipped when the build folders already
     hold a matching translation.
  5. The two build folders, <workshop>\\nightfire-port-native (Action) and
     <workshop>\\nightfire-driving-native (Driving), are filled from native\\action,
     native\\driving, the shared files listed in trees.json and the translation. Test
     programs that are not published get empty stand-ins; they are never built.
  6. Each engine is built: the preparation scripts its build-native.cmd runs (renames and
     hooks; Action's graphics translation step is skipped because its output is in
     native\\action\\runtime\\native), then CMake with Visual Studio, RelWithDebInfo.

Run it again after updating the source: only changed files are rewritten, so the compiler
rebuilds only what changed. Nothing generated from the game ever leaves this PC.
"""
from pathlib import Path
import argparse, hashlib, json, os, re, shutil, subprocess, sys, time

HERE = Path(__file__).resolve().parent
NATIVE = HERE.parent
REPO = NATIVE.parent
PIN = '051a128df5ec27ef14f1ceaaead11c5457321eef'
TOOLKIT_URL = 'https://github.com/sp00nznet/xboxrecomp.git'
CAPSTONE = 'capstone==5.0.6'
XBE_SHA = {
    'default.xbe': 'b464b787250ffc0ea7443a0e0848689c03a6cdaf6eb593fa0dba509bd2641aa1',   # PAL Action engine
    'Driving.xbe': '0f4c50e4f84b1edeedab4667933b36c9c228da463cc89180e4c70cf3a88bfcef',   # PAL Driving engine
}
MARKER = '.made-by-setup'
STAND_IN = '/* Stand-in made by Setup: this test program is not published and is never built. */\n'
# Folders inside a build folder that belong to the build or the game, never to the source.
KEEP = {'build-native', 'logs', 'cache', 'sessions', 'runs', 'saves', 'captures', 'game_files'}
ENGINES = {
    'action': dict(
        tree='nightfire-port-native',
        prep=[['nd3d_action_rename.py', '--apply'], ['nds_action_hook.py'], ['nf_fast_calls_hook.py'],
              ['pcg_menu_rename.py', '--apply'], ['nf_rcr_fix.py', '--apply']],
        cmake=['-C', 'native-cache.cmake', '-DNIGHTFIRE_HOST_EXE_NAME:STRING=nightfire_native',
               '-DNIGHTFIRE_NATIVE_D3D=ON'],
        target='nightfire_diagnostic', exe='nightfire_native.exe'),
    'driving': dict(
        tree='nightfire-driving-native',
        prep=[['native_d3d_rename.py', '--apply'], ['native_d3d_glue.py'], ['nf_frndint_fix.py', '--apply']],
        cmake=['-DDRIVING_LEAN_RENDERER=ON', '-DDRIVING_FAST_BOOTSTRAP_BUILD=OFF',
               '-DDRIVING_REGION_CACHE445=ON', '-DDRIVING_NATIVE_D3D=ON'],
        target=None, exe='nightfire_driving_lean.exe'),
}
GENERATORS = {'16': 'Visual Studio 16 2019', '17': 'Visual Studio 17 2022', '18': 'Visual Studio 18 2026'}


def say(text=''):
    print(text, flush=True)


def sha256(p):
    h = hashlib.sha256()
    with open(p, 'rb') as f:
        for block in iter(lambda: f.read(1 << 20), b''):
            h.update(block)
    return h.hexdigest()


def norm_sha(p):
    text = Path(p).read_bytes().replace(b'\r\n', b'\n').replace(b'\r', b'\n')
    return hashlib.sha256(b'\n'.join(line.rstrip() for line in text.split(b'\n'))).hexdigest()


def run(cmd, cwd, log, env=None, quiet=False):
    """Run cmd, appending its output to log; stop Setup with the log's tail if it fails."""
    t = time.time()
    with open(log, 'a', encoding='utf-8', errors='replace') as fh:
        fh.write(f'\n> {" ".join(str(c) for c in cmd)}\n')
        fh.flush()
        r = subprocess.run([str(c) for c in cmd], cwd=cwd, env=env, stdout=fh, stderr=subprocess.STDOUT)
    if r.returncode:
        tail = Path(log).read_text(encoding='utf-8', errors='replace')[-3000:]
        raise SystemExit(f'{tail}\n\nThis step failed (exit {r.returncode}): {" ".join(str(c) for c in cmd)}\n'
                         f'Full log: {log}')
    if not quiet:
        say(f'  done in {time.time() - t:.0f} s')


def nightfire_running():
    if os.name != 'nt':
        return False
    q = subprocess.run(['powershell', '-NoProfile', '-Command',
                        '@(Get-Process nightfire* -ErrorAction SilentlyContinue).Count'],
                       capture_output=True, text=True)
    return q.stdout.strip() not in ('', '0')


def find_tool(name, extra):
    found = shutil.which(name)
    if found:
        return found
    for p in extra:
        if p and Path(p).is_file():
            return str(p)
    return None


def find_visual_studio():
    """(install path, major version) of the newest Visual Studio with the x64 C++ tools."""
    vswhere = Path(os.environ.get('ProgramFiles(x86)', r'C:\Program Files (x86)')) / 'Microsoft Visual Studio/Installer/vswhere.exe'
    if not vswhere.is_file():
        return None, None
    q = subprocess.run([str(vswhere), '-latest', '-products', '*', '-requires',
                        'Microsoft.VisualStudio.Component.VC.Tools.x86.x64', '-format', 'json'],
                       capture_output=True, text=True)
    try:
        found = json.loads(q.stdout or '[]')
    except ValueError:
        found = []
    if not found:
        return None, None
    return found[0]['installationPath'], found[0]['installationVersion'].split('.')[0]


def check_tools(a):
    say('Checking the tools:')
    if sys.version_info < (3, 8):
        raise SystemExit('Python 3.8 or newer is needed.')
    say(f'  Python {sys.version.split()[0]}')
    git = find_tool('git', [r'C:\Program Files\Git\cmd\git.exe'])
    if not git:
        raise SystemExit('Git was not found. Install it from https://git-scm.com/download/win, then run Setup again.')
    say(f'  Git: {git}')
    vs, major = (None, None) if a.no_build else find_visual_studio()
    if not a.no_build and not vs:
        raise SystemExit('The Visual Studio C++ build tools were not found. Install "Build Tools for Visual Studio 2022"\n'
                         'from https://visualstudio.microsoft.com/downloads/ with the "Desktop development with C++"\n'
                         'workload, then run Setup again.')
    if vs:
        if major not in GENERATORS:
            raise SystemExit(f'Visual Studio {major} is not supported yet (2019, 2022 or 2026 are).')
        say(f'  Visual Studio {major}: {vs}')
    bundled = Path(vs) / 'Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe' if vs else None
    cmake = find_tool('cmake', [r'C:\Program Files\CMake\bin\cmake.exe', bundled])
    if not a.no_build and not cmake:
        raise SystemExit('CMake was not found. Install it from https://cmake.org/download/ (or the "C++ CMake tools"\n'
                         'part of the Visual Studio build tools), then run Setup again.')
    if cmake:
        say(f'  CMake: {cmake}')
    return git, cmake, GENERATORS.get(major)


def check_game(game):
    say(f'\nGame files ({game}):')
    found = {}
    for name, want in XBE_SHA.items():
        f = game / name
        if not f.is_file():
            raise SystemExit(f'{name} is missing from "{game}". Copy the contents of your extracted PAL disc there,\n'
                             'so that default.xbe and Driving.xbe sit directly inside it, then run Setup again.')
        if sha256(f) != want:
            raise SystemExit(f'{name} is not the PAL version this build is made for.')
        say(f'  {name}: PAL version confirmed')
        found[name] = f
    return found


def capstone_folder(ws, a):
    deps = Path(a.python_deps).resolve() if a.python_deps else ws / 'python-deps'
    probe = 'import capstone, sys; sys.exit(0 if capstone.__version__.startswith("5.") else 1)'
    env = dict(os.environ, PYTHONPATH=str(deps))
    if subprocess.run([sys.executable, '-c', probe], env=env, capture_output=True).returncode == 0:
        say('\nCapstone 5: found')
        return deps
    say(f'\nCapstone (the disassembler the translator uses) is installed into {deps}')
    say('  (only that folder is written; pip downloads it from pypi.org).')
    deps.mkdir(parents=True, exist_ok=True)
    run([sys.executable, '-m', 'pip', 'install', '--disable-pip-version-check', '--no-input',
         '--target', deps, CAPSTONE], ws, ws / 'logs/capstone.log')
    if subprocess.run([sys.executable, '-c', probe], env=env, capture_output=True).returncode:
        raise SystemExit(f'Capstone still does not load from {deps}; see {ws / "logs/capstone.log"}')
    return deps


def toolkit(ws, git):
    tk = ws / 'xboxrecomp'
    log = ws / 'logs/toolkit.log'
    if (tk / '.git').exists():
        head = subprocess.run([git, '-C', str(tk), 'rev-parse', 'HEAD'], capture_output=True, text=True).stdout.strip()
        dirty = subprocess.run([git, '-C', str(tk), 'status', '--porcelain'], capture_output=True, text=True).stdout.strip()
        if head == PIN and not dirty:
            say(f'\nTranslator: {tk} (commit {PIN[:7]})')
            return tk
        raise SystemExit(f'{tk} is not a clean checkout of commit {PIN[:7]}. Move it away and run Setup again.')
    if tk.exists():
        raise SystemExit(f'{tk} exists but is not a Git checkout. Move it away and run Setup again.')
    say(f'\nTranslator: cloning xboxrecomp (MIT licence) at commit {PIN[:7]} into {tk}')
    # Windows line endings: the Action build checks one toolkit file's hash as checked out on Windows.
    run([git, '-c', 'core.autocrlf=true', 'clone', '--quiet', '--no-checkout', TOOLKIT_URL, tk], ws, log, quiet=True)
    run([git, '-C', tk, 'config', 'core.autocrlf', 'true'], ws, log, quiet=True)
    run([git, '-C', tk, '-c', 'advice.detachedHead=false', 'checkout', '--quiet', PIN], ws, log)
    return tk


def gen_matches(tree, engine):
    """True when the build folder's translation already matches the recipe manifest."""
    gen = tree / 'src/recomp/gen'
    files = json.loads((NATIVE / engine / 'regen/manifest.json').read_text())['files']
    if not gen.is_dir():
        return False
    present = {p.name for p in gen.iterdir() if p.suffix in ('.c', '.h')}
    return present == set(files) and all(norm_sha(gen / n) == h for n, h in files.items())


def tree_files(engine, trees):
    """{path in the build folder: source file in this repository}"""
    base = NATIVE / engine
    out = {}
    for p in sorted(base.rglob('*')):
        rel = p.relative_to(base)
        if p.is_file() and rel.parts[0] != 'regen' and '__pycache__' not in rel.parts:
            out[rel.as_posix()] = p
    for rel, src in trees[engine]['shared'].items():
        if rel in out:
            raise SystemExit(f'trees.json lists {rel}, which native/{engine} already has')
        out[rel] = REPO / src
    missing = [rel for rel, src in out.items() if not src.is_file()]
    for rel in trees[engine].get('required', []):
        if rel not in out:
            missing.append(rel)
    if missing:
        raise SystemExit(f'The {engine} build needs files this copy of the source does not have:\n  '
                         + '\n  '.join(sorted(missing)))
    return out


def sync(tree, wanted, gen_dir):
    """Make tree hold exactly `wanted` (+ the translation), touching only files that differ."""
    if tree.exists() and not (tree / MARKER).exists():
        raise SystemExit(f'{tree} was not made by Setup; move it away and run Setup again.')
    tree.mkdir(parents=True, exist_ok=True)
    (tree / MARKER).write_text('This build folder is made and updated by Setup (native/setup/build_engines.py).\n')
    if gen_dir:
        for p in sorted(gen_dir.iterdir()):
            if p.is_file():
                wanted['src/recomp/gen/' + p.name] = p
    keep_gen = gen_dir is None
    written = removed = 0
    for p in sorted(tree.rglob('*'), reverse=True):
        rel = p.relative_to(tree)
        if rel.parts[0] in KEEP or rel.as_posix() == MARKER:
            continue
        if keep_gen and rel.as_posix().startswith('src/recomp/gen'):
            continue
        if p.is_file() and rel.as_posix() not in wanted:
            if p.stat().st_size == len(STAND_IN) and p.read_text(errors='replace') == STAND_IN:
                continue   # a test stand-in; stand_ins() keeps it
            p.unlink(); removed += 1
        elif p.is_dir() and not any(p.iterdir()):
            p.rmdir()
    for rel, src in wanted.items():
        dst = tree / rel
        data = src.read_bytes()
        if dst.is_file() and dst.read_bytes() == data:
            continue
        dst.parent.mkdir(parents=True, exist_ok=True)
        dst.write_bytes(data); written += 1
    return written, removed


def stand_ins(tree):
    """Empty files for the sources of EXCLUDE_FROM_ALL targets that are not published."""
    made = 0
    for cm in tree.rglob('CMakeLists.txt'):
        rel = cm.relative_to(tree)
        if rel.parts[0] in KEEP or 'gen' in rel.parts:
            continue
        text = re.sub(r'#[^\n]*', '', cm.read_text(encoding='utf-8', errors='replace'))
        # Targets the CMake file itself only defines when a source exists are left alone.
        guarded = {g.replace('${CMAKE_CURRENT_SOURCE_DIR}/', '')
                   for g in re.findall(r'\bEXISTS\s+"?([^")\s]+)', text)}
        for m in re.finditer(r'\badd_(?:executable|library)\s*\(([^)]*)\)', text):
            args = m.group(1).split()
            if 'EXCLUDE_FROM_ALL' not in args or guarded & {t.strip('"') for t in args}:
                continue
            for tok in args[1:]:
                tok = tok.strip('"').replace('${CMAKE_CURRENT_SOURCE_DIR}/', '')
                if '$' in tok or not re.fullmatch(r'[\w./-]+\.(?:c|cc|cpp)', tok):
                    continue
                p = (cm.parent / tok).resolve()
                if p.exists():
                    continue
                if tree.resolve() not in p.parents:
                    raise SystemExit(f'{cm} names a test source outside its build folder: {tok}')
                p.parent.mkdir(parents=True, exist_ok=True)
                p.write_text(STAND_IN)
                made += 1
    return made


def build(engine, tree, cmake, generator, jobs, ws):
    e = ENGINES[engine]
    log = ws / 'logs' / f'build-{engine}.log'
    log.write_text('')
    env = dict(os.environ, PYTHONDONTWRITEBYTECODE='1')
    say(f'  preparing ({len(e["prep"])} scripts)')
    for script in e['prep']:
        run([sys.executable, tree / 'scripts' / script[0]] + script[1:], tree, log, env, quiet=True)
    say(f'  configuring with {generator}')
    run([cmake, '-S', '.', '-B', 'build-native', '-G', generator, '-A', 'x64'] + e['cmake'], tree, log, env, quiet=True)
    say(f'  compiling (this is the long part; log: {log})')
    cmd = [cmake, '--build', 'build-native', '--config', 'RelWithDebInfo', '--parallel', str(jobs)]
    if e['target']:
        cmd += ['--target', e['target']]
    run(cmd, tree, log, env)
    exe = tree / 'build-native/RelWithDebInfo' / e['exe']
    if not exe.is_file():
        raise SystemExit(f'The build finished but {exe} is missing; see {log}')
    say(f'  built {exe.name} ({exe.stat().st_size:,} bytes)')


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('--game', required=True, help='folder holding the extracted PAL disc (default.xbe, Driving.xbe)')
    p.add_argument('--workshop', required=True, help='folder for the toolkit, translation and both build folders')
    p.add_argument('--engines', default='action,driving')
    p.add_argument('--python-deps', help='folder holding Capstone (default: <workshop>\\python-deps)')
    p.add_argument('--jobs', type=int, default=max(1, min(4, os.cpu_count() or 1)), help='parallel compiles')
    p.add_argument('--retranslate', action='store_true', help='translate again even if the build folders match')
    p.add_argument('--no-build', action='store_true', help='stop after filling the build folders')
    a = p.parse_args()
    engines = [e for e in a.engines.split(',') if e]
    if any(e not in ENGINES for e in engines):
        raise SystemExit(f'--engines takes action and/or driving, not {a.engines}')
    game, ws = Path(a.game).resolve(), Path(a.workshop).resolve()
    if nightfire_running():
        raise SystemExit('A Nightfire game is running. Close it, then run Setup again.')
    say(f'Building the Nightfire engines\n  source:   {REPO}\n  workshop: {ws}\n')
    git, cmake, generator = check_tools(a)
    check_game(game)
    (ws / 'logs').mkdir(parents=True, exist_ok=True)
    trees = json.loads((HERE / 'trees.json').read_text())
    wanted = {e: tree_files(e, trees) for e in engines}
    deps = capstone_folder(ws, a)
    tk = toolkit(ws, git)

    todo = [e for e in engines if a.retranslate or not gen_matches(ws / ENGINES[e]['tree'], e)]
    regen_out = None
    if todo:
        regen_out = ws / time.strftime('translation-%Y%m%d-%H%M%S')
        say(f'\nTranslating {" and ".join(todo)} from your game files (several minutes):')
        env = dict(os.environ, PYTHONDONTWRITEBYTECODE='1')
        run([sys.executable, NATIVE / 'regen/regen.py', '--game', game, '--toolkit', tk, '--out', regen_out,
             '--engines', ','.join(todo), '--python-deps', deps], ws, ws / 'logs/translate.log', env)
    else:
        say('\nTranslation: both build folders already match the recipes; not translating again.')

    for e in engines:
        tree = ws / ENGINES[e]['tree']
        gen = regen_out / e / 'src/recomp/gen' if e in todo else None
        written, removed = sync(tree, wanted[e], gen)
        made = stand_ins(tree)
        say(f'\n{e.capitalize()} build folder {tree}:\n  {written} files written, {removed} removed, '
            f'{made} test stand-ins')
        if not a.no_build:
            build(e, tree, cmake, generator, a.jobs, ws)
    if regen_out:
        shutil.copy2(regen_out / 'REGEN-INFO.json', ws / 'logs/translation-info.json')
        shutil.rmtree(regen_out)   # made by this run; the build folders now hold the translation
    say('\nBoth engines are ready.' if not a.no_build else '\nBuild folders are ready (not compiled).')
    return 0


if __name__ == '__main__':
    sys.exit(main())
