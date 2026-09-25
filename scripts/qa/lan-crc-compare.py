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
    object_records: dict = field(default_factory=dict)
    object_summaries: dict = field(default_factory=dict)
    object_details: dict = field(default_factory=dict)


def parse(lines):
    matches = []
    for line in lines:
        marker = re.search(r"\[GX-LAN-CRC\] (begin|generated|object-summary|object-detail|object-field|object-transform-word|object) (.*)", line)
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
        elif marker[1] == "generated":
            if not matches:
                raise ValueError("Generation without match header; log may be truncated")
            frame = int(fields["frame"])
            if frame < 0 or frame in matches[-1].frames:
                raise ValueError("Invalid or duplicate generation frame")
            values = {key: int(fields[key], 16) for key in (*STAGES, "rng_seed_crc")}
            if any(value < 0 or value > 0xFFFFFFFF for value in values.values()):
                raise ValueError("CRC outside unsigned 32-bit range")
            matches[-1].frames[frame] = values
        elif marker[1] == "object-summary":
            if not matches:
                raise ValueError("Object summary without match header; log may be truncated")
            frame = int(fields["frame"])
            if frame < 0 or frame in matches[-1].object_summaries:
                raise ValueError("Invalid or duplicate object summary frame")
            summary = {key: int(fields[key]) for key in ("total", "captured", "truncated", "limit")}
            if (summary["total"] < 0 or summary["captured"] < 0 or summary["limit"] <= 0 or
                    summary["captured"] > summary["total"] or summary["captured"] > summary["limit"] or
                    summary["truncated"] not in (0, 1) or
                    summary["truncated"] != int(summary["total"] > summary["captured"])):
                raise ValueError("Invalid object summary")
            matches[-1].object_summaries[frame] = summary
        elif marker[1] == "object":
            if not matches:
                raise ValueError("Object record without match header; log may be truncated")
            frame = int(fields["frame"])
            order = int(fields["order"])
            object_id = int(fields["id"], 16)
            crc = int(fields["crc"], 16)
            records = matches[-1].object_records.setdefault(frame, [])
            if frame < 0 or order != len(records) or object_id < 0 or object_id > 0xFFFFFFFF or crc < 0 or crc > 0xFFFFFFFF:
                raise ValueError("Invalid or non-sequential object record")
            records.append((object_id, crc))
        elif marker[1] == "object-detail":
            if not matches:
                raise ValueError("Object detail without match header; log may be truncated")
            frame = int(fields["frame"])
            detail = {
                "order": int(fields["order"]),
                "id": int(fields["id"], 16),
                "template": fields["template"],
                "start_crc": int(fields["start_crc"], 16),
                "fields": [],
                "transform_words": [],
            }
            if (frame < 0 or frame in matches[-1].object_details or detail["order"] < 0 or
                    detail["id"] < 0 or detail["id"] > 0xFFFFFFFF or
                    detail["start_crc"] < 0 or detail["start_crc"] > 0xFFFFFFFF):
                raise ValueError("Invalid or duplicate object detail")
            matches[-1].object_details[frame] = detail
        elif marker[1] == "object-transform-word":
            if not matches:
                raise ValueError("Transform word without match header; log may be truncated")
            frame = int(fields["frame"])
            detail = matches[-1].object_details.get(frame)
            if detail is None:
                raise ValueError("Transform word has no detail header")
            order = int(fields["order"])
            object_id = int(fields["id"], 16)
            index = int(fields["index"])
            bits = int(fields["bits"], 16)
            if (order != detail["order"] or object_id != detail["id"] or
                    index != len(detail["transform_words"]) or bits < 0 or bits > 0xFFFFFFFF):
                raise ValueError("Invalid or non-sequential transform word")
            detail["transform_words"].append(bits)
        else:
            if not matches:
                raise ValueError("Object field without match header; log may be truncated")
            frame = int(fields["frame"])
            detail = matches[-1].object_details.get(frame)
            if detail is None:
                raise ValueError("Object field has no detail header")
            order = int(fields["order"])
            object_id = int(fields["id"], 16)
            crc = int(fields["crc"], 16)
            if (order != detail["order"] or object_id != detail["id"] or
                    not fields["field"] or crc < 0 or crc > 0xFFFFFFFF):
                raise ValueError("Invalid object field")
            detail["fields"].append((fields["field"], crc))
    for match in matches:
        for frame, summary in match.object_summaries.items():
            if frame not in match.frames:
                raise ValueError("Object summary has no generation checkpoint")
            if len(match.object_records.get(frame, ())) != summary["captured"]:
                raise ValueError("Object record count does not match summary")
        if any(frame not in match.object_summaries for frame in match.object_records):
            raise ValueError("Object records have no summary")
        for frame, detail in match.object_details.items():
            if frame not in match.frames:
                raise ValueError("Object detail has no generation checkpoint")
            records = match.object_records.get(frame, ())
            if (detail["order"] >= len(records) or records[detail["order"]][0] != detail["id"] or
                    not detail["fields"]):
                raise ValueError("Object detail does not match object records")
            if detail["transform_words"] and len(detail["transform_words"]) != 12:
                raise ValueError("Transform word trace is incomplete")
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


