#!/usr/bin/env python3
"""Compare bounded LAN desync snapshots. Exit 0: retained data agrees, 1: difference,
2: incomplete/incompatible input. Never a full-match or retail-compatibility verdict.
Use --match-a/--match-b (one-based) when a log contains several snapshot dumps.
"""
# GeneralsX @feature Codex 22/09/2026 Strict offline comparison of universal snapshots.
import argparse
from pathlib import Path
import re
import sys

FIELDS = ('private_status', 'transform', 'id', 'upgrades', 'experience', 'health',
          'weapon_bonus', 'damage_scalar', 'weapon_0', 'weapon_1', 'weapon_2')
STAGES = ('objects', 'rng', 'partition', 'players', 'ai', 'crc')


def number(text, base=10, low=0, high=0xFFFFFFFFFFFFFFFF):
    value = int(text, base)
    if not low <= value <= high:
        raise ValueError('Number outside schema bounds')
    return value


def words(text, size):
    values = tuple(number(v, 16, high=0xFFFFFFFF) for v in text.split(','))
    if len(values) != size:
        raise ValueError('Wrong word count')
    return values


def name(text):
    return '' if text == '-' else bytes.fromhex(text).decode('utf-8', errors='strict')


def parse(lines):
    matches, current = [], None
    for line in lines:
        found = re.search(r'\[GX-LAN-SNAPSHOT\] (\S+) (.*)', line)
        if not found:
            continue
        kind, payload = found.groups()
        pairs = re.findall(r'(\w+)=([^\s]+)', payload)
        d = dict(pairs)
        if len(d) != len(pairs):
            raise ValueError('Duplicate record key')
        if kind in ('armed', 'unavailable'):
            if current is not None:
                raise ValueError('New match inside incomplete dump')
            continue
        if kind == 'begin':
            if current is not None:
                raise ValueError('Unterminated snapshot')
            if d['schema'] != '1' or d['math'] not in ('gamemath', 'native') or d['reason'] not in ('different_crc', 'missing_crc', 'match_end'):
                raise ValueError('Unsupported snapshot schema/reason')
            current = dict(meta=d, frames={}, commands=[])
            number(d['map_crc'], 16, high=0xFFFFFFFF)
            number(d['game_seed'], low=-0x80000000, high=0x7FFFFFFF)
            number(d['crc_interval'], low=1, high=0x7FFFFFFF)
            number(d['validation_frame'], low=-1, high=0x7FFFFFFF)
            number(d['frames'], low=1, high=8)
            number(d['commands'], high=4096)
            if d['object_limit'] != '2048' or d['arg_limit'] != '32':
                raise ValueError('Unsupported capture limits')
        elif current is None:
            raise ValueError('Record without snapshot header')
        elif kind == 'frame':
            frame = number(d['frame'], high=0x7FFFFFFF)
            frames = current['frames']
            if frames and frame <= max(frames):
                raise ValueError('Duplicate or unordered frame')
            stages = words(d['stages'], 5)
            total = number(d['total'], high=0x7FFFFFFF)
            captured = number(d['captured'], high=2048)
            if captured != min(total, 2048) or d['rng_present'] != '1':
                raise ValueError('Invalid object coverage or missing RNG state')
            frames[frame] = dict(crc=number(d['crc'], 16, high=0xFFFFFFFF), stages=stages,
                                 rng=words(d['rng'], 6), rng_crc=number(d['rng_crc'], 16, high=0xFFFFFFFF),
                                 total=total, captured=captured, objects=[],
                                 commands_before=number(d['commands_before']))
        elif kind == 'object':
            frame = number(d['frame'])
            if frame not in current['frames'] or frame != max(current['frames']):
                raise ValueError('Object without current frame')
            objects = current['frames'][frame]['objects']
            if number(d['order']) != len(objects) or len(objects) >= 2048:
                raise ValueError('Duplicate/nonsequential object')
            if number(d['mask'], 16) != (1 << len(FIELDS)) - 1 or d['transform_count'] != '12':
                raise ValueError('Incomplete object field/transform coverage')
            obj = dict(id=number(d['id'], 16, high=0xFFFFFFFF), name=name(d['name']),
                       name_cut=number(d['name_cut'], high=1), start=number(d['start'], 16, high=0xFFFFFFFF),
                       crc=number(d['crc'], 16, high=0xFFFFFFFF), fields=words(d['fields'], len(FIELDS)),
                       transform=words(d['transform'], 12))
            if obj['fields'][-1] != obj['crc'] or (objects and objects[-1]['crc'] != obj['start']):
                raise ValueError('Broken rolling object CRC chain')
            objects.append(obj)
        elif kind == 'command':
            commands = current['commands']
            seq, frame = number(d['seq']), number(d['frame'], high=0x7FFFFFFF)
            if commands and (seq != commands[-1]['seq'] + 1 or frame < commands[-1]['frame']):
                raise ValueError('Nonsequential command history')
            argc, captured = number(d['argc'], high=255), number(d['captured'], high=32)
            if captured != min(argc, 32):
                raise ValueError('Invalid argument coverage')
            args = []
            for item in (() if d['args'] == '-' else d['args'].split(',')):
                pieces = item.split(':')
                if len(pieces) != 6:
                    raise ValueError('Invalid typed command argument')
                args.append((number(pieces[0], high=0x7FFFFFFF), number(pieces[1], high=4),
                             *(number(v, 16, high=0xFFFFFFFF) for v in pieces[2:])))
                arg_type, count, *data = args[-1]
                sizes = (1, 1, 1, 1, 1, 1, 3, 2, 4, 1, 1)
                expected = sizes[arg_type] if arg_type < len(sizes) else 0
                if count != expected or any(data[count:]) or (arg_type == 2 and data[0] not in (0, 1)):
                    raise ValueError('Invalid active argument data or nonzero padding')
            if len(args) != captured:
                raise ValueError('Argument count mismatch')
            commands.append(dict(seq=seq, frame=frame, player=number(d['player'], low=-1, high=15),
                                 type=number(d['type'], low=1001, high=1998), name=name(d['name']),
                                 name_cut=number(d['name_cut'], high=1), argc=argc, args=tuple(args)))
        elif kind == 'end':
            m = current['meta']
            frames, commands = current['frames'], current['commands']
            if (len(frames) != number(m['frames']) or d['frames'] != m['frames'] or
                    len(commands) != number(m['commands']) or d['commands'] != m['commands']):
                raise ValueError('Missing frame/command records')
            generations, command_total = number(m['generations']), number(m['command_total'])
            if len(frames) != min(generations, 8) or len(commands) != min(command_total, 4096):
                raise ValueError('Ring coverage inconsistent with totals')
            if commands and (commands[0]['seq'] != command_total - len(commands) or
                             commands[-1]['seq'] != command_total - 1):
                raise ValueError('Missing command ring endpoints')
            previous, before = None, 0
            for f, values in frames.items():
                if previous is not None and f - previous != int(m['crc_interval']):
                    raise ValueError('Missing normal CRC checkpoint')
                if len(values['objects']) != values['captured']:
                    raise ValueError('Missing object records')
                if not before <= values['commands_before'] <= command_total:
                    raise ValueError('Invalid command boundary')
                if values['captured'] == values['total'] and values['objects'] and values['objects'][-1]['crc'] != values['stages'][0]:
                    raise ValueError('Object stage disagrees with object records')
                if values['stages'][-1] != values['crc']:
                    raise ValueError('Final CRC disagrees with final stage')
                previous, before = f, values['commands_before']
            matches.append(current)
            current = None
        else:
            raise ValueError('Unknown snapshot record')
    if current is not None:
        raise ValueError('Truncated snapshot: missing end record')
    return matches


