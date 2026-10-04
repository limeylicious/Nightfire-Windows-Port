"""Strict reader for the Windows/x64 D510Record ABI from driving_diag510.h.

Record syntax, atlas completeness and footer commitment are independent of
rendering attribution. Passing these checks is not a promotion verdict.
"""
from __future__ import annotations

import hashlib
import io
import json
import math
import struct
from collections import Counter
from pathlib import Path

RECORD_SIZE = 512
HEADER_SIZE = 64
PAYLOAD_SIZE = 448
RING_CAPACITY = 262144
ISSUE_EXAMPLE_LIMIT = 128
PUBLICATION_EXAMPLE_LIMIT = 4096
HEADER = struct.Struct('<iIII6Q')
DRAW = struct.Struct('<8I16f20I4Q12IQ')
GLYPH = struct.Struct('<4I48fQ')
RESOLVE = struct.Struct('<4I48f')
LINK = struct.Struct('<Q4I')
TYPE_NAMES = {1: 'ATTEMPT', 2: 'ROUTE', 3: 'DRAW', 4: 'GLYPH', 5: 'PIXEL', 6: 'ATLAS',
              7: 'RESOLVE', 8: 'CACHE', 9: 'PRESENT', 10: 'CLEAR', 11: 'UNKNOWN', 12: 'END',
              13: 'IMMEDIATE', 14: 'ATTRIBUTES'}
PATH_NAMES = {1: 'font', 2: 'sprite', 3: 'batch', 4: 'geometry', 5: 'generic',
              6: 'resolve', 7: 'clear', 8: 'blit'}
STAGE_NAMES = {0: 'boundary_marker', 1: 'plan', 2: 'collected', 3: 'successful', 4: 'replay', 5: 'refused',
               6: 'generic_indexed_count', 7: 'generic_inline_end_count'}
STATE_NAMES = ('blend_enable', 'blend_src', 'blend_dst', 'alpha_enable', 'alpha_func', 'alpha_ref',
               'depth_enable', 'depth_func', 'depth_write', 'color_write_mask212', 'cull_enable',
               'cull_face', 'front_face', 'left', 'top', 'right', 'bottom', 'fog_color', 'filtered', 'combiner')
EXACT_SIZES = {1: 128, 2: 16, 3: DRAW.size, 4: GLYPH.size, 5: 220,
               7: RESOLVE.size, 8: LINK.size, 9: LINK.size, 10: 32, 14: 320}
assert (HEADER.size, DRAW.size, GLYPH.size, RESOLVE.size, LINK.size) == (64, 264, 216, 208, 24)


class TraceError(ValueError):
    pass


class IssueLog:
    """Exact counts with a bounded sample; a truncated example list is not loss0."""
    def __init__(self):
        self.total = 0
        self.counts = Counter()
        self.examples = []

    def add(self, category, detail):
        self.total += 1
        self.counts[category] += 1
        if len(self.examples) < ISSUE_EXAMPLE_LIMIT:
            self.examples.append(detail)

    def extend(self, category, values):
        for detail in values:
            self.add(category, detail)


def f32_array(payload, offset, count):
    out = []
    for at in range(offset, offset + count * 4, 4):
        value = struct.unpack_from('<f', payload, at)[0]
        out.append(value if math.isfinite(value) else
                   {'nonfinite_f32_bits': f'{struct.unpack_from("<I", payload, at)[0]:08x}'})
    return out


