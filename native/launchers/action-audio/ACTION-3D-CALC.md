# Action engine DSOUND: the 3D calculation (PAL default.xbe, 2026-10-08)

This is a static check only. Nothing was built or run, and nothing outside this folder was changed.

- **Input:** `nightfire-port/game_files/default.xbe`. Its SHA-256 was checked and matches the PAL hash `b464b787…2641aa1`.
- **Disassembly:** capstone 5.0.6.
- **Emulator check:** `dsound_3d_check.py` (in this folder) runs the XBE's own code under the Unicorn emulator from `python-deps`, on synthetic objects:
  - 0x1155FE, the full-HRTF calculation step;
  - 0x116E53, the routine that packs the voice volumes.

  The results are compared with the Python reference model in the same script, over 12,000 random cases (seeds 7, 99 and 2026). Every output matches except:
  - one elevation sitting exactly on a quantisation boundary (−27.0°, see §4);
  - tiny float leftovers at gains of exactly 0 or 1. Both the model and the original give near-silence there (−7,900 mB or less).

  To re-run: `python -I dsound_3d_check.py <project-root> [cases] [seed]`.

How each statement is labelled:

- **[V]** read from the code and confirmed by the emulator check.
- **[C]** read from the code only.
- **[I]** inferred. This covers hardware behaviour, which the library code cannot prove.

A few terms used throughout:

- **mB:** millibels (1/100 dB).
- **"trunc"** is `cvttss2si`, called at 0x11A608. It rounds toward zero, after the value has been stored as a 32-bit float.
- **mB_amp(g)** is 0x114C4F: `g≤0 → −10000; g≥1 → 0; else trunc(2000·log10 g)`.
- **mB_pow(g)** is 0x114C08: the same, but with 1000·log10.

## 0. Call path and stored state

**How the calculation is reached. [C]**

1. `IDirectSound_CommitDeferredSettings` (0x113C13) calls `CDirectSound_CommitDeferredSettings` (0x113541).
2. That runs the APU 3D commit 0x115F50. This commit does two things:
   - it runs the listener step 0x114CE5;
   - it runs the I3DL2 listener step.
3. Then, for each voice, it runs 0x112AEF. This merges the listener's dirty bits (settings+0xA4) into the buffer's dirty bits (params+0x78) and calls `CMcpxVoiceClient_Commit3dSettings` (0x117F4C).
4. Inside 0x117F4C, when the mode is not DISABLE:
   - 0x114E02 clears the output flags (src+0x1C and src+0x24), then calls the function pointer at 0x12F4D4;
   - if dirty bit 0x100 is set, it calls 0x11AB29 (I3DL2 source).
5. Finally 0x117D4C applies the results:

   | Condition | Routine called |
   |---|---|
   | output flags & 0xF, Pan3D, or I3DL2 & 3 | SetVolume 0x117C06 (volumes from 0x116E53) |
   | 0x10 | SetPitch 0x117C9E (value from 0x11706F) |
   | 0x20 | HRTF load 0x117556 |
   | I3DL2 & 0xC | SetFilter 0x11765B |

6. The handled dirty bits are cleared. The listener's dirty bits are cleared at the end of 0x113541.

**Immediate setters and timing. [C]**

- A buffer setter called without `DS3D_DEFERRED` (argument bit 0 = 0) runs 0x112AEF for that voice only.
- A listener setter called without it runs 0x113541.
- If the voice has no hardware voice yet (the `[+0x12]&3 != 3` check at 0x117F60), its dirty bits are kept. They are applied at Play: 0x118023 calls 0x117F4C before VOICE_ON.
- When a voice is set up, 0x117B26 also reloads the filter pair.

**How the algorithm is chosen. [C]**

- The global 0x12F4DC starts at 3, which means "none chosen".
- `DirectSoundUseFullHRTF` (0x11275F → 0x1126A7) sets that global to 0. It also sets:
  - calc = 0x1155FE (stored at 0x12F4D4);
  - GetHrtfFilterPair = 0x115359 (stored at 0x12F4D8).
