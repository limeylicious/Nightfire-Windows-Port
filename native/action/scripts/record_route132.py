"""Passive PAL player route/camera guide, not deterministic input replay.

Reads only an explicitly identified game process. No process writes, global key
logging, input injection or game lifetime control. Samples are asynchronous;
frame/pointer checks mark transitions rather than claiming an atomic snapshot.
"""
from pathlib import Path
import argparse
import ctypes as C
import hashlib
import json
import math
import struct
import time


def span(address, size):
    return (0x1000 <= address and address + size <= 0x4000000) or (
        0x80000000 <= address and address + size <= 0x84000000)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--pid', type=int, required=True)
    parser.add_argument('--exe', type=Path, required=True)
    parser.add_argument('--offset', type=lambda s: int(s, 0), required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--seconds', type=int, default=1200)
    args = parser.parse_args()
    if not 1 <= args.seconds <= 1800 or args.pid <= 0 or args.offset <= 0:
        parser.error('positive PID/offset and 1..1800 seconds required')
    exe = args.exe.resolve(strict=True)
    if exe.name.lower() != 'nightfire_diagnostic.exe':
        parser.error('Expected a saved nightfire_diagnostic.exe')
    k = C.WinDLL('kernel32', use_last_error=True)
    k.OpenProcess.argtypes = [C.c_uint32, C.c_int, C.c_uint32]
    k.OpenProcess.restype = C.c_void_p
    k.CloseHandle.argtypes = [C.c_void_p]
    k.QueryFullProcessImageNameW.argtypes = [C.c_void_p, C.c_uint32, C.c_wchar_p, C.POINTER(C.c_uint32)]
    k.ReadProcessMemory.argtypes = [C.c_void_p, C.c_void_p, C.c_void_p, C.c_size_t, C.POINTER(C.c_size_t)]
    k.WaitForSingleObject.argtypes = [C.c_void_p, C.c_uint32]
    k.GetTickCount64.restype = C.c_uint64
    # Query + read + synchronize; never request VM_WRITE/VM_OPERATION.
    handle = k.OpenProcess(0x101010, False, args.pid)
    if not handle:
        raise C.WinError(C.get_last_error())
    try:
        name = C.create_unicode_buffer(32768)
        length = C.c_uint32(len(name))
        if not k.QueryFullProcessImageNameW(handle, 0, name, C.byref(length)):
            raise C.WinError(C.get_last_error())
        if Path(name.value).resolve() != exe:
            raise ValueError('PID does not match the explicitly selected executable')

        def read(address, size):
            if not span(address, size):
                raise ValueError('Out-of-range guest pointer')
            raw = C.create_string_buffer(size)
            got = C.c_size_t()
            if not k.ReadProcessMemory(handle, args.offset + address, raw, size, C.byref(got)) or got.value != size:
                raise C.WinError(C.get_last_error())
            return raw.raw

        def u32(address):
            return struct.unpack('<I', read(address, 4))[0]

        # Fail before creating output if the supplied mapping is not readable.
        read(0x1f65b0, 16)
        args.out.mkdir(parents=True, exist_ok=False)
        marker = args.out/'marker.txt'
        marker.write_text('start')
        count = valid = crossed = errors = 0
        start = time.monotonic()
        reason = 'duration'
        with (args.out/'route.jsonl').open('x', encoding='utf-8', buffering=1) as stream:
            def emit(row):
                stream.write(json.dumps(row, allow_nan=False) + '\n')
            emit(dict(type='header', version=1, pid=args.pid, executable=str(exe),
                      sha256=hashlib.sha256(exe.read_bytes()).hexdigest(), offset=hex(args.offset),
                      sample_hz=10, scope='Asynchronous player position/orientation guide; no raw mouse or full input replay'))
            print('Route recorder ready: '+str(args.out), flush=True)
            deadline = start
            try:
                while time.monotonic() - start < args.seconds:
                    wait = k.WaitForSingleObject(handle, 0)
                    if wait == 0:
                        reason = 'game-exited'; break
                    if wait != 258:
                        raise C.WinError(C.get_last_error())
                    row = dict(type='sample', time_ms=k.GetTickCount64(),
                               marker=marker.read_text(errors='replace')[:160].strip())
                    try:
                        frame, tick, _, _ = struct.unpack('<4I', read(0x1f65b0, 16))
                        row.update(frame=frame, game_tick=tick, paused=struct.unpack('<H', read(0x1fec64, 2))[0])
                        player = u32(0x1f6654)
                        if 0x80000000 <= player and span(player, 0x100):
                            data = read(player, 0x100)
                            xyz = struct.unpack_from('<3f', data, 0x24)
                            yaw = struct.unpack_from('<f', data, 0x40)[0]
                            if all(math.isfinite(v) and abs(v) < 10000 for v in (*xyz, yaw)):
                                row.update(player=hex(player), position=xyz, yaw=yaw,
                                           mode=struct.unpack_from('<h', data, 0xd2)[0])
                            state = struct.unpack_from('<I', data, 0xbc)[0]
                            if span(state, 0x8e0):
                                pitch, = struct.unpack('<f', read(state+0x838, 4))
                                zoom, = struct.unpack('<f', read(state+0x860, 4))
                                if math.isfinite(pitch) and math.isfinite(zoom):
                                    row.update(pitch= pitch, zoom=zoom)
                        row['player_changed'] = u32(0x1f6654) != player
                        row['frame_after'] = u32(0x1f65b0)
                        row['crossed_frame'] = row['frame_after'] != frame
                        if row['player_changed']:
                            for key in ('position', 'yaw', 'pitch', 'zoom', 'mode'):
                                row.pop(key, None)
                        crossed += row['crossed_frame']
                        valid += 'position' in row
                    except (OSError, ValueError, struct.error) as error:
                        # Loading can invalidate observed pointers between reads.
                        row = dict(type='unavailable', time_ms=row['time_ms'], detail=str(error))
                        errors += 1
                    emit(row); count += 1
                    if (args.out/'stop.txt').exists():
                        reason = 'requested-stop'; break
                    deadline += .1
                    time.sleep(max(0, deadline-time.monotonic()))
                    if deadline < time.monotonic()-.1:
                        deadline = time.monotonic()
            except KeyboardInterrupt:
                reason = 'interrupted'
            finally:
                summary = dict(type='end', samples=count, position_samples=valid,
                               crossed_frames=crossed, unavailable=errors, reason=reason)
                emit(summary)
                (args.out/'summary.json').write_text(json.dumps(summary, indent=2))
                print(summary, flush=True)
    finally:
        k.CloseHandle(handle)


if __name__ == '__main__':
    main()
