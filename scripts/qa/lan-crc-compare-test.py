#!/usr/bin/env python3
"""Synthetic parser/comparison regressions; does not validate game determinism."""
# GeneralsX @feature Codex 16/09/2026 Safe paired trace comparison tests.
import importlib.util
from pathlib import Path
import sys
import unittest

spec = importlib.util.spec_from_file_location("lan_crc_compare", Path(__file__).with_name("lan-crc-compare.py"))
module = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = module
spec.loader.exec_module(module)

HEADER = "[GX-LAN-CRC] begin game=ZeroHour mode=LAN map_crc=ABCDEF00 game_seed=42 crc_interval=100\n"
FRAME = "[GX-LAN-CRC] generated frame=100 local_slot=0 objects=01 rng=02 partition=03 players=04 ai=05 crc=05 rng_seed_crc=06\n"


class CompareTest(unittest.TestCase):
    def sample(self, text=HEADER + FRAME):
        return module.select(module.parse(text.splitlines()), None)

    def test_equal(self):
        code, report = module.compare(self.sample(), self.sample((HEADER + FRAME).replace("local_slot=0", "local_slot=1")))
        self.assertEqual(code, 0)
        self.assertIn("not proof", report)

    def test_rolling_difference(self):
        code, report = module.compare(self.sample(), self.sample((HEADER + FRAME).replace("partition=03", "partition=07")))
        self.assertEqual(code, 1)
        self.assertIn("100: partition", report)

    def test_metadata_mismatch(self):
        for field, replacement in (("game_seed=42", "game_seed=43"), ("crc_interval=100", "crc_interval=50"), ("ABCDEF00", "00000000")):
            with self.assertRaises(ValueError):
                module.compare(self.sample(), self.sample((HEADER + FRAME).replace(field, replacement)))

    def test_truncated_header_and_empty(self):
        for text in (FRAME, "", "unrelated game log"):
            with self.assertRaises(ValueError):
                self.sample(text)

    def test_multiple_matches_require_selection(self):
        matches = module.parse(((HEADER + FRAME) * 2).splitlines())
        with self.assertRaises(ValueError):
            module.select(matches, None)
        self.assertEqual(module.select(matches, 2).metadata["game_seed"], 42)
        with self.assertRaises(ValueError):
            module.select(matches, 0)

    def test_missing_frames(self):
        b = self.sample(HEADER + FRAME + FRAME.replace("frame=100", "frame=200"))
        self.assertEqual(module.compare(self.sample(), b)[0], 2)

    def test_duplicate_and_malformed(self):
        for text in (HEADER + FRAME * 2, (HEADER + FRAME).replace("crc=05", "crc=XYZ"), (HEADER + FRAME).replace("crc=05", "crc=100000000")):
            with self.assertRaises(ValueError):
                self.sample(text)

    def test_no_common_frame(self):
        with self.assertRaises(ValueError):
            module.compare(self.sample(), self.sample((HEADER + FRAME).replace("frame=100", "frame=200")))

    def test_ignores_validation_frame(self):
        b = self.sample(HEADER + "[GX-LAN-CRC] checkpoint validation_frame=100 reason=none\n")
        with self.assertRaises(ValueError):
            module.compare(self.sample(), b)

    def test_rng_only(self):
        code, report = module.compare(self.sample(), self.sample((HEADER + FRAME).replace("rng_seed_crc=06", "rng_seed_crc=07")))
        self.assertEqual(code, 1)
        self.assertIn("rng_seed_crc", report)


if __name__ == "__main__":
    unittest.main()
