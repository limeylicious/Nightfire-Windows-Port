"""Preserve source, scripts, notes and optional binary before local changes."""
from pathlib import Path
import hashlib, json, sys, zipfile
root = Path(__file__).resolve().parents[1]
target = root / 'recovery' / sys.argv[1]
target.mkdir(parents=True, exist_ok=False)
files = [p for folder in ('src', 'scripts', 'runtime', 'launch-data') for p in (root / folder).rglob('*') if p.is_file()]
files += [p for p in root.iterdir() if p.is_file()]
files += [p for p in (root / 'build-windows/RelWithDebInfo').glob('*') if p.suffix in ('.exe', '.pdb')]
manifest = {}
with zipfile.ZipFile(target / 'files.zip', 'x', zipfile.ZIP_DEFLATED) as archive:
    for path in files:
        key = path.relative_to(root).as_posix()
        manifest[key] = hashlib.sha256(path.read_bytes()).hexdigest()
        archive.write(path, key)
(target / 'manifest.json').write_text(json.dumps(manifest, indent=2), encoding='utf-8')
print(f'Recovery: {target} ({len(files)} files)')
