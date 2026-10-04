"""Independent, read-only helpers for TEXT510 evidence analysis.

The A8 reference is adapted from analysis/font348/check-gpu.py, not from the
production renderer. It models the verified native font348 contract only. It
does not decide planner acceptance, console raster accuracy, or promotion.
No file I/O or execution occurs on import.
"""
from __future__ import annotations

import math
from dataclasses import dataclass
from typing import Sequence


def finite(values: Sequence[float]) -> bool:
    return all(isinstance(v, (int, float)) and not isinstance(v, bool)
               and math.isfinite(v) for v in values)


def rgba_summary(colors: Sequence[Sequence[float]]) -> dict:
    if not colors or any(len(c) != 4 or not finite(c) for c in colors):
        raise ValueError('RGBA must contain finite four-component colors')
    if any(v < 0 or v > 1 for c in colors for v in c):
        raise ValueError('RGBA outside the admitted font348 range')
    return {'min': [min(c[k] for c in colors) for k in range(4)],
            'max': [max(c[k] for c in colors) for k in range(4)],
            'mean': [math.fsum(c[k] for c in colors) / len(colors) for k in range(4)],
            'vertex_count': len(colors)}


def valid_bounds(bounds: Sequence[float]) -> bool:
    return (len(bounds) == 4 and finite(bounds)
            and bounds[0] <= bounds[2] and bounds[1] <= bounds[3])


def bounds_overlap(a: Sequence[float], b: Sequence[float]) -> bool:
    """Positive-area half-open rectangle overlap; never pixel occlusion proof."""
    if not valid_bounds(a) or not valid_bounds(b):
        raise ValueError('Nonfinite or inverted bounds require unknown coverage')
    return max(a[0], b[0]) < min(a[2], b[2]) and max(a[1], b[1]) < min(a[3], b[3])


def font_quad_summary(positions: Sequence[Sequence[float]],
                      colors: Sequence[Sequence[float]],
                      uv: Sequence[Sequence[float]]) -> dict:
    """Independently verify the restricted font quad and retain per-quad color."""
    if len(positions) != 4 or len(colors) != 4 or len(uv) != 4:
        raise ValueError('A font quad needs exactly four vertices')
    if any(len(p) != 4 or not finite(p) for p in positions):
        raise ValueError('Invalid position')
    if any(len(t) != 4 or not finite(t) for t in uv):
        raise ValueError('Invalid UV')
    summary = rgba_summary(colors)
    a, b, c, d = positions
    ta, tb, tc, td = uv
    if not (a[0] == d[0] and b[0] == c[0] and a[1] == b[1] and c[1] == d[1]
            and a[0] < b[0] and a[1] < d[1]):
        raise ValueError('Not an admitted axis-aligned rectangle')
    if not (ta[0] == td[0] and tb[0] == tc[0] and ta[1] == tb[1] and tc[1] == td[1]):
        raise ValueError('UV does not match the admitted rectangle')
    if any(p[2] != a[2] or p[3] != 1 for p in positions):
        raise ValueError('Nonplanar or projective font position')
    if any(t[2] != 0 or t[3] != 1 for t in uv):
        raise ValueError('Projective font UV')
    if any(list(color) != list(colors[0]) for color in colors):
        raise ValueError('Font348 requires one color within each quad')
    return {'rgba': list(colors[0]), 'rgba_summary': summary,
            'xy': [a[0], a[1], c[0], c[1]],
            'uv': [ta[0], ta[1], tc[0], tc[1]],
            'lane_bounds': [[a[0] + lane * .5, a[1] + lane * .5,
                             c[0] + lane * .5, c[1] + lane * .5] for lane in (0, 1)]}


def linear_a8(atlas: bytes, width: int, height: int, u: float, v: float,
              quantized: bool = True) -> float:
    """Bilinear clamp with the pre-existing fixture's eight-bit filter weights."""
    if width <= 0 or height <= 0 or len(atlas) != width * height or not finite((u, v)):
        raise ValueError('Invalid atlas or UV')
    x, y = u - .5, v - .5
    x0, y0 = math.floor(x), math.floor(y)
    tx, ty = x - x0, y - y0
    if quantized:
        tx, ty = round(tx * 256) / 256, round(ty * 256) / 256

    def at(px: int, py: int) -> float:
        return atlas[max(0, min(height - 1, py)) * width + max(0, min(width - 1, px))] / 255

    return (((1 - tx) * at(x0, y0) + tx * at(x0 + 1, y0)) * (1 - ty)
            + ((1 - tx) * at(x0, y0 + 1) + tx * at(x0 + 1, y0 + 1)) * ty)