def object_difference(a, b):
    for i, (x, y) in enumerate(zip(a['objects'], b['objects'])):
        prefix = f"Object order {i}, id={x['id']:08X}, type={x['name']}: "
        if (x['id'], x['name']) != (y['id'], y['name']):
            return prefix + f"identity/order differs (B id={y['id']:08X}, type={y['name']})."
        if x['start'] != y['start']:
            return prefix + 'CRC differs before object entry.'
        for field, left, right in zip(FIELDS, x['fields'], y['fields']):
            if left != right:
                result = prefix + f"first field boundary {field}: A={left:08X}, B={right:08X}."
                if field == 'transform':
                    for j, (u, v) in enumerate(zip(x['transform'], y['transform'])):
                        if u != v:
                            result += f" First raw matrix word {j}: A={u:08X}, B={v:08X}."
                            break
                return result
        if x['transform'] != y['transform']:
            return prefix + 'raw transform differs despite equal rolling CRC (possible collision).'
    if a['total'] != b['total']:
        return f"Object count differs: A={a['total']}, B={b['total']}."
    return None


def command_context(a, b, frame, previous):
    result = []
    for label, match in (('A', a), ('B', b)):
        selected = [c for c in match['commands'] if previous < c['frame'] <= frame]
        result.append(f'{label} command context ({previous}, {frame}], last 12 of {len(selected)}:')
        for c in selected[-12:]:
            args = ','.join(':'.join((str(v[0]), *(f'{w:08X}' for w in v[2:2+v[1]]))) for v in c['args'])
            result.append(f"  seq={c['seq']} execution_frame={c['frame']} player={c['player']} {c['name']} args={args or '-'}")
        boundary = a['frames'][frame]['commands_before'] if label == 'A' else b['frames'][frame]['commands_before']
        start = match['commands'][0]['seq'] if match['commands'] else int(match['meta']['command_total'])
        prior = match['frames'].get(previous, {}).get('commands_before', 0)
        if start > prior or boundary > int(match['meta']['command_total']):
            result.append('  Earlier command context overwritten; do not infer absence of commands.')
    return '\n'.join(result)


