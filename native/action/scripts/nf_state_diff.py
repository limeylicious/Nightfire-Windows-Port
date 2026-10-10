"""Compare two NF_STATE_HASH files from NIGHTFIRE_LOCKSTEP runs (record vs replay).

Usage: python scripts/nf_state_diff.py first.bin second.bin
Prints how many frames match, the first frame where any 64 KB block of game
memory differs, and which blocks differ most often (with their guest addresses).
"""
import struct
import sys
from collections import Counter

BLOCK = 0x10000


def load(path):
    frames = {}
    with open(path, 'rb') as f:
        if f.read(8) != b'NFHASH01':
            sys.exit(f'{path}: not a state hash file')
        while True:
            head = f.read(4)
            if len(head) < 4:
                break
            frame = struct.unpack('<I', head)[0]
            blocks = {}
            while True:
                raw = f.read(2)
                if len(raw) < 2:
                    break
                bid = struct.unpack('<H', raw)[0]
                if bid == 0xFFFF:
                    break
                h = f.read(8)
                if len(h) < 8:
                    break
                blocks[bid] = struct.unpack('<Q', h)[0]
            frames[frame] = blocks
    return frames


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    a, b = load(sys.argv[1]), load(sys.argv[2])
    common = sorted(set(a) & set(b))
    if not common:
        sys.exit('no frames in common')
    first = None
    counts = Counter()
    same_frames = 0
    for fr in common:
        diff = [bid for bid in a[fr] if bid in b[fr] and a[fr][bid] != b[fr][bid]]
        if diff:
            counts.update(diff)
            if first is None:
                first = (fr, diff)
        else:
            same_frames += 1
    print(f'frames compared {len(common)} (first {common[0]}, last {common[-1]}), identical {same_frames}')
    if first is None:
        print('IDENTICAL: no block differs in any compared frame')
        return
    fr, diff = first
    print(f'FIRST DIFFERENCE at frame {fr} ({fr / 60:.2f} s of game time): {len(diff)} block(s)')
    for bid in diff[:20]:
        print(f'  block {bid:5d}  guest {bid * BLOCK:08X}-{(bid + 1) * BLOCK - 1:08X}')
    print('blocks that differ most often:')
    for bid, n in counts.most_common(20):
        print(f'  block {bid:5d}  guest {bid * BLOCK:08X}  differs in {n} sample(s)')


if __name__ == '__main__':
    main()
