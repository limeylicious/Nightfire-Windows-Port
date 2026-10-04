"""Observed TEXT510 draw/resolve/present chains, without a pixel-causality verdict.

This adapter uses explicit resolve and link payload epochs, never the later
CACHE/PRESENT header epoch. Missing generic input coverage remains explicit.
Prepared material positions already contain lane offsets; font UVs are host
normalized coordinates. Neither transformation is applied twice.
"""
from __future__ import annotations

from collections import defaultdict
import math

from text510_reference import bounds_overlap, finite, rgba_summary, valid_bounds


def physical_identity(value):
    if type(value) is not int or not 0 <= value <= 0xffffffff:
        return None
    if value < 0x08000000:
        return value
    if 0x80000000 <= value < 0x88000000 or 0xf0000000 <= value < 0xf8000000:
        return value & 0x07ffffff
    return None


class DrawAnalysis:
    def __init__(self):
        self.attempts = {}
        self.routes = defaultdict(list)
        self.draws = []
        self.last_draw = {}
        self.last_kind = {}
        self.resolves = defaultdict(list)
        self.caches = []
        self.presents = []
        self.clears = []
        self.immediate = defaultdict(list)
        self.attributes = defaultdict(list)
        self.issues = []

    def add(self, record):
        kind, p = record['type'], record['payload']
        tid, attempt = record['tid'], record['attempt']
        if kind == 'ATTEMPT':
            if not attempt or attempt in self.attempts:
                self.issues.append({'serial': record['serial'], 'reason': 'duplicate/zero ATTEMPT identity'})
            else:
                self.attempts[attempt] = record
        elif kind == 'ROUTE':
            self.routes[attempt].append(record)
        elif kind == 'DRAW':
            item = {'record': record, 'pixel': None, 'glyphs': [], 'issues': []}
            self.draws.append(item)
            self.last_draw[tid] = item
        elif kind in ('PIXEL', 'GLYPH'):
            item = self.last_draw.get(tid)
            previous_ok = self.last_kind.get(tid) == 'DRAW' if kind == 'PIXEL' else self.last_kind.get(tid) in ('PIXEL', 'GLYPH')
            if not item or item['record']['attempt'] != attempt or not previous_ok:
                self.issues.append({'serial': record['serial'], 'reason': 'unattached material detail'})
            elif kind == 'PIXEL':
                if item['pixel'] is not None:
                    item['issues'].append('duplicate pixel state')
                item['pixel'] = p
            else:
                draw = item['record']['payload']
                if any(p[key] != draw[key] for key in ('lane', 'target', 'atlas_hash')):
                    item['issues'].append('glyph and draw lane/target/atlas identity differ')
                item['glyphs'].append(p)
        elif kind == 'RESOLVE':
            self.resolves[record['epoch']].append(record)
        elif kind == 'CACHE':
            self.caches.append(record)
        elif kind == 'PRESENT':
            self.presents.append(record)
        elif kind == 'CLEAR':
            self.clears.append(record)
        elif kind == 'IMMEDIATE':
            self.immediate[attempt].append(record)
        elif kind == 'ATTRIBUTES':
            self.attributes[attempt].append(record)
        self.last_kind[tid] = kind

    def publication_chains(self):
        result, indices = [], set()
        for present in self.presents:
            link, issues = present['payload'], []
            candidates = self.resolves.get(link['epoch'], [])
            candidates = [r for r in candidates if r['serial'] < present['serial']]
            if link['index'] <= 0 or link['index'] in indices:
                issues.append('duplicate/zero present index')
            indices.add(link['index'])
            resolve = candidates[0] if len(candidates) == 1 else None
            if not link['epoch'] or resolve is None:
                issues.append('no unique preceding resolve for selected payload epoch')
            if resolve:
                destination = physical_identity(resolve['payload']['destination'])
                if destination is None or destination != physical_identity(link['physical']):
                    issues.append('resolve destination and presented physical differ/unmapped')
                if resolve['payload']['family'] != 1 or resolve['payload']['count'] != 3:
                    issues.append('unmodeled scene resolve family/vertex count')
            cache = None
            if link['version']:
                caches = [c for c in self.caches if c['serial'] < present['serial']
                          and all(c['payload'][k] == link[k] for k in ('epoch', 'physical', 'version'))]
                if len(caches) != 1:
                    issues.append('no unique preceding cache physical/version/epoch')
                else:
                    cache = caches[0]
                    if resolve and cache['serial'] <= resolve['serial']:
                        issues.append('cache precedes resolve')
            elif resolve and present['tid'] != resolve['tid']:
                issues.append('uncached direct-present crosses producer threads')
            result.append({'present': link['index'], 'physical': link['physical'],
                           'version': link['version'], 'selected_epoch': link['epoch'],
                           'present_header_epoch': present['epoch'],
                           'present_serial': present['serial'],
                           'resolve_serial': resolve['serial'] if resolve else None,
                           'source': resolve['payload']['source'] if resolve else None,
                           'resolve': resolve['payload'] if resolve else None,
                           'cache_serial': cache['serial'] if cache else None,
                           'status': 'OBSERVED_LINK_CHAIN' if not issues else 'UNRESOLVED',
                           'issues': issues,
                           'limit': 'Explicit diagnostic chain only; complete target-writer and pixel coverage are separate.'})
        return result

    def finish(self):
        chains = self.publication_chains()
        groups = defaultdict(list)
        chains_by_source = defaultdict(list)
        generic_by_epoch = defaultdict(list)
        clears_by_target = defaultdict(list)
        drawn_attempts = set()
        for c in chains:
            if c['status'] == 'OBSERVED_LINK_CHAIN' and physical_identity(c['source']) is not None:
                chains_by_source[(c['selected_epoch'], physical_identity(c['source']))].append(c)
        for values in self.routes.values():
            for r in values:
                if r['payload']['path'] == 5 and r['payload']['stage'] in (6, 7):
                    generic_by_epoch[r['epoch']].append(r['serial'])
        for c in self.clears:
            clears_by_target[(c['epoch'], physical_identity(c['payload']['target']))].append(c['serial'])
        for item in self.draws:
            r, p = item['record'], item['record']['payload']
            groups[(r['epoch'], physical_identity(p['target']), p['lane'])].append(item)
            drawn_attempts.add(r['attempt'])
        output = []
        for item in self.draws:
            r, p = item['record'], item['record']['payload']
            issue = list(item['issues'])
            known_target = physical_identity(p['target']) is not None
            if not known_target:
                issue.append('unmapped target identity; no same-target overlap assertion')
            if not valid_bounds(p['bounds']):
                issue.append('nonfinite/inverted prepared bounds')
            if not item['pixel'] and p['flags'] != 1:
                issue.append('missing pixel state')
            if item['pixel'] and p['flags'] == 1:
                issue.append('basic adapter unexpectedly has material PIXEL record')
            for key in ('minimum', 'maximum', 'mean'):
                if not finite(p[key]):
                    issue.append('nonfinite diffuse ' + key)
            quads = []
            for glyph in item['glyphs']:
                entry = {'index': glyph['index'], 'positions_prepared': glyph['position'],
                         'texture_stage': glyph['texture_stage'], 'uv_normalized': glyph['uv'],
                         'rgba_per_corner': glyph['color'],
                         'vertex_order': 'triangle-strip source order' if p['path'] == 2 else 'font perimeter order'}
                if glyph['texture_stage'] != (3 if p['path'] == 2 else 0):
                    issue.append('glyph texture stage differs from native path contract')
                try:
                    entry['rgba_summary'] = rgba_summary(glyph['color'])
                except ValueError:
                    issue.append('invalid/out-of-range glyph diffuse')
                if all(finite(v) for v in glyph['position']):
                    entry['bounds_prepared'] = [min(v[0] for v in glyph['position']), min(v[1] for v in glyph['position']),
                                                max(v[0] for v in glyph['position']), max(v[1] for v in glyph['position'])]
                if p['path'] == 1 and all(finite(v) for v in glyph['uv']):
                    entry['font_reference_uv_texels'] = [[v[0] * 256, v[1] * 128] for v in glyph['uv']]
                    entry['font_reference_lane_offset_already_applied'] = True
                quads.append(entry)
            if p['path'] in (1, 2):
                expected = p['count'] // 6
                if p['count'] % 6 or sorted(g['index'] for g in item['glyphs']) != list(range(expected)):
                    issue.append('missing/duplicate/inconsistent six-vertex glyph groups')
            attempt = self.attempts.get(r['attempt'])
            if not attempt:
                issue.append('no original ATTEMPT record')
            routes = self.routes.get(r['attempt'], [])
            successful_font = p['path'] == 1 and p['count'] > 0 and p['count'] % 6 == 0 and any(
                z['payload']['path'] == 1 and z['payload']['stage'] == 3 and z['payload']['value'] == 1
                and z['payload']['count'] == p['count'] // 6 * 4 and z['serial'] > r['serial'] for z in routes)
            target_key = (r['epoch'], physical_identity(p['target']))
            joined = [c for c in chains_by_source[target_key] if c['resolve_serial'] > r['serial']]
            overlaps = []
            key = (r['epoch'], physical_identity(p['target']), p['lane'])
            for later in groups[key] if p['path'] in (1, 2) and known_target else []:
                lr, lp = later['record'], later['record']['payload']
                if lr['serial'] <= r['serial']:
                    continue
                if not valid_bounds(lp['bounds']) or not valid_bounds(p['bounds']):
                    overlaps.append({'serial': lr['serial'], 'status': 'UNKNOWN_BOUNDS'})
                elif bounds_overlap(p['bounds'], lp['bounds']):
                    overlaps.append({'serial': lr['serial'], 'attempt': lr['attempt'], 'path': lp['path_name'],
                                     'bounds': lp['bounds'], 'state': lp.get('state'),
                                     'status': 'LATER_OVERLAPPING_PREPARED_SUBMISSION',
                                     'pixel_occlusion_proven': False})
            later_clears = [serial for serial in clears_by_target[target_key] if serial > r['serial']] if known_target else []
            generic = generic_by_epoch[r['epoch']]
            output.append({'draw_serial': r['serial'], 'attempt': r['attempt'], 'epoch': r['epoch'],
                           'execution_observation_order': r['serial'], 'observation_command': r['command'],
                           'original_begin_command': attempt['command'] if attempt else None,
                           'recorded_attempt_command': r['attempt_command'],
                           'material': p, 'pixel': item['pixel'], 'quads': quads,
                           'route_events': [{'serial': z['serial'], **z['payload']} for z in routes],
                           'native_font_success_route': successful_font,
                           'status': 'SUCCESS_ROUTE_RECORDED' if successful_font else 'PREPARED_SUBMISSION_ONLY',
                           'observed_present_chains': [c['present'] for c in joined],
                           'later_overlap_candidates': overlaps, 'later_clear_serials': later_clears,
                           'generic_entries_same_epoch': generic, 'issues': issue})
        raw_attempts = []
        for identity in sorted(set(self.immediate) | set(self.attributes) |
                               {i for i, r in self.attempts.items() if r['payload']['primitive'] == 8}):
            chunks = self.immediate[identity]
            commands = [c for r in chunks for c in r['payload']['commands']]
            attrs = self.attributes[identity]
            raw_issues = []
            if identity not in self.attempts:
                raw_issues.append('missing original ATTEMPT state record')
            if len(attrs) != 1:
                raw_issues.append('missing/duplicate original attribute snapshot')
            elif any(attrs[0]['payload']['unknown_mask_bits']):
                raw_issues.append('original attribute validity masks contain unknown bits outside xyzw')
            if any(a['command'] + 1 != b['command'] for a, b in zip(commands, commands[1:])):
                raw_issues.append('missing/duplicate/regressing original immediate command order')
            if not commands or (commands[0]['method'], commands[0]['value']) != (0x17fc, 8):
                raw_issues.append('missing original mode8 BEGIN')
            if not commands or (commands[-1]['method'], commands[-1]['value']) != (0x17fc, 0):
                raw_issues.append('missing original END; partial buffer/tail is not a complete attempt')
            raw_attempts.append({'attempt': identity, 'chunk_serials': [r['serial'] for r in chunks],
                                 'attributes': attrs[0]['payload']['attributes'] if len(attrs) == 1 else None,
                                 'validity_masks': attrs[0]['payload']['validity_masks'] if len(attrs) == 1 else None,
                                 'component_validity': attrs[0]['payload']['component_validity'] if len(attrs) == 1 else None,
                                 'commands': commands, 'issues': raw_issues,
                                 'status': 'ORIGINAL_COMMANDS_RETAINED' if not raw_issues else 'INCOMPLETE_ORIGINAL_ATTEMPT',
                                 'limit': 'No actual fallback or prepared-screen-space path inferred. Recorded component-validity masks describe the initial snapshot; subsequent input methods remain explicitly retained.'})
        return {'schema': 'text510-draw-evidence-v1', 'issues': self.issues,
                'publication_chains': chains, 'draws': output,
                'raw_mode8_attempts': raw_attempts,
                'attempts_without_material_draws': [identity for identity in self.attempts if identity not in drawn_attempts],
                'promotion_verdict': 'NOT_EVALUATED',
                'limits': ['Prepared-submission order is distinct from successful rasterization.',
                           'Missing generic inputs/bounds remain a coverage gap, not NOT CAPTURED.',
                           'Same-epoch overlap is a candidate predicate, not pixel overwrite proof.',
                           'Captured image bytes still require present/physical/hash matching and actual visual inspection.',
                           'Native font reference requires atlas/state/input provenance, not diffuse brightness alone.']}
