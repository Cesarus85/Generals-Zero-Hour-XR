#!/usr/bin/env python3
# GeneralsX @test Codex 22/09/2026 Corrupt real emitter output to verify fail-closed parsing/comparison.
import copy
import importlib.util
from pathlib import Path
import sys
import unittest

spec = importlib.util.spec_from_file_location('compare', Path(__file__).with_name('lan-snapshot-compare.py'))
compare = importlib.util.module_from_spec(spec)
spec.loader.exec_module(compare)
root = Path(sys.argv.pop(1))


class SnapshotTests(unittest.TestCase):
    def setUp(self):
        self.text = (root / 'equal.log').read_text()
        self.a = compare.parse(self.text.splitlines())[0]

    def test_emitted_equal(self):
        self.assertEqual(compare.compare(self.a, copy.deepcopy(self.a))[0], 0)

    def test_building_health_with_commands(self):
        b = compare.parse((root / 'different.log').read_text().splitlines())[0]
        code, report = compare.compare(self.a, b)
        self.assertEqual(code, 1)
        for expected in ('frame 400', 'id=000000D3', 'Test Building', 'health', 'execution_frame=400', 'ability'):
            self.assertIn(expected, report)

    def test_late_ring(self):
        b = compare.parse((root / 'late.log').read_text().splitlines())[0]
        self.assertEqual(list(b['frames']), list(range(1300, 2100, 100)))
        self.assertEqual(b['meta']['generations'], '21')

    def test_late_field_difference_after_original_budget(self):
        b = compare.parse((root / 'late.log').read_text().splitlines())[0]
        a = copy.deepcopy(b)
        for frame in (1900, 2000):
            before = a['frames'][frame]['commands_before']
            a['frames'][frame] = copy.deepcopy(a['frames'][1800])
            a['frames'][frame]['commands_before'] = before
        code, report = compare.compare(a, b)
        self.assertEqual(code, 1)
        self.assertIn('frame 1900', report)
        self.assertIn('health', report)

    def test_overflow_is_inconclusive(self):
        b = compare.parse((root / 'bounds.log').read_text().splitlines())[0]
        self.assertEqual(len(b['commands']), 4096)
        self.assertEqual(b['commands'][0]['seq'], 17)
        self.assertEqual(b['frames'][2000]['total'], 2049)
        self.assertEqual(compare.compare(b, b)[0], 2)

    def test_truncated_dump(self):
        with self.assertRaises(ValueError):
            compare.parse(self.text.splitlines()[:-1])

    def test_missing_object(self):
        with self.assertRaises(ValueError):
            compare.parse([s for s in self.text.splitlines() if not ('object frame=200 order=2' in s)])

    def test_duplicate_object(self):
        lines = self.text.splitlines()
        i = next(i for i, s in enumerate(lines) if 'object frame=200 order=2' in s)
        lines.insert(i, lines[i])
        with self.assertRaises(ValueError):
            compare.parse(lines)

    def test_missing_field(self):
        with self.assertRaises(ValueError):
            compare.parse(self.text.replace('mask=7FF', 'mask=7FE', 1).splitlines())

    def test_invalid_transform(self):
        with self.assertRaises(ValueError):
            compare.parse(self.text.replace('transform_count=12', 'transform_count=11', 1).splitlines())

    def test_missing_command(self):
        with self.assertRaises(ValueError):
            compare.parse([s for s in self.text.splitlines() if 'command seq=2 ' not in s])

    def test_argument_padding_rejected(self):
        with self.assertRaises(ValueError):
            compare.parse(self.text.replace('args=0:1:00000000:00000000:', 'args=0:1:00000000:00000001:', 1).splitlines())

    def test_math_profiles_must_match(self):
        b = copy.deepcopy(self.a)
        b['meta']['math'] = 'gamemath' if self.a['meta']['math'] == 'native' else 'native'
        with self.assertRaises(ValueError):
            compare.compare(self.a, b)

    def test_missing_rng(self):
        with self.assertRaises(ValueError):
            compare.parse(self.text.replace('rng_present=1', 'rng_present=0', 1).splitlines())

    def test_map_pairing(self):
        b = copy.deepcopy(self.a)
        b['meta']['map_crc'] = '00000001'
        with self.assertRaises(ValueError):
            compare.compare(self.a, b)

    def test_rng_word_even_crc_collision(self):
        b = copy.deepcopy(self.a)
        b['frames'][100]['rng'] = (9, 2, 3, 4, 5, 6)
        code, report = compare.compare(self.a, b)
        self.assertEqual(code, 1)
        self.assertIn('rng_state', report)

    def test_command_argument(self):
        b = copy.deepcopy(self.a)
        b['commands'][2]['args'] = ((0, 1, 33, 0, 0, 0),)
        self.assertEqual(compare.compare(self.a, b)[0], 1)

    def test_equal_data_with_engine_mismatch_never_passes(self):
        b = copy.deepcopy(self.a)
        b['meta']['reason'] = 'different_crc'
        self.assertEqual(compare.compare(self.a, b)[0], 2)

    def test_frame_coverage(self):
        b = copy.deepcopy(self.a)
        del b['frames'][0]
        self.assertEqual(compare.compare(self.a, b)[0], 2)

    def test_multiple_matches_require_selection(self):
        matches = compare.parse((self.text + self.text).splitlines())
        with self.assertRaises(ValueError):
            compare.select(matches, None)
        self.assertEqual(compare.select(matches, 2), self.a)

    def test_global_stage(self):
        b = copy.deepcopy(self.a)
        stages = b['frames'][200]['stages']
        b['frames'][200]['stages'] = stages[:2] + (123, 456, 789)
        code, report = compare.compare(self.a, b)
        self.assertEqual(code, 1)
        self.assertIn('frame 200: partition', report)


unittest.main()