def compare(a, b):
    for key in ('schema', 'math', 'map_crc', 'game_seed', 'crc_interval'):
        if a['meta'][key] != b['meta'][key]:
            raise ValueError(f'Incompatible {key}; pair the same match/settings')
    common = sorted(a['frames'].keys() & b['frames'].keys())
    if not common:
        raise ValueError('No shared generation checkpoints')
    prefix = (f'Retained generation frames {common[0]}..{common[-1]}; '
              'first retained difference is not necessarily the first divergent simulation tick.\n')
    for label, match in (('A', a), ('B', b)):
        truncated_objects = [f for f, data in match['frames'].items() if data['total'] > data['captured']]
        partial_commands = [c['seq'] for c in match['commands'] if c['argc'] > len(c['args']) or
                            c['name_cut'] or any(v[1] == 0 for v in c['args'])]
        if truncated_objects:
            prefix += f'{label}: object limit reached at frames {truncated_objects}; unrecorded objects cannot be localized.\n'
        if any(o['name_cut'] for f in match['frames'].values() for o in f['objects']):
            prefix += f'{label}: at least one object type name was truncated.\n'
        if partial_commands:
            prefix += f'{label}: incomplete argument/name coverage for {len(partial_commands)} commands.\n'
    cmd_a = {c['seq']: c for c in a['commands']}
    cmd_b = {c['seq']: c for c in b['commands']}
    cmd_diff = next((seq for seq in sorted(cmd_a.keys() & cmd_b.keys()) if cmd_a[seq] != cmd_b[seq]), None)
    previous = -1
    for frame in common:
        x, y = a['frames'][frame], b['frames'][frame]
        detail = object_difference(x, y)
        stage = next((stage for stage, u, v in zip(STAGES, (*x['stages'], x['crc']), (*y['stages'], y['crc'])) if u != v), None)
        rng = x['rng'] != y['rng'] or x['rng_crc'] != y['rng_crc']
        if detail or stage or rng or x['commands_before'] != y['commands_before']:
            report = prefix + f'First retained difference at generation frame {frame}: {stage or ("rng_state" if rng else "object/command observation")}.\n'
            if detail:
                report += detail + '\n'
            if rng:
                report += f"Logic RNG: A={x['rng']}, B={y['rng']} (six raw words).\n"
            if x['commands_before'] != y['commands_before']:
                report += f"Command counts before this CRC: A={x['commands_before']}, B={y['commands_before']}.\n"
            if cmd_diff is not None and min(cmd_a[cmd_diff]['frame'], cmd_b[cmd_diff]['frame']) <= frame:
                report += f'First retained unequal command sequence {cmd_diff}; execution frames A={cmd_a[cmd_diff]["frame"]}, B={cmd_b[cmd_diff]["frame"]}.\n'
            report += command_context(a, b, frame, previous)
            return 1, report + '\nRolling boundaries localize state differences; nearby commands do not prove causation.'
        previous = frame
    if cmd_diff is not None:
        return 1, prefix + f'First retained unequal command sequence {cmd_diff}: A={cmd_a[cmd_diff]}, B={cmd_b[cmd_diff]}.'
    incomplete = (a['frames'].keys() != b['frames'].keys() or cmd_a.keys() != cmd_b.keys() or
                  any(m['meta']['reason'] != 'match_end' for m in (a, b)) or
                  any(c['argc'] > len(c['args']) or c['name_cut'] or any(v[1] == 0 for v in c['args'])
                      for m in (a, b) for c in m['commands']) or
                  any(f['total'] > f['captured'] or any(o['name_cut'] for o in f['objects'])
                      for m in (a, b) for f in m['frames'].values()))
    if incomplete:
        return 2, prefix + 'Shared retained observations agree, but coverage is incomplete or the engine reported a mismatch. No synchronization verdict.'
    return 0, prefix + 'Retained observations agree. Older overwritten frames/commands and unsampled ticks are not checked; this is not match acceptance or Steam compatibility.'


def select(matches, index):
    if not matches or (index is None and len(matches) != 1):
        raise ValueError('Select one complete snapshot with --match-a/--match-b')
    index = 1 if index is None else index
    if index < 1 or index > len(matches):
        raise ValueError('Snapshot selection out of range')
    return matches[index - 1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log_a', type=Path)
    parser.add_argument('log_b', type=Path)
    parser.add_argument('--match-a', type=int)
    parser.add_argument('--match-b', type=int)
    args = parser.parse_args()
    try:
        a = select(parse(args.log_a.read_text(errors='strict').splitlines()), args.match_a)
        b = select(parse(args.log_b.read_text(errors='strict').splitlines()), args.match_b)
        code, report = compare(a, b)
        print(report)
        return code
    except (OSError, UnicodeError, KeyError, ValueError) as error:
        print(f'Inconclusive: {error}', file=sys.stderr)
        return 2


if __name__ == '__main__':
    sys.exit(main())
