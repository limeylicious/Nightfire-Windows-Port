"""Bounded native boot via the project run script; retain each attempt's logs."""
from pathlib import Path
import ctypes, datetime, hashlib, json, os, shutil, subprocess, sys, time
root = Path(__file__).resolve().parents[1]
sys.stdout.reconfigure(encoding='utf-8', errors='replace')
label = sys.argv[1] if len(sys.argv) > 1 else 'boot'
seconds = int(sys.argv[2]) if len(sys.argv) > 2 else 20
assert 5 <= seconds <= 180, 'Diagnostic watchdog must be5..180 seconds'
assert all(c.isalnum() or c in '-_' for c in label)
run = root / 'analysis/runs' / (datetime.datetime.now().strftime('%Y%m%d-%H%M%S-') + label)
run.mkdir(parents=True, exist_ok=False)
shutil.copy2(root/'run-windows.cmd',run/'run-windows.cmd')
exe = root / 'build-windows/RelWithDebInfo/nightfire_driving.exe'
xbe=root.parent/'nightfire-port/game_files/Driving.xbe'
xbe_hash=hashlib.sha256(xbe.read_bytes()).hexdigest()
assert xbe_hash=='0f4c50e4f84b1edeedab4667933b36c9c228da463cc89180e4c70cf3a88bfcef','Review changed Driving.xbe before running'
class PowerStatus(ctypes.Structure):
    _fields_ = [('ac', ctypes.c_ubyte), ('flags', ctypes.c_ubyte),
                ('percent', ctypes.c_ubyte), ('reserved', ctypes.c_ubyte),
                ('life', ctypes.c_uint32), ('full', ctypes.c_uint32)]
power = PowerStatus()
power_observation = (dict(ac_online=power.ac, battery_percent=power.percent,
                          battery_flags=power.flags)
                     if ctypes.windll.kernel32.GetSystemPowerStatus(ctypes.byref(power)) else None)