- Light HRTF and Pan3D are not linked. Their tables at 0x11CB90 and 0x11CC48, and the Pan3D speaker table at 0x11C8F0, are not referenced by the full-HRTF path.

**Steps inside 0x1155FE, run in this order, by input dirty bit. [V]**

| Bit | Routine | Output (src offset) | Changed-flag |
|---|---|---|---|
| 0x01 | 0x114E18 relative position | +00 unit direction from listener to source, +0C distance | – |
| 0x02 | 0x115490 azimuth/elevation, then 0x115359 | +10 azimuth, +14 elevation, +44/+48 left/right HRIR pointer | 0x20 |
| 0x04 | 0x114E6B distance | +28 mB | 0x01 |
| 0x08 | 0x114F98 cone angle | +18 (2θ, degrees) | – |
| 0x10 | 0x115033 cone attenuation | +2C mB | 0x02 |
| 0x20 | 0x1150CD front/back split | +30 front mB, +34 back mB | 0x04 |
| 0x40 | 0x1151B9 centre | +38 front-pair reduction mB, +3C centre mB | 0x08 |
| 0x80 | 0x115297 Doppler | +40 pitch (1/4096 octave) | 0x10 |
| 0x100 | (I3DL2, in 0x117F4C) | I3DL2 object +4 direct, +8 room | I3DL2 flags |

**Which setter marks which dirty bits. [C]**

| Setter | VA | Bits |
|---|---|---|
| Buffer SetPosition | 0x113795 | 0x1FF |
| Buffer SetVelocity | 0x1137E9 | 0x80 |
| SetMin/MaxDistance, SetRolloffCurve | 0x113762, 0x11372F, 0x11383B | 0x04 |
| SetI3DL2Source | 0x11387E | 0x100 |
| Listener SetPosition | 0x113F11 | 0x1FF |
| Listener SetVelocity | 0x113F89 | 0x80 |
| Listener SetOrientation | 0x113E7F | 0x466. Bit 0x400 recomputes the listener's right vector in 0x114CE5. |
| New 3D buffer | 0x1140E2 | 0x1FF |
| Mode change to or from DISABLE | 0x117F9E | forces 0x1FF, and all outputs |

SetRolloffCurve keeps the game's own pointer (params+0x70) and the count (params+0x74). It does not copy the curve.

**The structures. [C]**

**3D parameters, per buffer.** These are allocated by 0x114037 and are 0x7C bytes long.

- The first 0x4C bytes are the Xbox DS3DBUFFER, copied from the defaults at 0x12F240:

  | Offset | Field | Default |
  |---|---|---|
  | +00 | dwSize | 0x4C |
  | +04 | position | – |
  | +10 | velocity | – |
  | +1C | inside cone angle | 360 |
  | +20 | outside cone angle | 360 |
  | +24 | cone orientation | (0,0,1) |
  | +30 | cone outside volume | 0 |
  | +34 | min distance | 1.0 |
  | +38 | max distance | 1e9 |
  | +3C | mode | 0 (NORMAL) |
  | +40 | distance factor | 1.0 |
  | +44 | rolloff factor | 1.0 |
  | +48 | Doppler factor | 1.0 |

- +4C holds a DSI3DL2BUFFER, copied from 0x12F218. All of it is 0, except Occlusion.LFRatio, which is 0.25.
- +70 is the curve pointer, +74 the curve count and +78 the dirty bits.

**Listener parameters.** These live at CDirectSoundSettings+0x34. They are copied from 0x12F1D8 by the constructor 0x113D3D.

| Offset | Field | Default |
|---|---|---|
| +04 | position | – |
| +10 | velocity | – |
| +1C | front | (0,0,1) |
| +28 | top | (0,1,0) |
| +34 | distance factor | 1.0 |
| +38 | rolloff factor | 1.0 |
| +3C | Doppler factor | 1.0 |

The I3DL2 listener is at settings+0x74. Its defaults (0x12F1A8) are the Generic preset: Room −1000, RoomHF −100, RoomRolloff 0, and so on. The game never calls SetI3DL2Listener.

