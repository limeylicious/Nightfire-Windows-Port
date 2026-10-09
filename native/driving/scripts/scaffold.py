"""One-time pinned-template setup; fails rather than replacing existing files."""
from pathlib import Path
import hashlib, json
root = Path(__file__).resolve().parents[1]
toolkit = root.parent / 'xboxrecomp'
manifest = {}
def write(path, content):
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open('x', encoding='utf-8', newline='\n') as f:
        f.write(content)
for name in ('src/main.c', 'src/recomp_manual.c', 'CMakeLists.txt'):
    source = toolkit / 'templates/new-game' / name
    content = source.read_text(encoding='utf-8')
    manifest[str(source.relative_to(toolkit))] = hashlib.sha256(source.read_bytes()).hexdigest()
    if name == 'src/main.c':
        content = content.replace('#define YOUR_GAME_ENTRY_POINT   0x00000000', '#define YOUR_GAME_ENTRY_POINT   0x0010E777')
        content = content.replace('"game\\\\Your Game Title\\\\default.xbe"', '"../nightfire-port/game_files/Driving.xbe"')
        content = content.replace('"game\\\\Your Game Title"', '"../nightfire-port/game_files"')
        content = content.replace('YOUR_GAME_NAME', 'Nightfire PAL Driving Bootstrap')
        content = content.replace('xbox_path_init(YOUR_GAME_DIR, NULL);', 'xbox_path_init(YOUR_GAME_DIR, "saves");')
        # Console diagnostic: fail promptly and let scripts preserve the log.
        content = content.replace('MessageBoxA(NULL, "Failed to load default.xbe.\\n"\n                    "Place the game files in the \'game\' subdirectory.",\n                    "Recomp", MB_ICONERROR);', 'fprintf(stderr, "Missing supplied Driving.xbe\\n");')
    elif name == 'src/recomp_manual.c':
        content = content.replace('#include <stdint.h>', '#include <stdint.h>\n#include <stddef.h>\n#include <windows.h>')
        content = content.replace('extern uint32_t g_eax;', 'extern __declspec(thread) uint32_t g_eax;')
        # Missing translated calls must stop, never silently claim a boot.
        content = content.replace('    fflush(stderr);', '    fflush(stderr);\n    ExitProcess(142);')
    else:
        content = content.replace('project(your_game_recomp C)', 'project(nightfire_driving C)')
        content = content.replace('add_executable(${PROJECT_NAME} WIN32', 'add_executable(${PROJECT_NAME}')
        content = content.replace('PRIVATE /bigobj)', 'PRIVATE /bigobj /fp:strict /Ob2)')
    write(root / name, content)
write(root / 'analysis/template-provenance.json', json.dumps(manifest, indent=2))
