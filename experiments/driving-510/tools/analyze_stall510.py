"""Bounded read-only stall510.jsonl analysis. No sampling, game or GPU execution.

Durations come from the common QPC clock and recorded last-present anchors.
Samples identify locations and wait regions, not causation or performance.
"""
from __future__ import annotations
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parent
MAX_SAMPLES, MAX_LINE, MAX_THREADS, MAX_FRAMES = 32768, 8192, 256, 12
WAIT_PHASES = {0: 'outside dispatcher wait', 1: 'wait entry', 2: 'dispatcher lock acquisition region',
               3: 'dispatcher object-check region', 4: 'condition-variable wait region'}
NATIVE_WAIT = re.compile(r'^(?:NtWaitFor(?:Single|Multiple)Objects?|NtDelayExecution|NtWaitForAlertByThreadId|'
                         r'WaitFor(?:Single|Multiple)Objects?(?:Ex)?|Sleep(?:Ex|ConditionVariableSRW)?|'
                         r'Rtl(?:SleepConditionVariableSRW|WaitOnAddress|WaitForAddress)|'
                         r'D3DKMTWaitForSynchronizationObjectFromCpu)$')
UNSIGNED = ('qpc', 'gap_qpc', 'tid', 'generation', 'rip', 'rsp', 'cpu_100ns', 'wall_qpc', 'heartbeat',
            'wait_phase', 'wait_type', 'has_timeout', 'metadata_consistent', 'suspend_previous',
            'context_error', 'resume_error', 'copy_error', 'stack_bytes', 'truncated')
FIELDS = set(UNSIGNED) | {'kind', 'role', 'phase', 'timeout', 'objects', 'frames'}


def require(condition, message):
    if not condition:
        raise ValueError(message)


def strict_json(value):
    def pairs(items):
        out = {}
        for key, val in items:
            require(key not in out, 'duplicate JSON key: ' + key)
            out[key] = val
        return out
    def constant(value):
        raise ValueError('nonfinite JSON constant: ' + value)
    return json.loads(value, object_pairs_hook=pairs, parse_constant=constant)


def unsigned(value, bits=64):
    return type(value) is int and 0 <= value < 1 << bits


def validate_sample(row):
    require(isinstance(row, dict) and set(row) == FIELDS, 'unknown/missing sample fields')
    require(row['kind'] == 'sample', 'unknown log record kind')
    for key in UNSIGNED:
        require(unsigned(row[key]), 'invalid unsigned field: ' + key)
    for key in ('tid', 'generation'):
        require(0 < row[key] < 1 << 32, 'invalid thread identity')
    require(row['generation'] <= MAX_THREADS, 'generation exceeds fixed registry capacity')
    require(type(row['timeout']) is int and -(1 << 63) <= row['timeout'] < 1 << 63, 'invalid signed timeout')
    require(isinstance(row['role'], str) and len(row['role']) <= 31, 'invalid role')
    require(isinstance(row['phase'], str) and len(row['phase']) <= 39, 'invalid phase')
    for key in ('has_timeout', 'metadata_consistent', 'truncated'):
        require(row[key] in (0, 1), 'invalid Boolean word: ' + key)
    require(isinstance(row['objects'], list) and len(row['objects']) <= 64
            and all(unsigned(v, 32) for v in row['objects']), 'invalid dispatcher object list')
    require(isinstance(row['frames'], list) and len(row['frames']) <= MAX_FRAMES, 'invalid frame count')
    require(row['stack_bytes'] <= 65536, 'copied stack exceeds source capacity')
    for frame in row['frames']:
        require(isinstance(frame, dict) and set(frame) == {'address', 'symbol'}, 'invalid frame fields')
        require(unsigned(frame['address']) and isinstance(frame['symbol'], str)
                and len(frame['symbol']) <= 191, 'invalid frame contents')
    return row


def symbol_name(symbol):
    return re.sub(r'\+0x[0-9a-fA-F]+(?:\s.*)?$', '', symbol).split('!')[-1]