def payload_decode(kind, payload):
    expected = EXACT_SIZES.get(kind)
    if expected is not None and len(payload) != expected:
        raise TraceError(f'{TYPE_NAMES[kind]} payload is {len(payload)} bytes, expected {expected}')
    if kind == 1:
        words = struct.unpack('<32I', payload)
        return {'primitive': words[0], 'target': words[1], 'surface_format': words[2],
                'raw_texture': words[3], 'raw_format': words[4],
                'alpha_enable': words[5], 'blend_enable': words[6], 'depth_enable': words[7],
                'alpha_ref': words[8], 'blend_src': words[9], 'blend_dst': words[10], 'color_mask': words[11],
                'textures': [dict(zip(('offset', 'format', 'filter', 'address'), words[12 + k * 4:16 + k * 4]))
                             for k in range(4)], 'reserved_words': list(words[28:])}
    if kind == 2:
        path, stage, value, count = struct.unpack('<4I', payload)
        return {'path': path, 'path_name': PATH_NAMES.get(path, 'unknown'), 'stage': stage,
                'stage_name': STAGE_NAMES.get(stage, 'unknown'), 'value': value, 'count': count}
    if kind == 3:
        words = struct.unpack_from('<8I', payload)
        out = dict(zip(('path', 'lane', 'target', 'count', 'profile', 'raw_format', 'raw_texture', 'flags'), words))
        out['path_name'] = PATH_NAMES.get(out['path'], 'unknown')
        for key, offset in (('bounds', 32), ('minimum', 48), ('maximum', 64), ('mean', 80)):
            out[key] = f32_array(payload, offset, 4)
        out['state_words'] = list(struct.unpack_from('<20I', payload, 96))
        out['state'] = dict(zip(STATE_NAMES, out['state_words']))
        out['texture_identity'] = list(struct.unpack_from('<4Q', payload, 176))
        for key, offset in (('texture_format', 208), ('filter', 224), ('address', 240)):
            out[key] = list(struct.unpack_from('<4I', payload, offset))
        out['atlas_hash'] = struct.unpack_from('<Q', payload, 256)[0]
        return out
    if kind == 4:
        lane, index, target, reserved = struct.unpack_from('<4I', payload)
        out = {'lane': lane, 'index': index, 'target': target, 'reserved': reserved, 'texture_stage': reserved}
        for key, offset in (('position', 16), ('uv', 80), ('color', 144)):
            out[key] = [f32_array(payload, offset + k * 16, 4) for k in range(4)]
        out['atlas_hash'] = struct.unpack_from('<Q', payload, 208)[0]
        return out
    if kind == 5:
        words = list(struct.unpack('<55I', payload))
        return {'control': words[0], 'program': words[1], 'rgb': words[2:10], 'alpha': words[10:18],
                'rgb_out': words[18:26], 'alpha_out': words[26:34], 'final0': words[34], 'final1': words[35],
                'constant0': words[36:44], 'constant1': words[44:52],
                'final_constant0': words[52], 'final_constant1': words[53], 'white_stage2_242': words[54]}
    if kind == 6:
        if len(payload) < 16:
            raise TraceError('Short ATLAS header')
        identity, offset, total = struct.unpack_from('<QII', payload)
        if total != 256 * 108:
            raise TraceError('ATLAS is not the supplied ABI contract: linear 256x108 A8')
        if offset >= total or offset % 432 or len(payload) != 16 + min(432, total - offset):
            raise TraceError('Invalid ATLAS chunk offset/length')
        return {'hash': identity, 'offset': offset, 'total': total, 'data_hex': payload[16:].hex()}
    if kind == 7:
        source, destination, family, count = struct.unpack_from('<4I', payload)
        return {'source': source, 'destination': destination, 'family': family, 'count': count,
                'attributes': [f32_array(payload, 16 + k * 64, 16) for k in range(3)]}
    if kind in (8, 9):
        return dict(zip(('epoch', 'physical', 'version', 'index', 'reserved'), LINK.unpack(payload)))
    if kind == 10:
        return dict(zip(('path', 'target', 'flags', 'clear_horizontal_1d98', 'clear_vertical_1d9c',
                         'clear_color_1d90', 'clear_zstencil_1d8c', 'surface_format'), struct.unpack('<8I', payload)))
    if kind == 13:
        if len(payload) < 8:
            raise TraceError('Short IMMEDIATE header')
        count, primitive = struct.unpack_from('<2I', payload)
        if not 1 <= count <= 27 or len(payload) != 8 + count * 16:
            raise TraceError('IMMEDIATE count/length exceeds or differs from the fixed27-item payload')
        if primitive != 8:
            raise TraceError('IMMEDIATE primitive is outside the supplied mode8 source contract')
        commands = [dict(zip(('command', 'method', 'value'), struct.unpack_from('<QII', payload, 8 + i * 16)))
                    for i in range(count)]
        if any(not c['command'] for c in commands) or any(a['command'] >= b['command'] for a, b in zip(commands, commands[1:])):
            raise TraceError('IMMEDIATE command order is invalid')
        return {'count': count, 'primitive': primitive, 'commands': commands,
                'interpretation': 'original pre-interceptor commands; no actual fallback route inferred'}
    if kind == 14:
        masks = list(struct.unpack_from('<16I', payload, 256))
        return {'attributes': [f32_array(payload, i * 16, 4) for i in range(16)],
                'validity_masks': masks,
                'component_validity': [[bool(mask & (1 << component)) for component in range(4)] for mask in masks],
                'unknown_mask_bits': [mask & ~15 for mask in masks],
                'interpretation': 'original pre-collector current defaults and validity masks; not prepared vertices'}
    # The enum names exist but no payload/source contract is defined yet.
    return {'uninterpreted_hex': payload.hex(), 'semantic_contract': 'unspecified'}


