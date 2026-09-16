#!/usr/bin/env python3
"""Compare recorded generation checkpoints from two instrumented LAN peers.

Usage: python3 scripts/qa/lan-crc-compare.py quest.log pc.log
Use --match-a / --match-b (one-based) when a log contains multiple matches.
Exit 0: recorded samples agree (not a multiplayer acceptance pass).
Exit 1: a recorded checkpoint differs. Exit 2: incomplete/incompatible input.
No game files are read or modified; no network connection is made.
"""
# GeneralsX @feature Codex 16/09/2026 Offline, explicitly paired CRC trace analysis.
import argparse
from dataclasses import dataclass, field
from pathlib import Path
import re
import sys

STAGES = ("objects", "rng", "partition", "players", "ai", "crc")


@dataclass
class Match:
    metadata: dict
    frames: dict = field(default_factory=dict)


def parse(lines):
    matches = []
    for line in lines:
        marker = re.search(r"\[GX-LAN-CRC\] (begin|generated) (.*)", line)
        if not marker:
            continue
        fields = dict(re.findall(r"(\w+)=([^\s]+)", marker[2]))
        if marker[1] == "begin":
            if fields.get("game") != "ZeroHour" or fields.get("mode") != "LAN":
                raise ValueError("Not a live Zero Hour LAN trace")
            metadata = {
                "map_crc": int(fields["map_crc"], 16),
                "game_seed": int(fields["game_seed"]),
                "crc_interval": int(fields["crc_interval"]),
            }
            if metadata["crc_interval"] <= 0:
                raise ValueError("Invalid CRC interval")
            matches.append(Match(metadata))
        else:
            if not matches:
                raise ValueError("Generation without match header; log may be truncated")
            frame = int(fields["frame"])
            if frame < 0 or frame in matches[-1].frames:
                raise ValueError("Invalid or duplicate generation frame")
            values = {key: int(fields[key], 16) for key in (*STAGES, "rng_seed_crc")}
            if any(value < 0 or value > 0xFFFFFFFF for value in values.values()):
                raise ValueError("CRC outside unsigned 32-bit range")
            matches[-1].frames[frame] = values
    return matches


def select(matches, number):
    if not matches:
        raise ValueError("No LAN match metadata found")
    if number is None:
        if len(matches) != 1:
            raise ValueError("Multiple matches found; select --match-a and/or --match-b")
        return matches[0]
    if number < 1 or number > len(matches):
        raise ValueError("Selected match number is out of range")
    return matches[number - 1]


def compare(a, b):
    if a.metadata != b.metadata:
        raise ValueError("Map CRC, seed or CRC interval differ; these traces are not comparable")
    common = sorted(a.frames.keys() & b.frames.keys())
    if not common:
        raise ValueError("No shared generation frames; validation frames cannot substitute")
    for frame in common:
        left, right = a.frames[frame], b.frames[frame]
        differing = [stage for stage in STAGES if left[stage] != right[stage]]
        if differing or left["rng_seed_crc"] != right["rng_seed_crc"]:
            stage = differing[0] if differing else "rng_seed_crc"
            return 1, (
                f"First observed difference at generation frame {frame}: {stage}\n"
                f"A={left[stage]:08X} B={right[stage]:08X}; "
                f"final CRC A={left['crc']:08X} B={right['crc']:08X}\n"
                "Stages are rolling CRC checkpoints, not independent subsystem hashes. "
                "This identifies a recorded difference, not its root cause or exact first tick."
            )
    if a.frames.keys() != b.frames.keys():
        return 2, (f"{len(common)} shared generation samples agree, but frame coverage differs. "
                   "Incomplete comparison; no synchronization verdict.")
    return 0, (f"All {len(common)} recorded generation samples agree. "
               "This is not proof of a full match or Steam compatibility. "
               "Confirm matching source builds, game data and test setup separately.")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("log_a", type=Path)
    parser.add_argument("log_b", type=Path)
    parser.add_argument("--match-a", type=int)
    parser.add_argument("--match-b", type=int)
    args = parser.parse_args()
    try:
        with args.log_a.open(errors="replace") as stream:
            a = select(parse(stream), args.match_a)
        with args.log_b.open(errors="replace") as stream:
            b = select(parse(stream), args.match_b)
        code, report = compare(a, b)
        print(report)
        return code
    except (OSError, KeyError, ValueError) as error:
        print(f"Inconclusive: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