def location_category(symbol):
    name = symbol_name(symbol)
    if '[no-local-symbol]' in symbol or name == 'unknown':
        return 'unresolved symbol'
    if name.startswith(('pc508_', 'driving_pc508_')):
        return '508-named location; active path and causal role require source review'
    if name.startswith(('pc450_', 'pc451_', 'pc452_', 'pc453_')):
        return 'prior residency implementation; not evidence specific to the508 change'
    if NATIVE_WAIT.match(name):
        return 'shared host wait/sleep API'
    return 'shared/conditional implementation or title code; profile exclusivity unproved'


def annotate(row, hz, anchor):
    context_ok = row['context_error'] == 0 and row['resume_error'] == 0 and row['rip'] != 0
    symbols = [f['symbol'] for f in row['frames']]
    native_wait = bool(context_ok and symbols and '[no-local-symbol]' not in symbols[0]
                       and NATIVE_WAIT.match(symbol_name(symbols[0])))
    consistent = row['metadata_consistent'] == 1
    phase = row['wait_phase']
    explicit_region = bool(context_ok and consistent and phase == 4 and row['objects'] and row['wait_type'] in (0, 1))
    if explicit_region:
        wait = 'EXPLICIT_DISPATCHER_WAIT_REGION'
    elif native_wait:
        wait = 'EXPLICIT_NATIVE_WAIT_STACK'
    elif not consistent:
        wait = 'UNKNOWN_METADATA_INCONSISTENT'
    elif phase in (1, 2, 3):
        wait = 'DISPATCHER_ENTRY_LOCK_OR_CHECK_REGION'
    else:
        wait = 'NO_EXPLICIT_WAIT_EVIDENCE'
    wall = row['wall_qpc']
    cpu_start = row['qpc'] - wall if wall <= row['qpc'] else None
    if not wall:
        cpu = 'BASELINE_UNAVAILABLE'
    elif cpu_start is None:
        cpu = 'INVALID_WINDOW'
    elif cpu_start < anchor:
        cpu = 'WINDOW_EXTENDS_BEFORE_THIS_NO_PRESENT_INTERVAL'
    elif row['cpu_100ns']:
        cpu = 'CPU_EXECUTION_MEASURED_IN_INTERVAL'
    else:
        cpu = 'NO_CPU_EXECUTION_MEASURED_NOT_PROOF_OF_WAITING'
    classification = ('EXPLICIT_WAIT_EVIDENCE' if explicit_region or native_wait else
                      'CPU_EXECUTION_MEASURED' if cpu == 'CPU_EXECUTION_MEASURED_IN_INTERVAL' else 'UNKNOWN')
    timeout = ('no timeout supplied' if not row['has_timeout'] else
               'relative100ns' if row['timeout'] < 0 else 'absolute FILETIME100ns (0 already expired)')
    return {'qpc': row['qpc'], 'gap_qpc': row['gap_qpc'], 'gap_seconds': row['gap_qpc'] / hz,
            'classification': classification, 'cpu_evidence': cpu,
            'cpu_100ns': row['cpu_100ns'], 'cpu_seconds': row['cpu_100ns'] / 10000000,
            'cpu_window_start_qpc': cpu_start, 'cpu_window_end_qpc': row['qpc'], 'wall_qpc': wall,
            'wait_evidence': wait, 'wait_phase': phase, 'wait_phase_label': WAIT_PHASES.get(phase, 'unknown'),
            'wait_type': row['wait_type'], 'wait_type_label': {0: 'WaitAll', 1: 'WaitAny'}.get(row['wait_type'], 'unknown'),
            'objects': row['objects'], 'has_timeout': row['has_timeout'], 'timeout_raw': row['timeout'],
            'objects_are_current_dispatcher_region': consistent and phase in (1, 2, 3, 4),
            'object_types': 'NOT_RECORDED; wait_type is WaitAll/WaitAny, not event/semaphore object type',
            'timeout_interpretation': timeout, 'metadata_consistent': consistent,
            'heartbeat_qpc': row['heartbeat'], 'heartbeat_phase': row['phase'],
            'rip': row['rip'], 'rsp': row['rsp'], 'frames': row['frames'],
            'frame_categories': [location_category(s) for s in symbols],
            'context_error': row['context_error'], 'resume_error': row['resume_error'], 'copy_error': row['copy_error'],
            'suspend_previous': row['suspend_previous'], 'stack_bytes': row['stack_bytes'], 'truncated': row['truncated'],
            'limit': 'CPU window and sampled wait region may coexist. Snapshot follows the logged QPC; heartbeat is not current instruction proof.'}


