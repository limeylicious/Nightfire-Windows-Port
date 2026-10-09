"""Action native-audio survey, step 3: build action-dsound-map.json from work-dsound-match.json,
work-glue19.json and work-dsound-survey.json (run dsound_action_map.py and
dsound_action_survey.py first). Read-only on the project; writes into this folder only.
"""
import json, re, sys, hashlib
from pathlib import Path
HERE = Path(__file__).resolve().parent; ROOT = HERE.parents[1]
sys.path.insert(0, str(HERE))
import dsound_action_map as M

# XMediaObject / IDirectSoundStream vtable order (XDK); confirmed by the slot bodies (see survey)
STREAM_SLOTS = ['AddRef', 'Release', 'GetInfo', 'GetStatus', 'Process', 'Discontinuity', 'Flush']
XMV_SLOT_USE = {0: ['0x1305FA (0x130488)', '0x13061F (0x130605)'], 1: ['0x1304E9 (0x130488)', '0x1303B6 (0x130393)'],
                4: ['0x130803 (0x130624)'], 5: ['0x13082E (0x130624)', '0x130C1E (0x130624)']}
# NATIVE-AUDIO-PLAN.md "Per-game address tables" (Action) and cautions, as written there
PLAN = [('DirectSoundCreate', 0x1148B8), ('DownloadEffectsImage', 0x11338B), ('CreateSoundBuffer', 0x1146FE),
        ('DoWork', 0x1134FD), ('listener SetPosition', 0x11437E), ('listener SetVelocity', 0x1143B3),
        ('listener SetOrientation', 0x114334), ('CommitDeferredSettings', 0x113C13),
        ('buffer basics (range start) SetVolume', 0x1133CA), ('buffer basics (range end) SetCurrentPosition', 0x1134D2),
        ('SetBufferData', 0x1143E8), ('SetFrequency', 0x113C2B), ('SetHeadroom', 0x1133E6),
        ('ds_setheadroom row: SetHeadroom', 0x112A92), ('3D setters (range start) SetMaxDistance', 0x113C47),
        ('3D setters (range end) SetI3DL2Source', 0x113D1D), ('DirectSoundCreateStream', 0x1148FF),
        ('stream vtable', 0x16264C), ('stream SetVolume', 0x1134EE), ('stream SetMixBins', 0x1134F3),
        ('stream Pause', 0x1134F8), ('SynchPlayback', 0x1133B2), ('stream slot 5 Discontinuity', 0x112FD3),
        ('stream slot 6 Flush', 0x113020)]
# how the existing Action runtime treats each VA today (nightfire_game_audio.h / nightfire_video_audio.c)
EXISTING = {
    0x11343A: 'REPLACED for registered+decoded buffers (prologue in generated sub_0011343A -> nightfire_game_audio_call); else original; after-hook repeats game_play (guarded no-op)',
    0x114647: 'observed after (inner CDirectSound_CreateSoundBuffer): registers buffer; original runs',
    0x1146FE: 'original runs; its inner call 0x114647 is observed after and registers the buffer (format from the descriptor)',
    0x1143E8: 'mirrored after: decode to host PCM (or defer incomplete ADPCM); original runs',
    0x11345E: 'mirrored after: native stop/destroy; original runs',
    0x113496: 'original runs, then *pdwStatus overwritten with native status',
    0x113476: 'mirrored after; original runs',
    0x1134B2: 'original runs, then both cursors overwritten with native cursor',
    0x1134D2: 'mirrored after (destroy + replay); original runs',
    0x1133CA: 'mirrored after; original runs',
    0x113C2B: 'mirrored after; original runs',
    0x112749: 'mirrored after (clear); original runs; DSOUND-internal caller only',
    0x1133E6: 'mirrored after only with NIGHTFIRE_NATIVE_GAME_SPATIAL=1; original runs',
    0x113C47: 'mirrored after (distance gain) only with SPATIAL; original runs',
    0x113C6B: 'mirrored after (distance gain) only with SPATIAL; original runs',
    0x113C8F: 'mirrored after (distance gain) only with SPATIAL; original runs',
    0x113CF9: 'mirrored after (distance gain) only with SPATIAL; original runs',
    0x11437E: 'mirrored after (listener position) only with SPATIAL; original runs',
    0x113C13: 'mirrored after (apply deferred) only with SPATIAL; original runs',
    0x1148FF: 'observed after (thread_check video_progress): nightfire_audio_attach registers XMV stream (callback 0x130437) for waveOut; original runs',
    0x1134F8: 'observed before: waveOutPause/Restart (mode 2 = wait for SynchPlayback); original runs',
    0x1133B2: 'observed before: waveOutRestart of waiting streams; original runs',
    0x1134FD: 'observed before: poll waveOut completions, run XMV callback; original runs',
    0x112ED7: 'observed before (vtable slot 0): reference count; original runs',
    0x112F1E: 'REPLACED via recomp_lookup_manual for registered streams: last release closes waveOut, then calls original',
    0x1130BE: 'REPLACED via recomp_lookup_manual for registered streams: decode + waveOutWrite',
    0x112FD3: 'REPLACED via recomp_lookup_manual for registered streams, but treated as FLUSH (waveOutReset, packets completed 0x80004004) - wrong: slot 5 is Discontinuity',
}


