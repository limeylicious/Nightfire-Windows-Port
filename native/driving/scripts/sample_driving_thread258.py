"""Bounded Windows x64 instruction-pointer sampling of this development run.

Briefly suspends only the selected game's oldest thread to read its context,
always resuming it before aggregation/symbol lookup. No injection, register or
guest-memory writes. This perturbs execution: do not use the run as an FPS test.
Adapted from the existing Action sampler for Paris performance diagnosis.
Reports leaf locations (including waits), not inclusive function timings.
Output stays within this Driving project's analysis folder. No production code changes.
SDK layouts verified against Windows SDK 10.0.22621 winnt.h and DbgHelp.h.
"""
from pathlib import Path
import argparse, collections, ctypes as C, json, mmap, random, struct, time

p = argparse.ArgumentParser()
p.add_argument('pid', type=int)
p.add_argument('--seconds', type=int, default=20)
p.add_argument('--output', type=Path, required=True)
p.add_argument('--thread-id', type=int, help='Verified thread belonging to this Driving process; default oldest')
p.add_argument('--stack-words', action='store_true', help='Also collect stack words as caller leads, not an unwind')
p.add_argument('--stack-word-count', type=int, choices=(16,64,128,512), default=16,
               help='Bounded raw stack depth; reads stop at inaccessible pages (default16)')
a = p.parse_args()
if not 2 <= a.seconds <= 30 or C.sizeof(C.c_void_p) != 8:
    raise ValueError('Requires x64 Python and 2..30 seconds')
root = Path(__file__).resolve().parents[1]
archive = a.output.resolve()
archive.relative_to((root/'analysis').resolve())
archive.mkdir(parents=True, exist_ok=True)
expected = (root/'build-windows/RelWithDebInfo/nightfire_driving.exe').resolve()
pe = expected.read_bytes()
pe_offset = struct.unpack_from('<I', pe, 0x3c)[0]
if pe[pe_offset:pe_offset+4] != b'PE\0\0' or struct.unpack_from('<H', pe, pe_offset+4)[0] != 0x8664:
    raise ValueError('Only AMD64 game executables supported')

k = C.WinDLL('kernel32', use_last_error=True)
d = C.WinDLL('dbghelp', use_last_error=True)
H, U, Q = C.c_void_p, C.c_uint32, C.c_uint64

def api(lib, name, result, args):
    f = getattr(lib, name); f.restype = result; f.argtypes = args
    return f

open_process = api(k, 'OpenProcess', H, [U, C.c_int, U])
close = api(k, 'CloseHandle', C.c_int, [H])
query_image = api(k, 'QueryFullProcessImageNameW', C.c_int, [H, U, C.c_wchar_p, C.POINTER(U)])
snapshot = api(k, 'CreateToolhelp32Snapshot', H, [U, U])
open_thread = api(k, 'OpenThread', H, [U, C.c_int, U])
thread_times = api(k, 'GetThreadTimes', C.c_int, [H, C.POINTER(Q), C.POINTER(Q), C.POINTER(Q), C.POINTER(Q)])
suspend = api(k, 'SuspendThread', U, [H])
resume = api(k, 'ResumeThread', U, [H])
context = api(k, 'GetThreadContext', C.c_int, [H, H])
read_memory = api(k, 'ReadProcessMemory', C.c_int, [H, H, H, C.c_size_t, C.POINTER(C.c_size_t)])

class ModuleInfo(C.Structure):
    _fields_ = [('base', H), ('size', U), ('entry', H)]

enum_modules = api(k, 'K32EnumProcessModulesEx', C.c_int, [H, C.POINTER(H), U, C.POINTER(U), U])
module_info = api(k, 'K32GetModuleInformation', C.c_int, [H, H, C.POINTER(ModuleInfo), U])
module_name = api(k, 'K32GetModuleFileNameExW', U, [H, H, C.c_wchar_p, U])

class Entry(C.Structure):
    _fields_ = [('size', U), ('usage', U), ('tid', U), ('pid', U),
                ('priority', C.c_int32), ('delta', C.c_int32), ('flags', U)]

first = api(k, 'Thread32First', C.c_int, [H, C.POINTER(Entry)])
next_thread = api(k, 'Thread32Next', C.c_int, [H, C.POINTER(Entry)])

class Symbol(C.Structure):
    _fields_ = [('SizeOfStruct', U), ('TypeIndex', U), ('Reserved', Q*2),
                ('Index', U), ('Size', U), ('ModBase', Q), ('Flags', U),
                ('Value', Q), ('Address', Q), ('Register', U), ('Scope', U),
                ('Tag', U), ('NameLen', U), ('MaxNameLen', U), ('Name', C.c_char*1)]