def footer_issues(footer):
    out = []
    if not isinstance(footer, dict):
        return ['missing footer']
    for key in ('qpc_hz', 'start_qpc', 'terminal_qpc', 'last_present_qpc', 'has_present', 'max_gap_qpc',
                'thread_count', 'thread_overflow', 'sample_count', 'completed_samples', 'missed_periods',
                'sample_overflow_or_io_error', 'text_on', 'stall_on'):
        if not unsigned(footer.get(key)):
            out.append('missing/invalid footer ' + key)
    if out:
        return out
    if footer.get('checkpoint') != 510 or footer['stall_on'] != 1 or footer['text_on'] != 0:
        out.append('not a separate STALL510-only run')
    if not footer['qpc_hz'] or not footer['start_qpc'] or not footer['start_qpc'] <= footer['last_present_qpc'] <= footer['terminal_qpc']:
        out.append('invalid footer clock ordering')
    if footer['has_present'] not in (0, 1):
        out.append('invalid has_present flag')
    if footer['sample_count'] > MAX_SAMPLES or footer['thread_count'] > MAX_THREADS:
        out.append('source fixed capacity exceeded')
    if footer['sample_overflow_or_io_error'] or footer['thread_overflow']:
        out.append('sample/I/O/thread-registry loss reported')
    return out


