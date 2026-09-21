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
SUMMARY = "[GX-LAN-CRC] object-summary frame=100 total=2 captured=2 truncated=0 limit=2048\n"
OBJECT_A = "[GX-LAN-CRC] object frame=100 order=0 id=00000010 crc=00000011\n"
OBJECT_B = "[GX-LAN-CRC] object frame=100 order=1 id=00000020 crc=00000021\n"
DETAIL = "[GX-LAN-CRC] object-detail frame=100 order=0 id=00000010 template=TestObject start_crc=00000001\n"
FIELD_A = "[GX-LAN-CRC] object-field frame=100 order=0 id=00000010 field=private_status crc=00000002\n"
FIELD_B = "[GX-LAN-CRC] object-field frame=100 order=0 id=00000010 field=transform crc=00000011\n"
WORDS = "".join(
    f"[GX-LAN-CRC] object-transform-word frame=100 order=0 id=00000010 index={index} bits={bits:08X}\n"
    for index, bits in enumerate((0x3F800000, 0, 0, 0x41200000, 0, 0x3F800000, 0, 0x41A00000, 0, 0, 0x3F800000, 0))
)


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

    def test_first_object_crc_difference(self):
        left = self.sample(HEADER + FRAME + SUMMARY + OBJECT_A + OBJECT_B)
        right_text = (HEADER + FRAME.replace("objects=01", "objects=09") + SUMMARY + OBJECT_A +
                      OBJECT_B.replace("crc=00000021", "crc=00000029"))
        code, report = module.compare(left, self.sample(right_text))
        self.assertEqual(code, 1)
        self.assertIn("after id=00000020 at order 1", report)
        self.assertIn("equal through order 0", report)

    def test_object_order_difference(self):
        left = self.sample(HEADER + FRAME + SUMMARY + OBJECT_A + OBJECT_B)
        right_text = (HEADER + FRAME.replace("objects=01", "objects=09") + SUMMARY +
                      OBJECT_A.replace("id=00000010", "id=00000030") + OBJECT_B)
        code, report = module.compare(left, self.sample(right_text))
        self.assertEqual(code, 1)
        self.assertIn("order 0", report)
        self.assertIn("traversal order differs", report)

    def test_bounded_object_trace_reports_truncation(self):
        summary = SUMMARY.replace("total=2 captured=2 truncated=0", "total=3 captured=2 truncated=1")
        left = self.sample(HEADER + FRAME + summary + OBJECT_A + OBJECT_B)
        right_frame = FRAME.replace("objects=01", "objects=09")
        code, report = module.compare(left, self.sample(HEADER + right_frame + summary + OBJECT_A + OBJECT_B))
        self.assertEqual(code, 1)
        self.assertIn("bounded trace was truncated", report)

    def test_rejects_incomplete_object_trace(self):
        with self.assertRaises(ValueError):
            self.sample(HEADER + FRAME + SUMMARY + OBJECT_A)
        with self.assertRaises(ValueError):
            self.sample(HEADER + FRAME + OBJECT_A)

    def test_first_field_boundary_difference(self):
        left = self.sample(HEADER + DETAIL + FIELD_A + WORDS + FIELD_B +
                           FRAME + SUMMARY + OBJECT_A + OBJECT_B)
        right_words = WORDS.replace("index=7 bits=41A00000", "index=7 bits=41A00001")
        right_text = (HEADER + DETAIL + FIELD_A + right_words + FIELD_B.replace("crc=00000011", "crc=00000019") +
                      FRAME.replace("objects=01", "objects=09") + SUMMARY +
                      OBJECT_A.replace("crc=00000011", "crc=00000019") + OBJECT_B)
        code, report = module.compare(left, self.sample(right_text))
        self.assertEqual(code, 1)
        self.assertIn("template TestObject: transform", report)
        self.assertIn("equal through field private_status", report)
        self.assertIn("y (word 7) A=41A00000, B=41A00001", report)

    def test_rejects_field_without_detail_header(self):
        with self.assertRaises(ValueError):
            self.sample(HEADER + FIELD_A + FRAME + SUMMARY + OBJECT_A + OBJECT_B)

    def test_rejects_detail_without_fields(self):
        with self.assertRaises(ValueError):
            self.sample(HEADER + DETAIL + FRAME + SUMMARY + OBJECT_A + OBJECT_B)

    def test_rejects_transform_word_without_detail(self):
        with self.assertRaises(ValueError):
            self.sample(HEADER + WORDS + FRAME + SUMMARY + OBJECT_A + OBJECT_B)

    def test_rejects_non_sequential_transform_word(self):
        with self.assertRaises(ValueError):
            self.sample(HEADER + DETAIL + WORDS.replace("index=1", "index=2", 1) + FIELD_A +
                        FRAME + SUMMARY + OBJECT_A + OBJECT_B)

    def test_rejects_incomplete_transform_words(self):
        with self.assertRaises(ValueError):
            self.sample(HEADER + DETAIL + WORDS.rsplit("\n", 2)[0] + "\n" + FIELD_A +
                        FRAME + SUMMARY + OBJECT_A + OBJECT_B)


if __name__ == "__main__":
    unittest.main()