**Per-voice calculator object ("src").** This is 0xDC bytes, built by 0x114DC2 and initially all zero. Besides the outputs listed in the step table above, it holds:

| Offset | Contents |
|---|---|
| +20 | Pan3D mask |
| +4C | per-bin Pan3D volumes (0 for full HRTF) |
| +CC | centre-in-use flag |
| +D0 | the listener object (APU+0x58) |
| +D4 | pointer to the buffer flags |
| +D8 | pointer to the 3D parameters |

**Listener object (APU+0x58).** Built by 0x114C96.

| Offset | Contents |
|---|---|
| +04 | pointer to the listener parameters |
| +08 | right vector. It starts as (1,0,0), from 0x11C8D8. |
| +78 | surround flag (byte), see §9 |

## 1. Coordinates [V]

1. **Relative vector** (0x114E18): `d = srcPos − lstPos`, then normalised (by 0x1168CC). Its length goes to `dist`.
   - In HEADRELATIVE mode (mode 1), `d = srcPos` and the listener position is ignored.
   - A zero vector stays zero, and dist = 0.
2. **Right vector** (0x114CE5): `right = top × front`, which is `(t.y·f.z − t.z·f.y, t.z·f.x − t.x·f.z, t.x·f.y − t.y·f.x)`. The library does not normalise or orthogonalise front or top.
3. **Axes:** F = front·d, U = top·d, R = right·d. In head-relative mode the fixed defaults front (0,0,1), top (0,1,0) and right (1,0,0) are used (0x12F1D8 and 0x11C8D8).
   - The coordinate system is left-handed: +x is right, +y is up and +z is front. The unused Pan3D table at 0x11C8F0 agrees: bin 6 sits at (−0.7, 0, 0.7), bin 7 at (0.7, 0, 0.7), and bins 8 and 9 at z = −0.7.
4. **Angles** (0x115490). These use a piecewise approximation of atan in degrees: `ζ(t) = 45·t` for t ≤ 1, and `90 − 45/t` for t > 1. Its error is at most about 4°.
   - `el = sgn(U)·ζ(|U| / √(R²+F²))`
   - `az = ζ(|R|/|F|)` when |F| > |R|. Otherwise `az = 90 − 45·|F|/|R|`, or 0 when R = F = 0.
   - If F < 0, then `az = 180 − az`. If R < 0, then `az = −az`.
   - So az = 0 is in front, +90 is right and ±180 is behind. el = +90 is above.
   - If dist = 0, then az = el = 0.

## 2. Distance attenuation, `D` = src+0x28 (0x114E6B) [V]

Let m = min distance, M = max distance and x = dist.

1. If x ≤ m: D = 0.
2. If the buffer flags include 0x20000 (DSBCAPS_MUTE3DATMAXDISTANCE) and x ≥ M: D = −10000. The game's flags are 0x10, so this does not apply.
3. Otherwise, set x = min(x, M) and continue.

**With a curve** (pointer at params+0x70, count n at +0x74):

```
step = (M−m)/n
t    = x−m
i    = min(trunc(t·(1/step)), n−1)
L    = (i==0) ? 1.0 : c[i−1]
R    = c[i]
D    = mB_amp( L + (R−L)·((t − i·step)·(1/step)) )
```

Point c[k] is reached at m + (k+1)·step, and there is an implied 1.0 at m. This confirms the rule in `nightfire_audio_spatial.h`.

With the game's curve {1, .5, .25, .125, 0}: at x = M the gain is a float leftover near 0. The emulator gave anything from −10000 to −40338 mB. There is no lower clamp, so treat D ≤ −10000 as silent.

**Without a curve:**

```
D = trunc(−2000·log10(1 + (x/m − 1)·Rl·Rs))
```

where Rl is the listener rolloff (+0x38) and Rs is the buffer rolloff (+0x44). With the defaults this is 20·log10(m/x). It is **not clamped at −10000**. With M = 1e9, values such as −17231 were seen.