def analyze_rows(rows, footer, profile, scene, sample_sink=None):
    issues = footer_issues(footer)
    hz = footer.get('qpc_hz', 0) if isinstance(footer, dict) else 0
    require(type(hz) is int and hz > 0, 'valid footer QPC frequency required')
    start, terminal = footer.get('start_qpc', 0), footer.get('terminal_qpc', 0)
    require(unsigned(start) and start > 0 and unsigned(terminal) and terminal >= start,
            'valid footer start/terminal clock required')
    groups, rejected, identities = {}, Counter(), {}
    total, prior_qpc = 0, -1
    for row in rows:
        total += 1
        require(total <= MAX_SAMPLES, 'sample file exceeds frozen fixed capacity')
        validate_sample(row)
        identity_tid = identities.setdefault(row['generation'], row['tid'])
        require(identity_tid == row['tid'], 'one registry generation names multiple thread IDs')
        qpc, gap = row['qpc'], row['gap_qpc']
        if qpc < prior_qpc:
            rejected['sample QPC regression'] += 1
            continue
        prior_qpc = qpc
        if gap > qpc or qpc < start or qpc > terminal:
            rejected['invalid QPC range or unsigned gap underflow'] += 1
            continue
        anchor = qpc - gap
        if not footer.get('has_present') or anchor <= start:
            rejected['startup/no distinct observed present anchor'] += 1
            continue
        if gap < 3 * hz:
            rejected['below producer three-second sampling threshold'] += 1
            continue
        group = groups.setdefault(anchor, {'anchor_qpc': anchor, 'first_sample_qpc': qpc,
                                           'last_sample_qpc': qpc, 'max_sample_gap_qpc': gap,
                                           'samples': 0, 'threads': {}})
        group['last_sample_qpc'] = qpc
        group['max_sample_gap_qpc'] = max(group['max_sample_gap_qpc'], gap)
        group['samples'] += 1
        key = f'{row["tid"]}:{row["generation"]}'
        thread = group['threads'].setdefault(key, {'tid': row['tid'], 'generation': row['generation'],
                                                  'roles': set(), 'samples': 0, 'classification_counts': Counter(),
                                                  'cpu_evidence_counts': Counter(), 'in_interval_cpu_100ns': 0,
                                                  'first_sample': None, 'last_sample': None, 'stack_examples': {},
                                                  'unretained_stack_occurrences': 0})
        detail = annotate(row, hz, anchor)
        thread['roles'].add(row['role'])
        thread['samples'] += 1
        thread['classification_counts'][detail['classification']] += 1
        thread['cpu_evidence_counts'][detail['cpu_evidence']] += 1
        if detail['cpu_evidence'] == 'CPU_EXECUTION_MEASURED_IN_INTERVAL':
            thread['in_interval_cpu_100ns'] += row['cpu_100ns']
        if thread['first_sample'] is None:
            thread['first_sample'] = detail
        thread['last_sample'] = detail
        signature = tuple((f['address'], f['symbol']) for f in row['frames'])
        if signature in thread['stack_examples']:
            thread['stack_examples'][signature]['occurrences'] += 1
        elif len(thread['stack_examples']) < 8:
            thread['stack_examples'][signature] = {'occurrences': 1, 'sample': detail}
        else:
            thread['unretained_stack_occurrences'] += 1
        if sample_sink:
            sample_sink({'interval_anchor_qpc': anchor, 'tid': row['tid'], 'generation': row['generation'],
                         'role': row['role'], **detail})
    if total != footer.get('sample_count') or total != footer.get('completed_samples'):
        issues.append('file/sample_count/completed_samples differ; terminal coverage incomplete')
    if rejected:
        issues.append('one or more samples cannot establish a valid post-present interval')
    anchors = sorted(groups)
    intervals = []
    for position, anchor in enumerate(anchors):
        group = groups[anchor]
        end_candidates = [anchors[position + 1]] if position + 1 < len(anchors) else []
        if footer.get('last_present_qpc', 0) > anchor:
            end_candidates.append(footer['last_present_qpc'])
        if end_candidates and min(end_candidates) <= group['last_sample_qpc']:
            issues.append('different last-present anchors conflict with sample chronology')
        terminal_same = footer.get('has_present') == 1 and footer.get('last_present_qpc') == anchor and terminal >= group['last_sample_qpc']
        through = terminal if terminal_same else group['last_sample_qpc']
        lower = through - anchor
        threads = []
        for thread in group['threads'].values():
            thread['roles'] = sorted(thread['roles'])
            thread['classification_counts'] = dict(thread['classification_counts'])
            thread['cpu_evidence_counts'] = dict(thread['cpu_evidence_counts'])
            thread['stack_examples'] = list(thread['stack_examples'].values())
            threads.append(thread)
        intervals.append({'profile': profile, 'scene': scene, 'last_present_anchor_qpc': anchor,
                          'first_sample_qpc': group['first_sample_qpc'], 'last_sample_qpc': group['last_sample_qpc'],
                          'sample_supported_duration_qpc': group['max_sample_gap_qpc'],
                          'observed_through_qpc': through, 'duration_lower_bound_qpc': lower,
                          'duration_lower_bound_seconds': lower / hz,
                          'duration_upper_bound_qpc': min(end_candidates) - anchor if end_candidates else None,
                          'duration_exact': False,
                          'end_status': 'OPEN_AT_MATCHING_TERMINAL_FOOTER' if terminal_same else
                                        'RECOVERY_BY_LATER_ANCHOR_ONLY' if end_candidates else 'END_UNKNOWN',
                          'observed_ge20_seconds': lower >= 20 * hz,
                          'samples': group['samples'], 'threads': threads,
                          'causal_508_attribution': 'NOT_ESTABLISHED_BY_SAMPLING',
                          'limit': 'No first-recovery timestamp exists in this log. Later anchor only bounds recovery; stacks are sampled locations.'})
    return {'schema': 'stall510-analysis-v1', 'profile': profile, 'scene': scene,
            'recording_status': 'INCOMPLETE' if issues else 'COMPLETE_RECORDED_PREFIX',
            'issues': issues, 'sample_records': total, 'rejected_sample_counts': dict(rejected),
            'qpc_hz': hz, 'intervals': intervals,
            'observed_ge20_interval_count': sum(i['observed_ge20_seconds'] for i in intervals),
            'eligible_ge20_capture_count': 0 if issues else sum(i['observed_ge20_seconds'] for i in intervals),
            'footer': footer, 'footer_max_gap_used_to_establish_interval': False,
            'limits': ['No20s claim is derived from30-present blocks, startup silence, sample count or footer max_gap alone.',
                       'A matching terminal last-present timestamp may extend an already sampled interval lower bound.',
                       'Zero CPU delta is not proof of waiting. Cross-interval CPU windows cannot locate execution inside this gap.',
                       'Wait phase4 is an instrumented wait region; entry/return and snapshot timing prevent continuous blocked-time claims.',
                       'First/last samples plus at most8 stack signatures per interval/thread are retained; omitted occurrences are counted.',
                       'No complete thread inventory is logged. Lost/inconsistent/missing contexts limit coverage.',
                       '508-named or residency frames are review leads, not causal or profile-exclusive proof. Sampling overhead is not performance data.',
                       'Profile/scene labels are caller-supplied and must be tied to the run profile and actual images.']}