def iter_records(stream):
    index = 0
    while True:
        raw = stream.read(RECORD_SIZE)
        if not raw:
            return
        if len(raw) != RECORD_SIZE:
            raise TraceError(f'Truncated record {index + 1}: {len(raw)} bytes')
        ready, kind, tid, size, qpc, epoch, command, attempt, serial, attempt_command = HEADER.unpack_from(raw)
        if ready != 1:
            raise TraceError(f'Record {index + 1} not committed: ready={ready}')
        if kind not in TYPE_NAMES:
            raise TraceError(f'Unknown record type {kind} at {index + 1}')
        if not tid or not qpc or not epoch or size > PAYLOAD_SIZE or serial != index + 1:
            raise TraceError(f'Invalid record header at {index + 1}')
        # The current ring reuses flushed slots and overwrites only `size`
        # payload bytes. Bytes beyond that bound are deliberately uninterpreted.
        payload = payload_decode(kind, raw[HEADER_SIZE:HEADER_SIZE + size])
        yield {'type': TYPE_NAMES[kind], 'type_id': kind, 'tid': tid, 'qpc': qpc,
               'epoch': epoch, 'command': command, 'attempt': attempt, 'serial': serial,
               'attempt_command': attempt_command, 'payload_bytes': size, 'payload': payload}
        index += 1


def decode_binary(data):
    """Small synthetic traces only; CLI streams real traces instead."""
    return list(iter_records(io.BytesIO(data)))


def fnv1a64(data):
    value = 14695981039346656037
    for byte in data:
        value = ((value ^ byte) * 1099511628211) & 0xffffffffffffffff
    return value


class AtlasAssembly:
    def __init__(self):
        self.atlases = {}

    def add(self, payload):
        identity = payload['hash']
        if identity not in self.atlases and len(self.atlases) >= 1024:
            raise TraceError('Atlas identity count exceeds producer capacity')
        item = self.atlases.setdefault(identity, {'total': payload['total'], 'chunks': {}})
        if item['total'] != payload['total'] or payload['offset'] in item['chunks']:
            raise TraceError('Conflicting atlas total or duplicate atlas chunk')
        item['chunks'][payload['offset']] = bytes.fromhex(payload['data_hex'])

    def finish(self):
        reports, issues = [], []
        for identity, item in self.atlases.items():
            expected = list(range(0, item['total'], 432))
            missing = [offset for offset in expected if offset not in item['chunks']]
            report = {'hash': identity, 'total': item['total'], 'chunks': len(item['chunks']),
                      'missing_offsets': missing, 'complete': not missing}
            if missing:
                issues.append(f'Atlas {identity:016x} has missing chunks')
            else:
                data = b''.join(item['chunks'][offset] for offset in expected)
                if fnv1a64(data) != identity:
                    issues.append(f'Atlas {identity:016x} content hash mismatch')
                    report['complete'] = False
                report['sha256'] = hashlib.sha256(data).hexdigest()
            reports.append(report)
        return reports, issues