The final volume step turns any very large attenuation into 4095 (mute).

## 3. Mixbin volumes for a 3D voice, and the pan law

### Front/back split, `FBf` = src+0x30 and `FBb` = src+0x34 (0x1150CD) [V]

If the surround flag is 0 (listener object +0x78, §9): **FBf = 0 and FBb = −10000.** All of the dry 3D signal goes to the front bins.

If the surround flag is 1:

```
B  = clamp01( (|az|/90 − 1)·(1 − |el|/90) + 0.5 )     // 0 = all front, 1 = all back
A  = Ld·Sd·dist                                        // the two distance factors, i.e. metres
if (A < 0.5)  B = 0.5·((B − 0.5)·A + 1)                // near field: pulled toward 0.5
FBf = mB_pow(1 − B)          FBb = mB_pow(B)           // a power law: 10·log10
```

The near-field rule is discontinuous at A = 0.5. The emulator confirms it: at A = 0.49, a source dead in front gives front −205 / back −423; at A = 0.5 it gives 0 / −10000.

### Centre, `CF` = src+0x38 and `CT` = src+0x3C (0x1151B9) [V]

This only runs if src+0xCC is set. 0x116D78 (0x116DB6) sets that flag when the voice's mixbin list has **bin 2 at index 4**, which is true for the game's list {6,8,7,9,2,10}.

```
a = trunc(|az|), b = trunc(|el|)
if (a < 45 and b < 45):
    CT = mB_amp(T1[a]·T1[b])
    CF = mB_amp(1 − T2[a]·T2[b])          // both clamped to [−10000, 0]
else:
    CT = −10000, CF = 0
```

The flag is set when a voice is set up (SetMixBins / 0x117B45 → 0x116D78). If it is 0, then CT = −10000 and CF = 0.

The two tables are 45 floats each. Their closed forms match to within 5.2e-7:

- T1 at 0x11CA20: `T1[k] = 2^−0.25·cos(2k°)`
- T2 at 0x11CAD8: `T2[k] = 2^−0.5·cos²(2k°)`. Note T2 = T1².

```
T1: 0.840896 0.840384 0.838848 0.836290 0.832713 0.828121 0.822521 0.815918 0.808322 0.799740
    0.790184 0.779666 0.768197 0.755793 0.742467 0.728238 0.713121 0.697135 0.680300 0.662635
    0.644164 0.624908 0.604890 0.584136 0.562670 0.540518 0.517708 0.494267 0.470223 0.445607
    0.420448 0.394777 0.368625 0.342023 0.315005 0.287604 0.259851 0.231782 0.203431 0.174832
    0.146020 0.117030 0.087898 0.058658 0.029347
T2: 0.707107 0.706246 0.703666 0.699381 0.693411 0.685785 0.676540 0.665723 0.653384 0.639584
    0.624391 0.607878 0.590127 0.571223 0.551258 0.530330 0.508541 0.485997 0.462807 0.439086
    0.414947 0.390510 0.365892 0.341215 0.316597 0.292159 0.268021 0.244299 0.221110 0.198566
    0.176777 0.155849 0.135884 0.116980 0.099228 0.082716 0.067523 0.053723 0.041384 0.030566
    0.021322 0.013696 0.007726 0.003441 0.000861
```

So a source dead in front gives CT = −301 (centre at 0.707) and CF = −602 (front pair at 0.5).

### Per-bin 3D terms (0x116E53, at 0x116E6B–0x116F96) [V]

These apply only when the buffer flags include 0x10 and the mode is not DISABLE. All terms are in mB and ≤ 0.

- **base** = D + C. C is the cone attenuation, §6.
- **Id** = I3DL2 direct, and **Ir** = I3DL2 room (§10). For the game, Id = 0 and Ir = the game's lRoom.

