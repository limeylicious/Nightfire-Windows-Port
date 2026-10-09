"""Local pinned-runtime adapters, reusing only audited generic action DVD logic."""
from pathlib import Path
import hashlib, json
root = Path(__file__).resolve().parents[1]
toolkit = root.parent / 'xboxrecomp'
action = root.parent / 'nightfire-port/runtime'
dest = root / 'runtime'
provenance = {}
def read(path):
    provenance[str(path.relative_to(root.parent))] = hashlib.sha256(path.read_bytes()).hexdigest()
    return path.read_text(encoding='utf-8')
def replace_once(text, old, new):
    assert text.count(old) == 1, (old, text.count(old))
    return text.replace(old, new)
for name in ('kernel_path.c', 'kernel_file.c', 'kernel_bridge.c', 'kernel_thunks.c'):
    text = read(toolkit / 'src/kernel' / name)
    if name == 'kernel_path.c':
        # HDD title metadata belongs to this isolated project's saves, not assets.
        rule = '    { "\\\\Device\\\\Harddisk0\\\\Partition1\\\\",    0, NULL,         NULL          },'
        text = replace_once(text, rule, rule.replace('    0, NULL', '    1, NULL'))
        anchor = '    if (xbox_partition_device_path(xbox_path, host_path_buf, buf_size)) {'
        block = '    if (nightfire_disc_root(xbox_path)) {\n        remainder = "";\n        base_dir = s_game_dir;\n        sub_dir = NULL;\n        goto translate;\n    }\n\n'
        text = replace_once(text, anchor, block + anchor)
    elif name == 'kernel_file.c':
        anchor = '    if (CreateOptions & XBOX_FILE_DIRECTORY_FILE) {\n'
        block = '    /* Exact optical root uses a directory handle for its media query. */\n    if (nightfire_disc_root(get_xbox_path(ObjectAttributes)))\n        flags_and_attrs |= FILE_FLAG_BACKUP_SEMANTICS;\n\n'
        assert anchor in text
        text = text.replace(anchor, block + anchor, 1)
    elif name == 'kernel_bridge.c':
        # Lookup returns a shared dispatcher. Its selected slot must belong to
        # this thread until invocation; a concurrent USB worker can otherwise
        # substitute a different kernel operation and stack-argument cleanup.
        text = replace_once(text, 'static int g_kernel_dispatch_slot = -1;',
                            'static RECOMP_TLS int g_kernel_dispatch_slot = -1;')
        # Both original PAL callers and kernel.h use fastcall CL, no stack args.
        # The pinned wrappers mistakenly read a saved stack value as the IRQL.
        for function in ('KfRaiseIrql', 'KfLowerIrql'):
            anchor = f'static void bridge_{function}(void)\n{{\n    uint32_t new_irql = STACK_ARG(0);'
            text = replace_once(text, anchor, anchor.replace('STACK_ARG(0)', 'g_ecx & 0xFFu'))
        # Actual first boot overwrote five code DWORDs with bogus huge ordinals.
        # The Driving header's declared import run is complete: don't guess extras.
        anchor = 'for (i = 1; i <= LOOKBEHIND; i++) {'
        text = replace_once(text, anchor, 'for (i = 1; i <= 0; i++) { /* driving: no speculative imports before declared table */')
        old = read(action / name)
        start = old.index('    /* Xbox 32-bit SCSI pass-through direct request used during XAPI startup. */')
        end = old.index('    if (ioctl == IOCTL_DISK_GET_DRIVE_GEOMETRY)', start)
        block = old[start:end]
        anchor = '    if (ioctl == IOCTL_DISK_GET_DRIVE_GEOMETRY)'
        text = replace_once(text, anchor, block + anchor)
    else:
        anchor = '    xbox_kernel_get_thunk_address(&thunk_base, &thunk_count);\n'
        block = '    ULONG driving_thunk_limit = (thunk_base && xbox_GetMemoryBase())\n        ? (thunk_count < XBOX_KERNEL_THUNK_TABLE_SIZE ? thunk_count : XBOX_KERNEL_THUNK_TABLE_SIZE)\n        : XBOX_KERNEL_THUNK_TABLE_SIZE;\n'
        text = replace_once(text, anchor, anchor + block)
        text = replace_once(text, 'for (ULONG i = 0; i < XBOX_KERNEL_THUNK_TABLE_SIZE; i++)', 'for (ULONG i = 0; i < driving_thunk_limit; i++)')
        text = replace_once(text, 'resolved, XBOX_KERNEL_THUNK_TABLE_SIZE, unresolved);', 'resolved, driving_thunk_limit, unresolved);')
        text = replace_once(text, 'fopen("xbox_kernel.log", "w")', 'fopen("logs/xbox_kernel.log", "w")')
    (dest / name).write_text('#include "nightfire_disc.h"\n' + text, encoding='utf-8')
(dest / 'nightfire_disc.h').write_text(read(action / 'nightfire_disc.h'), encoding='utf-8')
(dest / 'nightfire_video_io.h').write_text(read(action / 'nightfire_video_io.h'), encoding='utf-8')
(root / 'analysis/runtime-provenance142.json').write_text(json.dumps(provenance, indent=2), encoding='utf-8')