def validate_footer(footer, records, allowed_fixture_reasons=()):
    issues = []
    if not isinstance(footer, dict):
        return ['Missing terminal footer']
    if type(footer.get('checkpoint')) is not int or footer.get('checkpoint') != 510 or type(footer.get('text_on')) is not int or footer.get('text_on') != 1:
        issues.append('Footer is not a TEXT510 run')
    if type(footer.get('stall_on')) is not int or footer.get('stall_on') != 0:
        issues.append('Part1 requires stall locator OFF')
    for field in ('text_reserved', 'text_written', 'text_overflow_or_io_error'):
        if type(footer.get(field)) is not int or footer[field] < 0:
            issues.append('Invalid/missing footer ' + field)
    if issues:
        return issues
    if footer['text_reserved'] != footer['text_written'] or footer['text_written'] != records:
        issues.append('File/committed/reserved counts differ; pending, lost, stale footer or extra tail')
    if footer['text_overflow_or_io_error']:
        issues.append('Recorder overflow or I/O error')
    if type(footer.get('qpc_hz')) is not int or footer['qpc_hz'] <= 0:
        issues.append('Missing/invalid QPC frequency')
    for field in ('start_qpc', 'terminal_qpc'):
        if type(footer.get(field)) is not int or footer[field] <= 0:
            issues.append('Missing/invalid footer ' + field)
    if type(footer.get('start_qpc')) is int and type(footer.get('terminal_qpc')) is int:
        if footer['terminal_qpc'] < footer['start_qpc']:
            issues.append('Footer time precedes start')
    if footer.get('reason') not in ('atexit', 'locator-own-deadline', 'display-window-close', 'original-watchdog-deadline') + tuple(allowed_fixture_reasons):
        issues.append('Unspecified terminal reason')
    return issues


def summarize_records(records, footer, record_sink=None, allowed_fixture_reasons=()):
    """Streaming syntax summary; preserves uncertain semantics explicitly."""
    counts, routes, atlas = Counter(), Counter(), AtlasAssembly()
    issues, atlas_refs, links = IssueLog(), set(), []
    link_count = 0
    last_qpc, last_serial, first_qpc, draws, glyphs = 0, 0, None, 0, 0
    for record in records:
        if record_sink:
            record_sink(record)
        kind, payload = record['type'], record['payload']
        counts[kind] += 1
        last_serial = record['serial']
        last_qpc = max(last_qpc, record['qpc'])
        first_qpc = record['qpc'] if first_qpc is None else min(first_qpc, record['qpc'])
        if kind == 'ATLAS':
            atlas.add(payload)
        elif kind == 'ROUTE':
            known = payload['path_name'] != 'unknown' and payload['stage_name'] != 'unknown'
            routes[f'{payload["path"]}:{payload["stage"]}' if known else 'unknown_route_or_stage'] += 1
            if not known:
                issues.add('unknown_route_or_stage', f'Unknown route/stage at record{last_serial}: path={payload["path"]} stage={payload["stage"]}')
        elif kind in ('DRAW', 'GLYPH'):
            if payload['atlas_hash']:
                atlas_refs.add(payload['atlas_hash'])
                if len(atlas_refs) > 1024:
                    raise TraceError('Referenced atlas identities exceed the producer1024-identity bound')
            if payload['lane'] not in (0, 1):
                issues.add('unmodeled_lane', f'Unmodeled lane at record{last_serial}')
            if kind == 'DRAW' and payload['path_name'] == 'unknown':
                issues.add('unknown_draw_path', f'Unknown DRAW path at record{last_serial}')
            if kind == 'DRAW':
                basic = payload['path'] == 4 and payload['profile'] in (214, 216, 220)
                if payload['flags'] != int(basic):
                    issues.add('inconsistent_basic_flags', f'Unknown/inconsistent material/basic flags at record{last_serial}')
            elif payload['texture_stage'] not in (0, 3):
                issues.add('unknown_glyph_stage', f'Unknown glyph texture stage at record{last_serial}')
            draws += kind == 'DRAW'
            glyphs += kind == 'GLYPH'
        elif kind in ('CACHE', 'PRESENT', 'RESOLVE'):
            link_count += 1
            if len(links) < PUBLICATION_EXAMPLE_LIMIT:
                links.append({'serial': last_serial, 'header_epoch': record['epoch'],
                              'command': record['command'], 'type': kind, 'payload': payload})
        elif kind in ('UNKNOWN', 'END'):
            issues.add('unspecified_' + kind.lower(), f'Unspecified {kind} semantic contract at record{last_serial}')
    atlas_reports, atlas_issues = atlas.finish()
    issues.extend('atlas_integrity', atlas_issues)
    present_atlas = {a['hash'] for a in atlas_reports if a['complete']}
    for identity in sorted(atlas_refs - present_atlas):
        issues.add('missing_referenced_atlas', f'Referenced atlas {identity:016x} not complete')
    footer_issues = validate_footer(footer, last_serial, allowed_fixture_reasons)
    issues.extend('footer_integrity', footer_issues)
    if isinstance(footer, dict) and type(footer.get('terminal_qpc')) is int and last_qpc > footer['terminal_qpc']:
        issues.add('records_after_footer', 'Records occur after selected footer')
    if first_qpc is not None and isinstance(footer, dict) and type(footer.get('start_qpc')) is int and first_qpc < footer['start_qpc']:
        issues.add('records_before_start', 'Records occur before declared start')
    return {'schema': 'text510-binary-summary-v1', 'record_size': RECORD_SIZE,
            'payload_sizes': {TYPE_NAMES[k]: v for k, v in EXACT_SIZES.items()},
            'records': last_serial, 'counts': dict(counts), 'route_counts': dict(routes),
            'first_qpc': first_qpc, 'last_qpc': last_qpc, 'draws': draws, 'glyphs': glyphs,
            'atlases': atlas_reports, 'publication_evidence': links,
            'publication_evidence_total': link_count,
            'publication_evidence_retained': len(links),
            'publication_evidence_omitted': link_count - len(links),
            'publication_evidence_scope': 'bounded first4096 examples only; use draw analysis or record stream for full attribution',
            'record_syntax_status': 'PARSED_COMPLETE_RECORDS',
            'footer_status': 'INCOMPLETE' if footer_issues else 'COMMITTED_PREFIX_COMPLETE',
            'syntax_status': 'INCOMPLETE' if issues.total else 'BINARY_AND_FOOTER_CHECKS_PASSED',
            'issues': issues.examples, 'issue_count': issues.total, 'issue_counts': dict(issues.counts),
            'issue_examples_retained': len(issues.examples),
            'issue_examples_omitted': issues.total - len(issues.examples),
            'promotion_verdict': 'NOT_EVALUATED',
            'streaming_storage_bounds': {'input_record_bytes': RECORD_SIZE,
                                         'issue_examples': ISSUE_EXAMPLE_LIMIT,
                                         'publication_examples': PUBLICATION_EXAMPLE_LIMIT,
                                         'atlas_identities': 1024, 'a8_bytes_per_atlas': 27648,
                                         'route_count_buckets': len(PATH_NAMES) * len(STAGE_NAMES) + 1,
                                         'scope': 'statistics only; optional full draw-analysis retains its input detail separately'},
            'frame_join_status': 'PRODUCER_SEMANTICS_REVIEW_PENDING',
            'footer': footer,
            'limits': ['Exact source state-word mapping and producer attribution remain separate checks.',
                       'Counter/epoch/overlap values are evidence, not inferred final-frame causality.',
                       'A deadline footer covers its stated prefix only; no terminal-tail extrapolation.']}