| Bin | 3D term (each clamped to ≤ 0) |
|---|---|
| 6, 7 (XTLK front L/R) | `min(0, base + Id + FBf + CF)` |
| 8, 9 (XTLK back L/R) | `min(0, base + Id + FBb)` |
| 2, only if src+0xCC is set | `min(0, base + Id + FBf + CT)` |
| 10 (I3DL2 send) | `min(0, base + Ir)` |
| any other bin, including bin 2 at another index | `base`. **No Id.** |
| plus, if src+0x20 has bit (1<<bin) | −src[+0x4C + 4·bin]. This is Pan3D only, so 0 here. |

**Left and right volumes are identical.** Bins 6 and 7 get the same value, and so do 8 and 9. All of the left/right image comes from the HRTF filter pair (§4). There is no amplitude pan law for left/right.

**Bin 2 (centre)** is neither left alone nor replaced. It gets the game's bin volume (0) plus distance, cone and direct, plus the front split and the centre term CT. That makes it silent once |az| ≥ 45° or |el| ≥ 45°.

## 4. HRTF filter pair (0x115490 → 0x115359; loaded by 0x117556)

### Choosing the filter [V]

1. **Elevation.** `ie = trunc(el ± 3)`, using the sign of el. Then `e6 = 6·(ie/6)`, with integer division rounding toward zero. This rounds to the nearest 6°, with halves going away from zero.
   - The one emulator mismatch was el = −26.99999 against −27.0 at exactly this boundary.
2. **Azimuth step.** Let A = |az|.
   - If |e6| = 90: q = 0.
   - Else if |e6| > 60: q = 12·(trunc(A+6)/12).
   - Else if |e6| > 30: q = 6·(trunc(A+3)/6).
   - Else: q = 3·(trunc(A+1.5)/3).
3. **Surround mirror.** If the surround flag is set and q > 90, then q = 180 − q. Rear sources reuse the front filter, and the front/back split puts them in the back bins.
4. **Indices.** `ai = q/3`, from 0 to 60. `ei = (e6+90)/6`, from 0 to 30. Either index is set to 0 if out of range.
5. **Filter number.** `id = u16[0x12E2C0 + 2·(ai·31 + ei)]`. This table is 61 × 31 = 1,891 words, ending at 0x12F186. The ids are even, from 0 to 2220, and there are 1,111 different ones.
6. **Pair.** `first = 0x11CD00 + 32·id`, `second = first + 32`.
   - If az ≥ 0: left = first and right = second. If az < 0, the two are swapped.
   - These are stored at src+0x44 (left) and src+0x48 (right).
   - The HRIR table is 0x11CD00–0x12E2C0: 2,222 entries of 32 bytes, 71,104 bytes in all.

At ei = 0 and ei = 30 the filter does not depend on azimuth; the ids are 0 and 2220.

### Loading the filter (0x117556) [C]

1. **Default pair.** The pair at 0x12F3B8 is used if the mode is DISABLE or a pointer is null. Both of its pointers are 0x12F398, which holds the same bytes as id 1170, the front filter (az 0, el 0), and has ITD 0.
2. **Entry handle.** `SET_CURRENT_HRTF_ENTRY` (method 0x160) = 2·voice + toggle. The entries are double-buffered: the toggle at voice+0x66 alternates.
3. **Taps.** For k = 0..14, `SET_HRIR[k]` (method 0x400 + 4k) = `L[2k] | R[2k]<<8 | L[2k+1]<<16 | R[2k+1]<<24`.
4. **Last tap and delay.** `SET_HRIR_X` (method 0x43C) = `L[30] | R[30]<<8 | ITD<<25`.
   - `ITD = +first[31]` when az ≥ 0, and `−first[31]` when az < 0.
   - The ITD field (bits 16–31) therefore holds ITD·512, which is s6.9.
5. **Attach.** `SET_VOICE_TAR_HRTF` (method 0x31C) = the handle. Non-HRTF voices get 0xFFFF (0x117B03).

### The entry format [I]

Each entry has bytes 0–30 as 31 FIR taps per ear, and byte 31 as the delay.