assert C.sizeof(Entry) == 28 and C.sizeof(Symbol) == 88 and Symbol.Name.offset == 84
sym_options = api(d, 'SymSetOptions', U, [U])
sym_init = api(d, 'SymInitializeW', C.c_int, [H, C.c_wchar_p, C.c_int])
sym_cleanup = api(d, 'SymCleanup', C.c_int, [H])
sym_from = api(d, 'SymFromAddr', C.c_int, [H, Q, C.POINTER(Q), C.POINTER(Symbol)])

def times(handle):
    created, exited, kernel, user = Q(), Q(), Q(), Q()
    if not thread_times(handle, C.byref(created), C.byref(exited), C.byref(kernel), C.byref(user)):
        raise C.WinError(C.get_last_error())
    return created.value, (kernel.value+user.value)/1e7

process = open_process(0x410, False, a.pid)  # Query information + VM read only.
if not process:
    raise C.WinError(C.get_last_error())
thread = None
symbols_started = False
samples = []
stack_samples = []
try:
    path = C.create_unicode_buffer(32768); size = U(len(path))
    if not query_image(process, 0, path, C.byref(size)):
        raise C.WinError(C.get_last_error())
    if Path(path.value).resolve() != expected:
        raise ValueError('PID does not match the current development executable')
    handles = (H*1024)(); needed = U()
    if not enum_modules(process, handles, C.sizeof(handles), C.byref(needed), 3):
        raise C.WinError(C.get_last_error())
    if needed.value > C.sizeof(handles):
        raise ValueError('Module list exceeded bounded capacity')
    modules = []
    for handle in handles[:needed.value//C.sizeof(H)]:
        info = ModuleInfo(); name = C.create_unicode_buffer(32768)
        if module_info(process, handle, C.byref(info), C.sizeof(info)) and module_name(process, handle, name, len(name)):
            modules.append((info.base, info.size, str(Path(name.value))))
    # Preserve address mapping even if the watchdog terminates a diagnostic
    # before symbol aggregation. Raw stack words still are not an unwind.
    import hashlib
    module_records = []
    for base,size,path in modules:
        record = dict(base=base,size=size,path=path)
        try:
            record['disk_sha256'] = hashlib.sha256(Path(path).read_bytes()).hexdigest()
        except OSError as error:
            record['disk_hash_error'] = str(error)
        module_records.append(record)
    (archive/'sample-inputs.json').write_text(json.dumps(dict(
        pid=a.pid, executable=str(expected), exe_sha256=hashlib.sha256(pe).hexdigest(),
        stack_words_requested=a.stack_word_count if a.stack_words else 0,
        modules=module_records),indent=2))
    snap = snapshot(4, 0)
    if snap == C.c_void_p(-1).value:
        raise C.WinError(C.get_last_error())
    candidates = []
    try:
        entry = Entry(); entry.size = C.sizeof(entry)
        ok = first(snap, C.byref(entry))
        while ok:
            if entry.pid == a.pid:
                handle = open_thread(0x4a, False, entry.tid)  # Query/context/suspend; no set-context.
                if handle:
                    try:
                        candidates.append((times(handle)[0], entry.tid))
                    finally:
                        close(handle)
            entry.size = C.sizeof(entry)
            ok = next_thread(snap, C.byref(entry))
    finally:
        close(snap)
    if not candidates:
        raise ValueError('No accessible game threads')
    if a.thread_id is not None and a.thread_id not in {row[1] for row in candidates}:
        raise ValueError('Requested thread is not an accessible thread of the verified game process')
    tid = a.thread_id if a.thread_id is not None else min(candidates)[1]
    thread = open_thread(0x4a, False, tid)
    if not thread:
        raise C.WinError(C.get_last_error())
    # AMD64 CONTEXT: aligned 16, flags at48, RIP at248, total1232 bytes.
    backing = C.create_string_buffer(1232+15)
    pointer = (C.addressof(backing)+15) & ~15
    flags = U.from_address(pointer+48)
    rip = Q.from_address(pointer+248)
    rsp = Q.from_address(pointer+152)
    words = (Q*a.stack_word_count)(); got = C.c_size_t()
    start_cpu = times(thread)[1]
    started = time.perf_counter(); paused_seconds = 0
    rng = random.Random(52)
    while time.perf_counter()-started < a.seconds:
        flags.value = 0x100003  # AMD64 control + integer; read only.
        before = time.perf_counter()
        prior = suspend(thread)
        if prior == 0xffffffff:
            raise C.WinError(C.get_last_error())
        try:
            if not context(thread, pointer):
                raise C.WinError(C.get_last_error())
            address = rip.value
            stack = None
            if a.stack_words:
                stack_address = rsp.value
                # CONTEXT integer registers RAX..R15 occupy offsets120..240;
                # retain nonvolatile values for later verified frame unwinding.
                registers = {name:Q.from_address(pointer+120+8*i).value for i,name in
                             enumerate(('rax','rcx','rdx','rbx','rsp','rbp','rsi','rdi',
                                        'r8','r9','r10','r11','r12','r13','r14','r15'))}
                # Do not let a later inaccessible page discard an earlier valid
                # prefix. No permissions are changed to obtain deeper stacks.
                copied = 0
                while copied < C.sizeof(words):
                    cursor = stack_address+copied
                    length = min(C.sizeof(words)-copied, mmap.PAGESIZE-cursor%mmap.PAGESIZE)
                    got.value = 0
                    success = read_memory(process, cursor, C.byref(words,copied), length, C.byref(got))
                    copied += min(got.value,length)
                    if not success or got.value != length:
                        break
                stack = list(words)[:copied//C.sizeof(Q)]
        finally:
            if resume(thread) == 0xffffffff:
                raise C.WinError(C.get_last_error())
        paused_seconds += time.perf_counter()-before
        samples.append(address)
        if a.stack_words:
            stack_samples.append(dict(rip=address, rsp=stack_address, words=stack,
                                      registers=registers,
                                      words_read=len(stack or []), words_requested=a.stack_word_count))
        time.sleep(rng.uniform(.004, .011))
    elapsed = time.perf_counter()-started
    cpu_seconds = times(thread)[1]-start_cpu
    # Local exact symbols only; ignore any configured symbol-server paths.
    sym_options(0x4 | 0x200 | 0x400 | 0x1000 | 0x80000 | 0x02000000)
    if not sym_init(process, str(expected.parent), True):
        raise C.WinError(C.get_last_error())
    symbols_started = True
    counts = collections.Counter()
    module_counts = collections.Counter()
    for address, count in collections.Counter(samples).items():
        mod_base, mod_path = next(((base, path) for base, size, path in modules if base <= address < base+size), (0, 'unknown'))
        module_counts[mod_path] += count
        buf = C.create_string_buffer(C.sizeof(Symbol)+1024)
        symbol = C.cast(buf, C.POINTER(Symbol))
        symbol.contents.SizeOfStruct = C.sizeof(Symbol); symbol.contents.MaxNameLen = 1024
        displacement = Q()
        if sym_from(process, address, C.byref(displacement), symbol):
            name = C.string_at(C.addressof(buf)+Symbol.Name.offset, symbol.contents.NameLen).decode(errors='replace')
            key = (name, symbol.contents.Address, symbol.contents.ModBase, mod_path)
        else:
            key = ('unresolved', address, mod_base, mod_path)
        counts[key] += count
    report = dict(pid=a.pid, thread_id=tid, executable=str(expected), seconds=elapsed,
                  samples=len(samples), pause_call_seconds=paused_seconds, main_thread_cpu_seconds=cpu_seconds,
                  limitation='Leaf instruction-pointer samples, including waits; not inclusive timing or an FPS benchmark.',
                  modules=[dict(path=path, samples=count, percent=100*count/len(samples)) for path, count in module_counts.most_common()],
                  functions=[dict(name=n, address=hex(addr), module_base=hex(mod), module_path=path, samples=count,
                                  percent=100*count/len(samples)) for (n, addr, mod, path), count in counts.most_common()])
    if a.stack_words:
        image_base, image_size, _ = next(m for m in modules if Path(m[2]).resolve() == expected)
        candidate_symbols = {}; caller_counts = collections.Counter()
        for sample in stack_samples:
            for index, value in enumerate(sample['words'] or []):
                if not image_base <= value < image_base+image_size:
                    continue
                if value not in candidate_symbols:
                    buf = C.create_string_buffer(C.sizeof(Symbol)+1024); symbol = C.cast(buf, C.POINTER(Symbol))
                    symbol.contents.SizeOfStruct = C.sizeof(Symbol); symbol.contents.MaxNameLen = 1024
                    displacement = Q(); candidate_symbols[value] = None
                    if sym_from(process, value, C.byref(displacement), symbol) and symbol.contents.Tag == 5:
                        name = C.string_at(C.addressof(buf)+Symbol.Name.offset, symbol.contents.NameLen).decode(errors='replace')
                        candidate_symbols[value] = name
                name = candidate_symbols[value]
                if name:
                    caller_counts[(sample['rip'], name, index)] += 1
                    break
        report['stack_limitation'] = 'First matching symbols can be stale stack storage (observed in profile266). These are unverified leads, never active caller attribution; verify unwind/prologue and preceding CALL.'
        report['stack_candidates'] = [dict(sample_rip=hex(ip), candidate=name, word_index=index, samples=count)
                                      for (ip, name, index), count in caller_counts.most_common()]
    (archive/'main-thread-samples.json').write_text(json.dumps(report, indent=2))
    display = {**report, 'functions': report['functions'][:30]}
    if 'stack_candidates' in display:
        display['stack_candidates'] = display['stack_candidates'][:30]
    print(json.dumps(display, indent=2))
finally:
    if symbols_started:
        sym_cleanup(process)
    if thread:
        close(thread)
    close(process)
    (archive/'main-thread-samples-raw.json').write_text(json.dumps(samples))
    if a.stack_words:
        (archive/'main-thread-stack-words.json').write_text(json.dumps(stack_samples))