start = time.monotonic()
# Children inherit the error mode: crash logs, not blocking Windows error dialogs.
ctypes.windll.kernel32.SetErrorMode(0x0001 | 0x0002 | 0x8000)
with (run / 'launcher.log').open('w', encoding='utf-8') as log:
    env = dict(os.environ, DRIVING_CAPTURE_DIR=str(run), RECOMP_WATCHDOG_SECS=str(seconds))
    env.setdefault('RECOMP_AC97_READY','1')
    env.setdefault('DRIVING_INPUT224','1')
    env.setdefault('DRIVING_DIAGNOSTIC_DSP149','1')
    env.pop('RECOMP_APU_DSP_ACK',None)
    if env.get('DRIVING_LEAN_TEST240')=='1':
        env.pop('RECOMP_FB_DUMP',None)
        env.pop('RECOMP_APU_TRACE',None)
    else:env['RECOMP_FB_DUMP']=str(run/'framebuffer')
    if env.get('DRIVING_NO_LAUNCH144')=='1':env.pop('DRIVING_LAUNCH_PAYLOAD144',None)
    else:env.setdefault('DRIVING_LAUNCH_PAYLOAD144',str(root/'launch-data/original-first-paris144.bin'))
    if env.get('DRIVING_LAUNCH_PAYLOAD144'):
        payload=Path(env['DRIVING_LAUNCH_PAYLOAD144']).resolve()
        shutil.copy2(payload,run/'launch-payload-a50.bin')
        (run/'launch-input.json').write_text(json.dumps(dict(source=str(payload),
            sha256=hashlib.sha256(payload.read_bytes()).hexdigest()),indent=2),encoding='utf-8')
    (run/'diagnostic-environment.json').write_text(json.dumps(dict(
        xbe_sha256=xbe_hash, watchdog_seconds=seconds, power_at_start=power_observation,
        optional_flags={name:env.get(name) for name in (
            'DRIVING_DEPTH_PING272','DRIVING_DRAIN_MAP274','DRIVING_NO_LANE_FLUSH275','DRIVING_COLOR_SEED276','DRIVING_NATIVE_INDEXED277','DRIVING_DEPTH_SEED278','DRIVING_NATIVE_PROFILE277','DRIVING_BEGIN280','DRIVING_SEMAPHORE281','DRIVING_PALETTE_CAPTURE288','DRIVING_ADMISSION283','DRIVING_REMAINING_CAPTURE290','DRIVING_FRAGMENT_DEPTH291','DRIVING_MATERIAL292',
            'DRIVING_READBACK294','DRIVING_ASYNC_TAIL295','DRIVING_GPU_SAMPLE297','DRIVING_GPU_SAMPLE297_BATCH','DRIVING_GPU_SAMPLE297_MIN_DRAWS','DRIVING_LIGHTING_CAPTURE298','DRIVING_LIGHTING301','DRIVING_LIGHTING304','DRIVING_NATIVE_DEPTH302','DRIVING_INACTIVE_STENCIL306',
            'DRIVING_VALIDATION267','DRIVING_DEPTH_IMPORT268','DRIVING_DEPTH_CLEAN269',
            'DRIVING_COMMAND262','DRIVING_UNHANDLED263','DRIVING_BATCH_MAP264','DRIVING_COMMAND_READ265',
            'DRIVING_HOST_SPANS261','DRIVING_BATCH32_260','DRIVING_MAPPING259',
            'DRIVING_DEPTH_SRV256','DRIVING_BATCH_CENSUS255','DRIVING_GPU_SAMPLE254','DRIVING_GPU_SAMPLE254_BATCH',
            'DRIVING_DESCRIPTOR252','DRIVING_OWNED253','DRIVING_TIMING250','DRIVING_QUEUE_TIMING251',
            'DRIVING_NATIVE247','DRIVING_NATIVE_TIMING247','DRIVING_PAIR_BATCH248','DRIVING_MATERIAL_RING246','DRIVING_COMMAND245','DRIVING_BATCH_TIMING244','DRIVING_OFFSCREEN_GPU243','DRIVING_OFFSCREEN_CAPTURE243','DRIVING_ROAD_WHITE242','DRIVING_BARRIER242','DRIVING_LATE_CAPTURE240','DRIVING_DEV_SKIP_PARIS_INTRO240','DRIVING_LEAN_TEST240','RECOMP_FB_DUMP','RECOMP_AC97_READY','RECOMP_APU_TRACE','RECOMP_APU_DSP_ACK','DRIVING_PRODUCER235','DRIVING_BATCH236',
            'DRIVING_NO_LAUNCH144','DRIVING_DIAGNOSTIC_DSP149','DRIVING_TEXTURE_CAPTURE167','DRIVING_MOVIE_TRACE180','DRIVING_MOVIE_CAPTURE198','DRIVING_MOVIE_GPU201','DRIVING_MOVIE_WINDOW201','DRIVING_CADENCE204','DRIVING_FLIP204','DRIVING_POSTMOVIE_TRACE207','DRIVING_WORLD_CAPTURE210','DRIVING_WORLD_GPU214','DRIVING_WORLD_GPU216','DRIVING_WORLD_GPU220','DRIVING_WORLD_GPU221','DRIVING_RESOLVE_CAPTURE222','DRIVING_WORLD_CAPTURE223','DRIVING_INPUT224','DRIVING_WORLD_CAPTURE_SURFACE214','DRIVING_WORLD_CAPTURE_BATCH217','DRIVING_WORLD_CAPTURE_BATCH218','DRIVING_WORLD_CAPTURE_VISIBLE218','DRIVING_WORLD_CAPTURE_SCENERY218','DRIVING_WORLD_CAPTURE_BATCH219','DRIVING_WORLD_CAPTURE_BATCH221')}),indent=2),encoding='utf-8')
    process = subprocess.Popen([os.environ['COMSPEC'], '/d', '/c', str(root / 'run-windows.cmd')], cwd=root, env=env, stdout=log, stderr=subprocess.STDOUT)
    timed_out = False
    try:
        code = process.wait(timeout=seconds+15)
    except subprocess.TimeoutExpired:
        timed_out = True
        subprocess.run(['taskkill', '/PID', str(process.pid), '/T', '/F'], stdout=log, stderr=subprocess.STDOUT)
        code = process.wait(timeout=10)
for name in ('driving-startup.log', 'xbox_kernel.log'):
    source = root / 'logs' / name
    if source.exists():
        shutil.copy2(source, run / name)
result = dict(label=label, exit_code=code, external_timeout=timed_out, seconds=round(time.monotonic()-start,3), exe_sha256=hashlib.sha256(exe.read_bytes()).hexdigest())
(run / 'result.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
print(run)
print(json.dumps(result))
startup = (run / 'driving-startup.log').read_text(encoding='utf-8', errors='replace')
for line in startup.splitlines():
    if any(marker in line for marker in ('[BOOT]', '[DRIVING', '[ICALL]', '[CRASH]', '[DVD]', '[GPU145]', '[MOVIE144]', 'unbridged', 'watchdog', 'HalReturnToFirmware')):
        print(line)
paths=[line for line in startup.splitlines() if '[PATH]' in line]
reads=sum('[READ]' in line for line in startup.splitlines())
print(f'Full logs retained: {len(paths)} path records, {reads} read records.')
for line in paths[-6:]:print(line)
print(startup[-1400:])