def inspect_file(path, footer_path=None, *, fixture_scope=False):
    """Read-only structural wrapper for independent fixture callers."""
    source = Path(path)
    footer = None
    if footer_path is not None:
        def pairs(items):
            result = {}
            for key, value in items:
                if key in result:
                    raise TraceError('Duplicate footer key: ' + key)
                result[key] = value
            return result

        def constant(value):
            raise TraceError('Nonfinite footer constant: ' + value)

        footer = json.loads(Path(footer_path).read_text(encoding='utf-8-sig'),
                            object_pairs_hook=pairs, parse_constant=constant)
    try:
        with source.open('rb') as stream:
            report = summarize_records(iter_records(stream), footer,
                                       allowed_fixture_reasons=('fixture-text-complete', 'fixture-stall-complete') if fixture_scope else ())
    except TraceError as exc:
        report = {'schema': 'text510-binary-summary-v1', 'syntax_status': 'INVALID_BINARY',
                  'record_syntax_status': 'INVALID_BINARY', 'footer_status': 'NOT_VALIDATED',
                  'issues': [str(exc)], 'promotion_verdict': 'NOT_EVALUATED'}
    digest = hashlib.sha256()
    with source.open('rb') as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b''):
            digest.update(chunk)
    report['input'] = str(source.resolve())
    report['input_sha256'] = digest.hexdigest()
    report['footer_path'] = str(Path(footer_path).resolve()) if footer_path else None
    report['scope'] = 'EXPLICIT_FIXTURE_STRUCTURE_ONLY' if fixture_scope else 'LIVE_TRACE_STRUCTURE'
    return report