1. **The delay** is set only in the even (first) entries, with values 0–32: 0 at front and back, about 30 at az 90. That fits **samples at 48 kHz** (30 samples is 0.625 ms).
   - The toolkit's xemu-derived code decodes this field as s6.9 samples and delays channel 0 (left) when the ITD is positive. A positive ITD means the source is on the right, so the far (left) ear is delayed. These agree.
2. **The taps are almost certainly sign-magnitude, not two's complement.** Across all 68,882 tap bytes:
   - 0x7B–0x7F, 0xFB–0xFF and 0x80 never occur;
   - 0x81–0x85 occur 3,963 times;
   - read as sign-magnitude, the total variation of the responses halves.

   Suggested decode: `v = (b & 0x80) ? −(b & 0x7F) : b`, scaled by 1/128.
   - The toolkit APU (`xboxrecomp/src/apu/apu_vp.c` set_hrir_coeff_tar → `int8_to_float`) decodes two's complement. That is probably wrong.
3. **Ear.** For az > 0, the second (right) entry has about twice the energy of the first. This fits first = left ear and second = right ear.

### Where the HRTF output goes [I]

Start-up programs these HRTF submixes: 6, 8, 7, 9 (§8). The default 3D mixbin list (0x12F28C → 0x12F2BC) is {6, 8, 7, 9, 10}, and the game's list {6, 8, 7, 9, 2, 10} keeps the same first four.

So voice slots 0–3 are bins XTLK-FL, BL, FR and BR, with volumes (front, back, front, back). The only consistent reading is:

- the left-ear output goes to slots 0 and 1 (bins 6 and 8);
- the right-ear output goes to slots 2 and 3 (bins 7 and 9).

In other words, **the HRTF output reaches only the first 4 bins, through the HRTF submixes.**

Slots 4 and 5 (bins 2 and 10) presumably carry the unfiltered mono. This is hardware behaviour and cannot be proved from the library.

The xemu model in the toolkit uses `samples[b % channels]` for slots 0–3. That would put the right ear into bin 8, which contradicts this layout.

## 5. Doppler and pitch [V]

At 0x115297, let v be the velocity along the unit direction d from listener to source (positive means moving apart):

```
v  = (Sv − Lv)·d           // head-relative mode: v = Sv·d
X  = Ld·Sd·v·Sdop·Ldop     // listener +0x34 / +0x3C, buffer +0x40 / +0x48
P3 = 0       if X == 0
P3 = −32767  if X ≥ 342
P3 = +4096   if X ≤ −342
P3 = round(4096·log2(1 − X/342))   otherwise   // fistp, round-to-nearest
```

The speed of sound is 342 units per second.

**How this combines with the frequency** (0x11706F; written by 0x117C9E):

1. `Pf` is the value at settings+0x18 from `XAudioCalculatePitch` (0x112787): `round(4096·log2(f/48000))`, or 0 when f = 48000.
   - SetFrequency(0) uses the buffer's own format rate.
   - Examples: 44,032 Hz → −510 and 44,100 Hz → −501.
2. `P = clamp(Pf + P3, −32767, 8191)`. P3 is added only for a 3D voice whose mode is not DISABLE.
   - Terms for a voice routed to a submix are also added; the game does not use these.
3. `SET_VOICE_TAR_PITCH` (method 0x37C) = `P<<16`.

The playback rate is 48000·2^(P/4096) [I]. P3 itself has no lower clamp: a speed of 341 gave −34480. The final clamp handles that.

## 6. Cone [V]

**Cone angle** (0x114F98). Let o be the cone orientation, a = |o − d| and b = |o + d|.

- If b < a: angle = 180·(b/a).
- Otherwise: angle = 4·(90 − 45·a/b).

This is about 2θ, where θ is the angle between o and −d. The listener is inside the cone when 2θ is within the cone angle, as in DirectSound.

**Cone attenuation** (0x115033). Let in and out be the inside and outside angles (DWORDs), and Vo the outside volume.

1. If angle ≤ in: C = 0.
2. Else if in < out and angle ≥ out: C = Vo.
3. Otherwise: `C = trunc((angle − in)·Vo / max(out − in, 1))`. The subtraction is unsigned.