def h(v): return None if v is None else f'0x{v:06X}'


def main():
    w = json.load(open(HERE / 'work-dsound-match.json'))
    g19 = json.load(open(HERE / 'work-glue19.json'))
    s = json.load(open(HERE / 'work-dsound-survey.json'))
    alab = {int(e['address'], 16): e['research_name'] for e in json.loads(M.ALAB.read_text())['entries']}
    d2a = {int(k, 16): int(v[0], 16) for k, v in w['match'].items()}
    a2d = {a: d for d, a in d2a.items()}
    dn = M.dsym()
    ax = M.Xbe(M.AXBE)
    info, bodies = M.gen_funcs(M.AGEN)
    covered = {int(r['action_va'], 16): r['glue_handler'] for r in g19 if r['action_va']}

    def inner(va):
        b = bodies.get(va, '')
        m = re.search(r'RECOMP_ABI_CALL\(0x([0-9A-F]+)u, sub_[0-9A-F]+\);[^\n]*\n(?:(?!RECOMP_ABI_CALL).)*?/\* ret', b, re.S)
        calls = [int(x, 16) for x in re.findall(r'RECOMP_ABI_CALL\(0x([0-9A-F]+)u', b)]
        tails = [int(x, 16) for x in re.findall(r'sub_([0-9A-F]{8})\(\); return; /\* tail jmp', b)]
        c = [x for x in calls + tails if x not in (0x1126C3,)]
        return c[-1] if c else None

    plan_vas = {v for _, v in PLAN}
    in_plan = lambda va: va in plan_vas or 0x1133CA <= va <= 0x1134D2 or 0x113C47 <= va <= 0x113D1D
    eps = []
    for r in s['entry_points']:
        va = int(r['va'], 16)
        if not (r['callers'] or r['tail_callers']): continue
        if va == 0x116524: continue        # reached only through the .text adjustor thunk 0x0EE225 in a DSOUND vtable
        d = a2d.get(va)
        inn = inner(va)
        callers = {}
        for sec, cs in r['callers'].items():
            callers[sec] = [f'{c} {alab.get(int(c, 16), "")}'.strip() for c in cs]
        name = dn.get(d) if d else alab.get(va, '')
        eps.append(dict(
            va=r['va'], name=name, name_source=('Driving symbol, deep-equal code' if d else 'research label (lead), wrapper shape + callee label checked'),
            nargs_stdcall=(r['nargs'][0] if r['nargs'] else None), ret_bytes=(r['ret_bytes'][0] if r['ret_bytes'] else None),
            inner_call=(f'{h(inn)} {alab.get(inn, "")}'.strip() if inn else None),
            callers=callers, driving_va=r['driving_va'],
            driving_native=(covered.get(va) or ('no - not linked in Driving' if not d else 'no - in Driving DSOUND but not in the 19')),
            in_plan=in_plan(va), existing_action_layer=EXISTING.get(va, 'none (original DSOUND only)')))
    # stream vtable
    vt = 0x16264C
    slots = []
    for i, nm in enumerate(STREAM_SLOTS):
        f = ax.u32(vt + 4 * i)
        inn = inner(f)
        slots.append(dict(slot=i, offset=f'+0x{4*i:X}', method=nm, va=h(f), inner_call=(f'{h(inn)} {alab.get(inn, "")}'.strip() if inn else None),
                          xmv_call_sites=XMV_SLOT_USE.get(i, []), existing_action_layer=EXISTING.get(f, 'none (original DSOUND only)'),
                          driving_native='no - Driving has no streams'))
    plan = []
    epmap = {int(e['va'], 16): e for e in eps}
    for nm, va in PLAN:
        if va == vt:
            ok = all(ax.u32(vt + 4 * i) for i in range(7)); note = 'stream vtable in .rdata; 7 slots ' + ', '.join(f'{x["method"]}={x["va"]}' for x in slots)
        elif va in epmap:
            e = epmap[va]; ok = True
            note = f'{e["name"]}, {e["nargs_stdcall"]} args; called from ' + '; '.join(f'{k}: {", ".join(v)}' for k, v in e['callers'].items())
        elif va in (0x112FD3, 0x113020):
            ok = True; note = f'vtable slot; body calls {inner(va) and h(inner(va))} {alab.get(inner(va), "")}'
        elif va == 0x112A92:
            ok = False; note = 'CDirectSoundVoice_SetHeadroom (inner method); the game-facing entry is IDirectSoundBuffer_SetHeadroom 0x1133E6'
        else:
            ok = False; note = 'not an externally called entry point'
        plan.append(dict(plan_item=nm, va=h(va), verified=ok, label=alab.get(va, ''), note=note))
    def nargs_of(va):
        r = sorted({int(m.group(1), 0) for m in re.finditer(r'/\* ret (0x[0-9A-Fa-f]+|\d+) \*/', bodies.get(va, ''))})
        return r[0] // 4 if r else 0
    nargs_ok = all(nargs_of(int(r['action_va'], 16)) == r['nargs_glue'] for r in g19 if r['action_va'])
    print('glue nargs == Action ret N for all mapped wrappers:', nargs_ok)
    out = dict(
        meta=dict(action_xbe='nightfire-analysis/game_files/default.xbe (= nightfire-port/game_files/default.xbe)', action_sha256=w['meta']['action_sha'],
                  driving_xbe='nightfire-analysis/game_files/Driving.xbe', driving_sha256=w['meta']['driving_sha'],
                  action_dsound_section='0x112600-0x12FD4C (code 0x112600-0x11C801; tables to 0x12F000; globals 0x12F000-0x12FD4C, BSS from 0x12FB04)',
                  driving_dsound_section='0x17AC40-0x183AA4',
                  method='masked machine-code match (image addresses and external call targets ignored), then deep check: the whole call tree must be equal modulo addresses',
                  driving_funcs=w['meta']['d_funcs'], action_dsound_funcs=w['meta']['a_funcs'], matched=len(w['match']),
                  scripts=['dsound_action_map.py', 'dsound_action_survey.py', 'dsound_action_export.py']),
        driving_glue_19=[dict(driving_va=r['driving_va'], driving_symbol=r['driving_symbol'], glue_handler=r['glue_handler'], nargs=r['nargs_glue'],
                              action_va=r['action_va'], research_label=r['research_label'],
                              confidence=('exact masked match, call tree deep-equal' if r['deep'] == 'deep-equal' else r['confidence']),
                              length=[r['length_driving'], r['length_action']],
                              action_nargs=(nargs_of(int(r['action_va'], 16)) if r['action_va'] else None),
                              action_callers=(epmap[int(r['action_va'], 16)]['callers'] if r['action_va'] and int(r['action_va'], 16) in epmap
                                              else ({'DSOUND-internal only': ['0x113DC9 (CDirectSound destructor, vtable 0x162618)']} if r['action_va'] == '0x112749' else {})))
                         for r in g19],
        action_entry_points=eps,
        stream_vtable=dict(at=h(vt), slots=slots),
        plan_check=plan,
        not_linked_in_action=[dict(driving_va=k, symbol=dn.get(int(k, 16), '') or 'table bytes the generator decoded as code (not a routine)') for k in sorted(set(f'0x{v:06X}' for v in [int(x, 16) for x in w['lengths']['driving']]) - set(w['match']))],
        globals=dict(
            action_dsound=[
                dict(va='0x12FB98', driving='0x1838F0', name='DSOUND CDirectSound singleton pointer (read by DirectSoundDoWork 0x1134FD; Driving probe calls it global.device)', game_access='none'),
                dict(va='0x12F50C', driving='0x183264', name='DSOUND "unusable" flag: every C++ method returns 0x80004005 when non-zero (41 users)', game_access='none'),
                dict(va='0x12F518', driving='0x183270', name='DSOUND critical-section object (DirectSoundEnterCriticalSection 0x1126C3, imports 0x15D220 enter / 0x15D21C leave)', game_access='none'),
                dict(va='0x12FD08', driving='0x183A60 (layout offset, not seen as operand)', name='DSP command block base; startup command at base+0x810 (nightfire_dsp_startup_command)', game_access='none'),
                dict(va='0x12FC48', driving='0x1839A0', name='16 x 16-byte contiguous-memory page table (thread_check AUDIO-TABLE log)', game_access='none'),
                dict(va='0x12F4BC', driving='0x183214', name='AC97 channel table (offsets 0x10/0x70; nightfire_ac97_write8)', game_access='none')],
            action_game=[
                dict(va='0x2AE598', driving='0x244C84 (game.device)', name='game IDirectSound pointer (xboxInitSound 0x0E1AE0 passes it to DirectSoundCreate; 12 readers)'),
                dict(va='0x2AE59C', name='DSEFFECTIMAGEDESC* out of DownloadEffectsImage; written only, never read by the game'),
                dict(va='0x2AE5A0', name='DSEFFECTIMAGELOC {I3DL2 reverb index 0, crosstalk index 1}'),
                dict(va='0x194840', name='effects image passed to DownloadEffectsImage, 0x6168 bytes (.data)'),
                dict(va='0x2AE5A8', name='64 voice slots x 3 buffers: +0 2D mono ADPCM, +0x100 2D stereo ADPCM, +0x200 3D mono ADPCM (all 44032 Hz)'),
                dict(va='0x2AE8C0', name='64-byte per-slot table used by SFXUpdate/dsnd*; music ring source +40 / bytes +44 (nightfire_game_audio.h)'),
                dict(va='0x19A9A8', name='5-point rolloff curve for the 3D buffers'),
                dict(va='0x2AF8C4', name='101-entry volume table (mB) built by xboxInitSound; used by dsndStreamSetVolume')],
            outside_access_to_dsound_data='none: no .text/XMV/XPP routine reads or writes a DSOUND global (only constant 0x12C000 in 0x0E7130/0x0E8A90, not an address)',
            apu_mmio_outside_dsound='XPP 0x15BDE0 reads APU XGSCNT 0xFE80200C (USB isochronous clock sync); everything else touching 0xFE800000/0xFEC00000 is inside DSOUND'),
        runtime_hook_vas=s['runtime_hook_vas'],
        dsound_internal_vtables=[t for t in s['tables'] if t['section'] == '.rdata' and t['at'] != '0x16264C'],
    )
    json.dump(out, open(HERE / 'action-dsound-map.json', 'w'), indent=1)
    print('entry points', len(eps), 'covered by Driving native', sum(1 for e in eps if e['driving_native'].startswith('lean_')))
    for e in eps: print(e['va'], e['name'], e['nargs_stdcall'], e['driving_native'], e['in_plan'])
    for p in plan: print(p)
    print('not linked:', out['not_linked_in_action'])


if __name__ == '__main__':
    main()