def describe_object_difference(a, b, frame):
    summary_a = a.object_summaries.get(frame)
    summary_b = b.object_summaries.get(frame)
    if summary_a is None or summary_b is None:
        return "Per-object observations are unavailable for one or both peers."
    records_a = a.object_records.get(frame, ())
    records_b = b.object_records.get(frame, ())
    for order, (left, right) in enumerate(zip(records_a, records_b)):
        if left[0] != right[0]:
            return (f"First per-object difference at order {order}: "
                    f"A id={left[0]:08X}, B id={right[0]:08X}; traversal order differs.")
        if left[1] != right[1]:
            previous = "start of object traversal" if order == 0 else f"equal through order {order - 1}"
            field_detail = describe_field_difference(a, b, frame, order, left[0])
            return (f"First per-object difference after id={left[0]:08X} at order {order}: "
                    f"A={left[1]:08X}, B={right[1]:08X}; {previous}.\n{field_detail}")
    if len(records_a) != len(records_b):
        order = min(len(records_a), len(records_b))
        next_a = f"{records_a[order][0]:08X}" if order < len(records_a) else "missing"
        next_b = f"{records_b[order][0]:08X}" if order < len(records_b) else "missing"
        return (f"First per-object coverage difference at order {order}: "
                f"A id={next_a}, B id={next_b}.")
    if summary_a["total"] != summary_b["total"]:
        return (f"Object totals differ after {len(records_a)} matching bounded records: "
                f"A={summary_a['total']}, B={summary_b['total']}.")
    if summary_a["truncated"] or summary_b["truncated"]:
        return (f"The first {len(records_a)} object records agree, but the bounded trace was truncated "
                f"(A total={summary_a['total']}, B total={summary_b['total']}).")
    return ("All complete per-object records agree despite the object-stage CRC difference; "
            "the trace is internally inconsistent and should be repeated.")


def describe_field_difference(a, b, frame, order, object_id):
    left = a.object_details.get(frame)
    right = b.object_details.get(frame)
    if left is None or right is None or left["order"] != order or right["order"] != order:
        return "Per-field observations are unavailable for this object on one or both peers."
    if left["id"] != object_id or right["id"] != object_id:
        return "Per-field observation IDs do not match the differing object."
    if left["template"] != right["template"]:
        return (f"Object template differs: A={left['template']}, B={right['template']}.")
    if left["start_crc"] != right["start_crc"]:
        return (f"CRC already differs before template {left['template']}: "
                f"A={left['start_crc']:08X}, B={right['start_crc']:08X}.")
    for index, (field_a, field_b) in enumerate(zip(left["fields"], right["fields"])):
        if field_a[0] != field_b[0]:
            return (f"Field trace order differs at index {index}: "
                    f"A={field_a[0]}, B={field_b[0]}.")
        if field_a[1] != field_b[1]:
            previous = "object start" if index == 0 else f"field {left['fields'][index - 1][0]}"
            matrix_detail = ""
            if field_a[0] == "transform":
                matrix_detail = describe_transform_difference(left, right)
            return (f"First field boundary difference in template {left['template']}: {field_a[0]} "
                    f"A={field_a[1]:08X}, B={field_b[1]:08X}; equal through {previous}.{matrix_detail}")
    if len(left["fields"]) != len(right["fields"]):
        return (f"Field trace coverage differs for template {left['template']}: "
                f"A={len(left['fields'])}, B={len(right['fields'])}.")
    return (f"All recorded field boundaries agree for template {left['template']}; "
            "a narrower probe is required.")


def describe_transform_difference(left, right):
    words_a = left["transform_words"]
    words_b = right["transform_words"]
    if not words_a or not words_b:
        return " Raw transform words are unavailable for one or both peers."
    labels = ("m00", "m01", "m02", "x", "m10", "m11", "m12", "y", "m20", "m21", "m22", "z")
    for index, (word_a, word_b) in enumerate(zip(words_a, words_b)):
        if word_a != word_b:
            label = labels[index] if index < len(labels) else f"word{index}"
            return (f" First raw transform difference: {label} (word {index}) "
                    f"A={word_a:08X}, B={word_b:08X}.")
    if len(words_a) != len(words_b):
        return f" Raw transform coverage differs: A={len(words_a)}, B={len(words_b)}."
    return " Raw transform words agree despite the transform CRC difference."


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
            object_detail = f"\n{describe_object_difference(a, b, frame)}" if stage == "objects" else ""
            return 1, (
                f"First observed difference at generation frame {frame}: {stage}\n"
                f"A={left[stage]:08X} B={right[stage]:08X}; "
                f"final CRC A={left['crc']:08X} B={right['crc']:08X}{object_detail}\n"
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