With the defaults (360, 360, (0,0,1), 0) the result is always C = 0. The game links no cone setters (§2 of the survey), so **C = 0 always.**

## 7. The full 12-bit volume formula [V]

The volume for each slot is built in 0x116E53 (0x116F22–0x116FAF):

- `vol[bin]` comes from the DSMIXBINVOLUMEPAIR (stored by 0x11295B / 0x112936).
- `T3D(bin)` is the 3D term from §3. It is 0 for a 2D voice.
- settings+0x1C holds `lVolume − headroom` (0x112A76 / 0x112A92).

```
att  = headroom − lVolume − vol[bin] − T3D(bin)
val  = (att ≥ 0) ? min(⌊att·64/100⌋, 0xFFF) : 0xFFF     // the division is unsigned: a negative att wraps to mute
```

Slots at or past the voice's mixbin count are set to 0xFFF (0x117062).

The values are packed as follows (verified):

```
VOLA = v1<<20 | (v7&F)<<16 | v0<<4 | (v6&F)
VOLB = v3<<20 | (v7>>4&F)<<16 | v2<<4 | (v6>>4&F)
VOLC = v5<<20 | (v7>>8&F)<<16 | v4<<4 | (v6>>8&F)
```

These are written to methods 0x360, 0x364 and 0x368.

**So yes: 3D adds its attenuation inside the same formula as 2D.**

For the game's 3D voice (headroom 0, all bin volumes 0, list {6,8,7,9,2,10}), each slot is `val = f(−lVolume − term)`, where f is the 0x116E53 formula above:

| Slot | Bin | term |
|---|---|---|
| 0 | 6 | min(0, D + FBf + CF) |
| 1 | 8 | min(0, D + FBb) |
| 2 | 7 | min(0, D + FBf + CF) |
| 3 | 9 | min(0, D + FBb) |
| 4 | 2 | min(0, D + FBf + CT) |
| 5 | 10 | min(0, D + lRoom) |
| 6, 7 | – | 0xFFF |

## 8. What start-up programs: headroom and HRTF submixes [C]

**Submix headroom.**

1. The CDirectSoundSettings constructor (0x113D3D, at 0x113D8A–0x113D9A) sets the headroom bytes: settings+0x14..+0x32 = **1** (bins 0–30) and settings+0x33 = **0** (bin 31).
2. The APU set-up routine (0x116704) loops over i = 0..31 (0x1167DD–0x1167EF), calling `CMcpxAPU_SetMixBinHeadroom` (0x1161DC). Each call writes `headroom[i] & 7` to `SET_SUBMIX_HEADROOM` (method 0x200 + 4i, at 0x11620B).
3. Result: **bins 0–30 = 1 and bin 31 = 0, the same as Driving.**
4. SetMixBinHeadroom is not linked, so the game cannot change this.

**HRTF headroom.** The same set-up routine calls 0x1161A5 with 0 (at 0x1167D0). That writes `SET_HRTF_HEADROOM` (method 0x280, at 0x1161CC) = **0**.

**HRTF submixes.** 0x11B07D runs during device start-up (called from 0x11BBF6 at 0x11BC15). It writes `SET_HRTF_SUBMIXES` (method 0x2C0, at 0x11B146) = **0x09070806**. The value is taken from the {bin, volume} list at 0x12F2E4, through the pointer at 0x12F298 (count 4). So **hrtf_submix[0..3] = 6, 8, 7, 9**.

**Other writes.** The same routine writes 0xFFF to methods 0x2A0, 0x2A4, 0x2A8, 0x2AC and 0x2B0. These methods have no names in the toolkit header.

**What the headroom does [I].** In the xemu model, a mix into a bin is divided by 2^headroom. Slots 0–3 of a 3D voice use the HRTF headroom instead of the bin headroom. If that is right:

| Path | Hardware headroom | Software headroom |
|---|---|---|
| 3D HRTF path | ×1 | 0 |
| 3D bins 2 and 10 | ×0.5 | 0 |
| 2D voices | ×0.5 | 600 mB |