@dataclass(frozen=True)
class FontContract:
    # These are reference preconditions, not a replacement production planner.
    accepted_font348: bool = False
    alpha_enable: int = 1
    alpha_func: int = 0x204
    alpha_ref: int = 16
    blend_enable: int = 1
    blend_src: int = 0x302
    blend_dst: int = 0x303
    blend_equation: int = 0x8006
    depth_func: int = 0x207
    depth_write: int = 0
    stencil_enable: int = 0
    filter_word: int = 0x02062000
    write_alpha: bool = False

    def require_supported(self) -> None:
        expected = (1, 0x204, 16, 1, 0x302, 0x303, 0x8006, 0x207, 0, 0)
        actual = (self.alpha_enable, self.alpha_func, self.alpha_ref,
                  self.blend_enable, self.blend_src, self.blend_dst,
                  self.blend_equation, self.depth_func, self.depth_write, self.stencil_enable)
        if not self.accepted_font348 or actual != expected:
            raise ValueError('Native font348 contract not independently established')
        if self.filter_word not in (0x02062000, 0x02063f01):
            raise ValueError('Unmodeled filter')


def font_pixel(quad: dict, lane: int, x: int, y: int, atlas: bytes,
               old_bgra: Sequence[int], contract: FontContract) -> dict:
    """Predict one lane pixel before later writes/resolves, never a final capture."""
    contract.require_supported()
    if lane not in (0, 1) or len(atlas) != 256 * 108:
        raise ValueError('The verified font adapter has two lanes and a 256x108 A8 atlas')
    if len(old_bgra) != 4 or any(type(v) is not int or not 0 <= v <= 255 for v in old_bgra):
        raise ValueError('Expected four destination BGRA bytes')
    xy, uv, rgba = quad['xy'], quad['uv'], quad['rgba']
    if not valid_bounds(xy) or xy[0] == xy[2] or xy[1] == xy[3] or len(uv) != 4 or not finite(uv):
        raise ValueError('Invalid quad')
    rgba_summary([rgba])
    x0, y0, x1, y1 = [round((value + lane * .5) * 256) / 256 for value in xy]
    inside = (math.ceil(x0 - .5) <= x < math.ceil(x1 - .5)
              and math.ceil(y0 - .5) <= y < math.ceil(y1 - .5)
              and 0 <= x < 640 and 0 <= y < 480)
    if not inside:
        return {'covered': False, 'discarded': False, 'expected_bgra': list(old_bgra),
                'reason': 'outside restricted raster coverage'}
    u = uv[0] + (x + .5 - x0) / (x1 - x0) * (uv[2] - uv[0])
    v = uv[1] + (y + .5 - y0) / (y1 - y0) * (uv[3] - uv[1])
    atlas_alpha = linear_a8(atlas, 256, 108, u, v)
    alpha = atlas_alpha * rgba[3]
    discarded = math.floor(alpha * 255 + .5) <= 16
    expected = list(old_bgra)
    if not discarded:
        expected[:3] = [round(rgba[2 - k] * alpha * 255 + old_bgra[k] * (1 - alpha)) for k in range(3)]
        if contract.write_alpha:
            expected[3] = round(alpha * alpha * 255 + old_bgra[3] * (1 - alpha))
    return {'covered': True, 'discarded': discarded, 'atlas_alpha': atlas_alpha,
            'effective_alpha': alpha, 'expected_bgra': expected,
            'reason': 'alpha test discard' if discarded else 'reference source-alpha blend'}


def prepared_font_quad(glyph: dict, draw: dict) -> dict:
    """Explicit bridge from native D510Glyph to the independent reference.

    Caller must separately prove native font success, full state and atlas
    provenance. Returned coordinates are already lane-shifted; use lane=0
    when passing this quad to font_pixel, for either recorded lane.
    """
    if draw.get('path') != 1 or glyph.get('lane') not in (0, 1) or glyph.get('texture_stage', glyph.get('reserved', 0)) != 0:
        raise ValueError('Not the native prepared font payload')
    if any(glyph.get(k) != draw.get(k) for k in ('lane', 'target', 'atlas_hash')):
        raise ValueError('Glyph/material identity mismatch')
    summary = font_quad_summary(glyph['position'], glyph['color'], glyph['uv'])
    u0, v0, u1, v1 = summary['uv']
    return {'quad': {'xy': summary['xy'], 'uv': [u0 * 256, v0 * 128, u1 * 256, v1 * 128],
                     'rgba': summary['rgba']},
            'reference_lane': 0, 'recorded_lane': glyph['lane'],
            'positions_already_lane_shifted': True,
            'limit': 'Transforms only; no native-success, atlas-version or final-pixel assertion'}


def compare_reference_pixel(expected: dict, actual_bgra: Sequence[int],
                             exact_input_and_version_join: bool) -> dict:
    """A renderer comparison requires complete input and target provenance."""
    if not exact_input_and_version_join:
        return {'status': 'UNRESOLVED', 'reason': 'No exact input/target-version join'}
    if len(actual_bgra) != 4 or any(type(v) is not int or not 0 <= v <= 255 for v in actual_bgra):
        raise ValueError('Invalid observed pixel')
    error = max(abs(a - b) for a, b in zip(expected['expected_bgra'], actual_bgra))
    return {'status': 'REFERENCE_MATCH' if error <= 1 else 'REFERENCE_MISMATCH',
            'max_byte_error': error,
            'limit': 'Existing host-fixture one-byte bound; not original-console accuracy or promotion verdict'}