def iter_jsonl(path):
    with Path(path).open('rb') as source:
        line = 0
        while True:
            data = source.readline(MAX_LINE + 1)
            if not data:
                return
            line += 1
            require(len(data) <= MAX_LINE and data.endswith(b'\n'), f'oversized/truncated JSONL line{line}')
            try:
                yield strict_json(data.decode('utf-8'))
            except (ValueError, UnicodeError) as exc:
                raise ValueError(f'line{line}: {exc}') from exc


def sha(path):
    digest = hashlib.sha256()
    with Path(path).open('rb') as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b''):
            digest.update(chunk)
    return digest.hexdigest()


def analyze_file(path, footer_path, profile, scene, annotated_path=None):
    footer = strict_json(Path(footer_path).read_text(encoding='utf-8-sig'))
    output = None
    try:
        if annotated_path:
            annotated_path = Path(annotated_path).resolve()
            require(annotated_path.is_relative_to(ROOT), 'annotated output must stay inside510')
            output = annotated_path.open('x', encoding='utf-8')
        def sink(sample):
            if output:
                output.write(json.dumps(sample, separators=(',', ':'), allow_nan=False) + '\n')
        result = analyze_rows(iter_jsonl(path), footer, profile, scene, sink)
    except ValueError as exc:
        result = {'schema': 'stall510-analysis-v1', 'profile': profile, 'scene': scene,
                  'recording_status': 'INVALID', 'issues': [str(exc)], 'intervals': [],
                  'observed_ge20_interval_count': 0, 'footer': footer,
                  'limit': 'Malformed input is preserved; partial annotated output cannot establish a complete capture.'}
    finally:
        if output:
            output.close()
    result.update(input=str(Path(path).resolve()), input_sha256=sha(path),
                  footer_path=str(Path(footer_path).resolve()), footer_sha256=sha(footer_path),
                  analyzer_sha256=sha(__file__))
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('input', type=Path)
    parser.add_argument('--footer', required=True, type=Path)
    parser.add_argument('--profile', required=True, choices=('A', 'B508', 'fixture'))
    parser.add_argument('--scene', required=True, choices=('paris', 'vehicle', 'fixture'))
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--annotated-jsonl', type=Path)
    args = parser.parse_args()
    output = args.output.resolve()
    require(output.is_relative_to(ROOT), 'all output must remain under510')
    result = analyze_file(args.input, args.footer, args.profile, args.scene, args.annotated_jsonl)
    output.write_text(json.dumps(result, indent=2, allow_nan=False) + '\n', encoding='utf-8')
    print(json.dumps({'recording_status': result['recording_status'], 'samples': result.get('sample_records'),
                      'intervals': len(result['intervals']), 'observed_ge20_intervals': result['observed_ge20_interval_count']}))
    return int(result['recording_status'] != 'COMPLETE_RECORDED_PREFIX')


if __name__ == '__main__':
    raise SystemExit(main())
