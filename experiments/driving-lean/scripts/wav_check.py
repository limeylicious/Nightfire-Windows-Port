"""Audio output checks on a lean WAV dump: silent gaps, block-boundary jumps,
clipping and repeated blocks. Usage: python scripts/wav_check.py file.wav [start_s]"""
import array, sys, wave
w = wave.open(sys.argv[1]); n = w.getnframes()
a = array.array('h', w.readframes(n)); L = a[0::2]; R = a[1::2]
start = int(float(sys.argv[2]) * 48000) if len(sys.argv) > 2 else 0
B = 256
clip = sum(1 for v in a[start * 2:] if v in (32767, -32768, -32767))
gaps = 0; jumps_edge = 0; jumps_in = 0; edge_n = in_n = 0; repeats = 0
prev_nz = False; inside_gap = 0
for b in range(start // B, n // B - 1):
    blk = L[b * B:(b + 1) * B]
    nz = any(blk)
    if not nz and prev_nz: inside_gap = 1
    if nz and inside_gap: gaps += 1; inside_gap = 0
    if nz: prev_nz = True
    if blk == L[(b + 1) * B:(b + 2) * B] and nz: repeats += 1
    # boundary jump vs in-block jump
    e = abs(L[(b + 1) * B] - L[(b + 1) * B - 1]); edge_n += 1; jumps_edge += e
    for i in range(b * B + 1, b * B + B, 16):
        jumps_in += abs(L[i] - L[i - 1]); in_n += 1
big_edges = 0
for b in range(start // B + 1, n // B):
    i = b * B
    d = abs(L[i] - L[i - 1]); ref = (abs(L[i - 1] - L[i - 2]) + abs(L[i + 1] - L[i])) / 2 + 1
    if d > 2000 and d > 8 * ref: big_edges += 1
print('seconds=%.1f clipped_samples=%d silent_gaps=%d repeated_blocks=%d mean_edge_jump=%.1f mean_inblock_jump=%.1f big_edge_discontinuities=%d'
      % ((n - start) / 48000, clip, gaps, repeats, jumps_edge / max(1, edge_n), jumps_in / max(1, in_n), big_edges))
