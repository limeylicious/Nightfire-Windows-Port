"""TEXT510 binary reader and independent semantic analysis scaffold.

Normalized input is an explicit test/review interface, not a live trace format.
This module never infers a frame version from a present counter or promotes a
profile from color statistics. All output paths are constrained to perf510.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path

from text510_reference import bounds_overlap, rgba_summary, valid_bounds
from text510_binary import TraceError, decode_binary, iter_records, summarize_records
from text510_analysis import DrawAnalysis

ROOT = Path(__file__).resolve().parent
SCHEMA = 'text510-normalized-scaffold-v1'
KNOWN_PATHS = {'font348_native', 'font348_prefix_replay', 'generic_direct',
               'generic_replay', 'batch', 'cpu', 'renderer352', 'geometry350',
               'intercept201', 'unknown'}


def integer(value, minimum=0):
    return type(value) is int and value >= minimum


def strict_json(path):
    def pairs(items):
        out = {}
        for key, value in items:
            if key in out:
                raise ValueError('Duplicate JSON key: ' + key)
            out[key] = value
        return out

    def invalid(value):
        raise ValueError('Nonfinite JSON constant: ' + value)

    return json.loads(Path(path).read_text(encoding='utf-8-sig'),
                      object_pairs_hook=pairs, parse_constant=invalid)


def exact_frame_join(draw, publication, capture):
    """Only explicit, complete same-target provenance qualifies a capture join."""
    issues = []
    if publication.get('provenance_complete') is not True:
        issues.append('publication provenance not complete')
    for item, name in ((draw, 'draw'), (publication, 'publication'), (capture, 'capture')):
        if not isinstance(item.get('surface_id'), str) or not item['surface_id']:
            issues.append(name + ' has no surface identity')
        if not integer(item.get('version')):
            issues.append(name + ' has no target version')
    if any(draw.get(k) != publication.get(k) or capture.get(k) != publication.get(k)
           for k in ('surface_id', 'version')):
        issues.append('surface/version differs')
    if not integer(capture.get('present'), 1) or capture.get('present') != publication.get('present'):
        issues.append('capture/publication present differs')
    if draw.get('id') not in publication.get('draw_ids', []):
        issues.append('draw not explicitly linked to publication')
    digest = capture.get('sha256')
    if not isinstance(digest, str) or len(digest) != 64 or any(c not in '0123456789abcdef' for c in digest):
        issues.append('capture has no valid content hash')
    return {'status': 'JOINED' if not issues else 'UNRESOLVED', 'issues': issues,
            'present': capture.get('present') if not issues else None}


def later_overlap_candidates(text_draw, draws):
    """Same published target-version overlap only; this is never occlusion proof."""
    candidates, unknown = [], []
    order = text_draw.get('execution_order')
    if not integer(order):
        return {'candidates': [], 'unknown': ['text draw lacks execution order'],
                'pixel_occlusion_proven': False}
    if not text_draw.get('surface_id') or not integer(text_draw.get('version')):
        return {'candidates': [], 'unknown': ['text draw lacks target/version'],
                'pixel_occlusion_proven': False}
    for draw in draws:
        if draw.get('id') == text_draw.get('id'):
            continue
        later = draw.get('execution_order')
        if not integer(later):
            unknown.append({'draw_id': draw.get('id'), 'reason': 'unknown execution order'})
            continue
        if later <= order:
            continue
        if draw.get('result') in ('refused', 'held', 'no_draw'):
            continue
        if not draw.get('surface_id') or not integer(draw.get('version')):
            unknown.append({'draw_id': draw.get('id'), 'reason': 'unknown target/version'})
            continue
        if any(draw[k] != text_draw[k] for k in ('surface_id', 'version')):
            continue
        if not draw.get('lanes') or not text_draw.get('lanes'):
            unknown.append({'draw_id': draw.get('id'), 'reason': 'missing per-lane bounds'})
            continue
        for text_lane in text_draw['lanes']:
            for lane in draw['lanes']:
                if lane.get('lane') != text_lane.get('lane'):
                    continue
                a, b = text_lane.get('bounds', []), lane.get('bounds', [])
                if not valid_bounds(a) or not valid_bounds(b):
                    unknown.append({'draw_id': draw.get('id'), 'lane': lane.get('lane'),
                                    'reason': 'unknown or invalid bounds'})
                elif bounds_overlap(a, b):
                    candidates.append({'draw_id': draw['id'], 'execution_order': later,
                                       'command_order': draw.get('command_order'),
                                       'path': draw.get('path'), 'lane': lane['lane'],
                                       'bounds': b, 'predicate': 'later positive-area bounds overlap',
                                       'pixel_occlusion_proven': False})
    return {'candidates': candidates, 'unknown': unknown, 'pixel_occlusion_proven': False}


def analyze_normalized(document):
    if document.get('schema') != SCHEMA:
        raise ValueError('Not the explicit normalized scaffold schema')
    for key in ('draws', 'publications', 'captures', 'prompts'):
        if not isinstance(document.get(key), list):
            raise ValueError('Missing normalized array: ' + key)
    issues, draw_reports, ids = [], [], set()
    terminal = document.get('terminal', {})
    if terminal.get('complete') is not True:
        issues.append('missing/incomplete terminal record')
    for key in ('lost_records', 'unready_records'):
        value = terminal.get(key)
        if not integer(value) or value:
            issues.append(key + ' is missing, invalid, or nonzero')
    if document.get('all_target_writers_covered') is not True:
        issues.append('complete target-writer coverage not established')
    previous_sequence = -1
    for draw in document['draws']:
        local = []
        ident = draw.get('id')
        if not integer(ident, 1) or ident in ids:
            local.append('invalid/duplicate draw id')
        ids.add(ident)
        seq = draw.get('sequence')
        if not integer(seq) or seq <= previous_sequence:
            local.append('record sequence is invalid or not increasing')
        else:
            previous_sequence = seq
        for key in ('attempt', 'command_order', 'execution_order'):
            if not integer(draw.get(key)):
                local.append('missing/invalid ' + key)
        if draw.get('path') not in KNOWN_PATHS or draw.get('path') == 'unknown':
            local.append('unknown actual execution path')
        if draw.get('result') not in ('drawn', 'refused', 'held', 'no_draw'):
            local.append('unknown execution result')
        lanes = draw.get('lanes', [])
        lane_ids = [lane.get('lane') for lane in lanes]
        if not lanes or len(set(lane_ids)) != len(lane_ids) or any(type(v) is not int or v not in (0, 1) for v in lane_ids):
            local.append('missing/duplicate/invalid lanes')
        if any(not valid_bounds(lane.get('bounds', [])) for lane in lanes):
            local.append('invalid per-lane bounds')
        quads, all_colors = [], []
        for i, quad in enumerate(draw.get('quads', [])):
            try:
                color = quad['rgba']
                rgba_summary([color])
                all_colors.extend([color] * 4)
                quads.append({'quad': i, 'rgba': color, 'xy': quad.get('xy'), 'uv': quad.get('uv')})
            except (KeyError, TypeError, ValueError):
                local.append('quad has invalid or missing RGBA')
        if draw.get('text_or_hud') is True and not quads:
            local.append('text/HUD attempt lacks per-quad data')
        joins = []
        for pub in document['publications']:
            if ident in pub.get('draw_ids', []):
                for capture in document['captures']:
                    if capture.get('present') == pub.get('present'):
                        joins.append(exact_frame_join(draw, pub, capture))
        if draw.get('text_or_hud') is True and not any(j['status'] == 'JOINED' for j in joins):
            local.append('text/HUD draw has no exact capture join')
        draw_reports.append({'draw_id': ident, 'attempt': draw.get('attempt'),
                             'path': draw.get('path'), 'result': draw.get('result'),
                             'per_quad': quads,
                             'rgba_summary': rgba_summary(all_colors) if all_colors else None,
                             'frame_joins': joins,
                             'later_overlap': later_overlap_candidates(draw, document['draws'])
                             if draw.get('text_or_hud') is True else None,
                             'issues': local})
        issues.extend({'draw_id': ident, 'reason': item} for item in local)
    for prompt in document['prompts']:
        if prompt.get('visible') is True:
            matched = [r for r in draw_reports if r['draw_id'] in prompt.get('draw_ids', [])
                       and any(j['status'] == 'JOINED' and j['present'] == prompt.get('present')
                               for j in r['frame_joins'])]
            if not matched:
                issues.append({'prompt': prompt.get('label'),
                               'reason': 'visible prompt lacks a linked draw; cannot classify NOT CAPTURED'})
    return {'schema': 'text510-semantic-report-v1', 'binary_abi_validated': False,
            'evidence_status': 'INCOMPLETE' if issues else 'NORMALIZED_CHECKS_PASSED',
            'promotion_verdict': 'NOT_EVALUATED', 'issues': issues, 'draws': draw_reports,
            'limits': ['Normalized inputs are assertions supplied by an adapter/test, not a live binary proof.',
                       'Bright diffuse inputs alone never imply a rendering failure.',
                       'Bounds overlap alone never establishes pixel occlusion.',
                       'A capture join needs explicit target-version provenance.']}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('input', type=Path)
    parser.add_argument('--normalized', action='store_true', help='Explicit semantic scaffold JSON, not live binary')
    parser.add_argument('--footer', type=Path, help='Explicit final selected diag510-footer-N.json')
    parser.add_argument('--records-jsonl', type=Path, help='Optional lossless decoded records under510')
    parser.add_argument('--draws-json', type=Path, help='Per-draw/lane inputs and observed publication chains under510')
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    if not output.is_relative_to(ROOT):
        raise ValueError('All output must stay inside perf510')
    if args.normalized:
        report = analyze_normalized(strict_json(args.input))
    else:
        records_path = args.records_jsonl.resolve() if args.records_jsonl else None
        draws_path = args.draws_json.resolve() if args.draws_json else None
        if records_path and not records_path.is_relative_to(ROOT):
            raise ValueError('Decoded records output must stay inside perf510')
        if draws_path and not draws_path.is_relative_to(ROOT):
            raise ValueError('Draw analysis output must stay inside perf510')
        draw_analysis = DrawAnalysis() if draws_path else None
        footer = strict_json(args.footer) if args.footer else None
        output_file = None
        try:
            if records_path:
                records_path.parent.mkdir(parents=True, exist_ok=True)
                output_file = records_path.open('x', encoding='utf-8')

            def sink(record):
                if draw_analysis:
                    draw_analysis.add(record)
                if output_file:
                    output_file.write(json.dumps(record, allow_nan=False, separators=(',', ':')) + '\n')

            with args.input.open('rb') as stream:
                report = summarize_records(iter_records(stream), footer, sink)
        except TraceError as exc:
            report = {'schema': 'text510-binary-summary-v1', 'syntax_status': 'INVALID_BINARY',
                      'issues': [str(exc)], 'promotion_verdict': 'NOT_EVALUATED',
                      'footer': footer, 'frame_join_status': 'UNRESOLVED',
                      'limits': ['Malformed input is preserved; no partial record stream can qualify.']}
        finally:
            if output_file:
                output_file.close()
        if args.footer:
            report['footer_path'] = str(args.footer.resolve())
            report['footer_sha256'] = hashlib.sha256(args.footer.read_bytes()).hexdigest()
        report['decoded_records_path'] = str(records_path) if records_path else None
        if draw_analysis:
            draws = draw_analysis.finish()
            draws['trace_integrity_status'] = report['syntax_status']
            draws['trace_issues'] = report['issues']
            draws_path.parent.mkdir(parents=True, exist_ok=True)
            with draws_path.open('x', encoding='utf-8') as destination:
                json.dump(draws, destination, indent=2, allow_nan=False)
                destination.write('\n')
            report['draw_analysis_path'] = str(draws_path)
    report['input'] = str(args.input.resolve())
    digest = hashlib.sha256()
    with args.input.open('rb') as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b''):
            digest.update(chunk)
    report['input_sha256'] = digest.hexdigest()
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(report, indent=2, allow_nan=False) + '\n', encoding='utf-8')
    print(json.dumps({'evidence_status': report.get('evidence_status', report.get('syntax_status')), 'issues': len(report['issues']),
                      'promotion_verdict': report['promotion_verdict']}))
    return 1 if report['issues'] else 0


if __name__ == '__main__':
    raise SystemExit(main())