Whether the DSP later makes up this gain is not visible in DSOUND.

## 9. The surround flag (listener object +0x78)

**How the library sets it. [C]**

- At 0x1167F5–0x116822, the flag is 1 when the speaker configuration's low word is 2 (SURROUND) **or** bit 0x10000 (ENABLE_AC3) is set. A negative configuration gives 0.
- The speaker configuration is `XGetAudioFlags`, the routine labelled GetAudioMode at 0x0EAD9D. It reads EEPROM setting 9 (XC_AUDIO), then adjusts the result by AV pack:
  - AV pack 3 gives 1;
  - AV pack 6 clears the high word.
- The 0x12F508 override is not used.

**What the flag changes.**

- Front/back split: on gives the split from §3. Off sends everything to the front bins and mutes the back bins.
- Rear sources: on reuses the front-hemisphere filter (the mirror in §4). Off uses the true rear filter.

**What the current native runtime gives. [C]**

- `runtime/kernel_xbox.c` returns XC_AUDIO = 0x00010001.
- HalBootSMCVideoMode is 0.
- So GetAudioMode returns 0x10001, and **the flag is 1 (surround behaviour) in today's emulated-APU runs**, because of the AC3 bit.
- Also, the comment in that file calls 1 "stereo". In the XDK, 1 is MONO and 0 is STEREO. This is a lead only; the file was not changed.

A stock stereo console (0x00000000) gives flag 0. A native replacement must choose one of the two.

## 10. I3DL2 source (0x11AB29; object built by 0x11A970) [C]

Let x = dist · listener RoomRolloff · source RoomRolloff.

| Output | Formula |
|---|---|
| direct (+4) | `lDirect + trunc(ObsHF·ObsLF + OccHF·OccLF)` |
| room (+8) | `lRoom + trunc(OccHF·OccLF + (x > 1 ? 2000·log10 x : 0))` |
| directHF level | `lDirectHF + trunc((1−ObsLF)·ObsHF + (1−OccLF)·OccHF)`, clamped to [−10000, 0] |
| roomHF level | `lRoom` + trunc((1−OccLF)·OccHF), clamped. **This uses lRoom, not lRoomHF** (0x11AC24); it may be an XDK slip. |

The two HF levels become filter coefficients through 0x11A993. A level of 0 gives 0.

The voice filter (0x11765B) is set as follows for a 3D voice:

- filter mode 3 is forced if it was 0;
- FCB = `direct coefficient | (src+0xCC ? direct coefficient : room coefficient)<<16`.

**What the game sends.** 0x0E1C6E zeroes the whole DSI3DL2BUFFER. 0x0E15E0 then sets only **lRoom = the game's volume table entry** (the table at 0x2AF8C4) and passes it deferred. As a result, for the game:

- direct = 0;
- room = lRoom;
- the direct coefficient is 0, and because src+0xCC is set, FCB = 0. **No I3DL2 filtering is expected** [I: the meaning of coefficient 0 in hardware].

The listener RoomRolloff is 0, so the distance term is not used.

## 11. Native recipe (summary)

For each 3D buffer, at commit or at Play:

1. Compute d, dist, az and el (§1).
2. Compute D (§2), FBf and FBb (§3, using the surround flag you choose), CF and CT (§3), P3 (§5), and the HRIR pair with its ITD (§4).
3. Build the slot attenuations with §7.

To mix a mono source:

- Filter it with the left and right 31-tap HRIRs (sign-magnitude/128), delaying the far ear by |ITD| samples at 48 kHz.
- Send the left output to bin 6 (front gain) and bin 8 (back gain), and the right output to bins 7 and 9.
- Send the dry mono to bin 2 (CT) and bin 10 (room send).
- Resample by 2^(P/4096) × 48 kHz.

Bins 6–9 are XTLK inputs to the crosstalk DSP effect, which was not analysed here. For stereo output with the flag at 0, the back bins are muted, so bin 6 → left and bin 7 → right is a reasonable first mapping, with a separate decision for how to fold in the centre bin 2.
