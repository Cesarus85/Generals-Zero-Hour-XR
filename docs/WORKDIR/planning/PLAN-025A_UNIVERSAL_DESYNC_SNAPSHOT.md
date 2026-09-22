# PLAN-025A — bounded universal LAN desync snapshot

**State, 2026-09-22:** implemented and host/Android-compiled on the isolated
`codex/lan-object-crc-trace` branch, starting at `4fdda8a`. No new physical
match or simulation fix is claimed. Quest USB ADB has no connected device;
the recorded Omarchy WLAN and Tailscale SSH endpoints both time out. The
previously installed 10236 pair therefore remains historical, not freshly
verified device state. Public 1.2.28 and `main` are unchanged.

## Observer contract

`GXLanDesyncSnapshot.h` retains the last **eight normal production CRC
generations**, continuously throughout a live LAN match, rather than stopping
after the first eight. It allocates **7,144,064 bytes (about 6.8 MiB)** only
after explicit activation at LAN match start. No snapshot storage is allocated
for disabled diagnostics, campaign, Skirmish or replay. The same implementation
is used by both diagnostic peers.

Each generation retains up to **2048 objects**, in their existing production
traversal order, with stable ID, template name (95 bytes), starting/final rolling
CRC, all eleven existing object CRC boundaries and twelve raw matrix words.
The boundaries are private status, transform, ID, upgrades, experience, health,
weapon bonus, damage scalar and weapon slots 0–2. These are exactly the existing
`Object::crc` boundaries; this is not a complete dump of every behavior module
or an independent hash of each object. Template names are hex encoded so spaces
cannot break parsing. Truncated names/counts and absent field coverage are explicit.

The same record stores rolling stages after objects, RNG, partition, players
and AI, final CRC, RNG checksum and all **six live logic RNG words**. The new
RNG accessor only copies state; it never advances or reseeds any generator.

The command ring retains **4096 dispatched network game messages**, with
execution frame, global observation sequence, engine player index, type/name,
argument count and up to **32 typed arguments** per command. It observes the
existing `processCommandList` immediately before the dispatcher. Local UI
input/packet-arrival timing is not used as the execution frame. `MSG_LOGIC_CRC`
is excluded from gameplay command comparison because its payload is expected
to differ on a desync. Non-game transport controls are not included. Active
union members are serialized explicitly; no padding or pointer data is read.
Floats are recorded as raw 32-bit words, without decimal rounding. Each CRC
record also saves the number of preceding observed commands.

**No additional CRC traversal, CRC transfer, random draw, command injection,
network field, simulation rule or CRC interval is introduced.** Existing
`XferCRC` implementation and the earlier opt-in railroad math fix are unchanged.
The runtime observer has bounded CPU/memory overhead; it is diagnostic tooling,
not a claim of zero timing overhead.

## Activation and collection

Set `GX_LAN_SNAPSHOT=1`, or create `gx_lan_snapshot.txt` in the engine's actual
game-data working directory, before starting a LAN match. `GX_LAN_SNAPSHOT=0`
explicitly overrides the marker. Removing the marker and unsetting the variable
disables it for the next match. Snapshot mode suppresses the legacy
`GX_LAN_CRC` single-target/railroad logs even if their marker remains present.
The existing Setup checkbox still controls that **legacy** marker; it is not
the new snapshot switch.

The isolated PC launcher now sets `GX_LAN_SNAPSHOT=1`. It still runs only the
lab executable/data/profile and does not modify Steam or Proton.

For Quest, first run `adb devices -l`, choose the explicit serial, inspect the
installed package/build and confirm the engine's actual selected data directory
from the device configuration/log. Create the new marker there via that serial.
Do not guess a different import directory. Update-install only a candidate with
the installed release certificate and a higher versionCode; never uninstall or
clear data to bypass a signature mismatch.

Each serialized record is assembled in a bounded buffer and emitted with one
stdio call, preventing background log lines from interleaving inside a record.
There is no per-object/per-frame disk output. The engine emits one bounded
`[GX-LAN-SNAPSHOT] begin ... end` block to its existing stderr log at the first
detected missing/different CRC. An orderly game reset also dumps the retained
window with `reason=match_end`, if no failure dump was emitted. A forced process
kill/crash can lose the in-memory window. Finish an ordinary test through the
game UI and collect full logs promptly; Android's on-screen log preview may
truncate them. On a mismatch, collect both peers before another launch rotates
the files. A peer that has not yet detected failure can still emit its window
on an orderly reset. Nothing is requested or sent to the other peer by this tool.

```sh
python3 scripts/qa/lan-snapshot-compare.py quest.log pc.log
# For files containing several completed dumps:
python3 scripts/qa/lan-snapshot-compare.py quest.log pc.log --match-a 2 --match-b 2
```

The parser requires matching schema, math mode, map CRC, seed and CRC interval,
complete dump delimiters, ring/count consistency, sequential object/command
records, complete field masks/matrices/RNG and valid typed argument sizes.
It reports the first retained differing generation/stage/object/field, raw matrix
word where applicable, RNG state and each peer's recent command context. It also
compares dispatched command order, frames and arguments. Nearby commands are
context, **not proof of causation**. Build hashes, matching assets, factions and
the actual pairing must still be recorded separately.

Exit **1** means an observed difference. Exit **2** means incompatible/incomplete
input, including matching retained data despite an engine-reported mismatch.
Exit **0** means only that the retained observations agree. Ring overwrites,
object/argument/name limits, unsampled ticks and CRC collisions limit coverage;
no exit code certifies an entire match or Steam interoperability. At interval
100, eight checkpoints span 700 simulation frames. Late mismatches remain
observable without recording an unbounded match history.

## Reproducible host verification

```sh
bash scripts/qa/lan-snapshot-test.sh
bash scripts/qa/lan-crc-trace-test.sh
bash scripts/qa/lan-crc-detector-test.sh
python3 scripts/qa/lan-crc-compare-test.py
bash scripts/qa/lan-diagnostic-launcher-test.sh
bash scripts/qa/xr-workspace-test.sh
cmake --build build/android-vulkan --target z_generals -j 6
```

The new suite exercises the production emitter with UBSan: disabled/offline
gating, marker/explicit-OFF semantics, suppression of legacy probes, both ring
wraps, 2049 objects, excess arguments, all object IDs, freeze-once and reset.
The production command adapter is tested with the real argument declarations
and an asset-free message test double, including deliberately poisoned union
padding, typed coordinates, negative zero, unknown arguments and no message
mutation. Twenty-one Python fixtures cover paired field/global/RNG/command
differences, a mismatch after the original eight-checkpoint budget, corruption,
truncation, multi-match selection and fail-closed comparison.

ARM64 macOS and x86-64 under Rosetta pass the observer suite; paired synthetic
ARM64/x86-64 emitter logs agree. These tests use synthetic state, not a played
construction/production/combat match. Legacy observer/detector tests, 20 legacy
comparator fixtures and launcher tests pass. Workspace tests pass **788 checks
per LAN presentation gate**. Android ARM64 native linking passes.

Local packaging also passes for version **10237**,
`1.2.37-lan-universal-snapshot`, package `com.generalsx.zerohour.xr`.
This packaging regression artifact uses the repository **debug certificate**
(`644a3b0f...`), not the installed XR release certificate (`a3774568...`). It is
**not update-installable over 10236** and has not been installed. Preserve the
existing signed 10236 APK. A release-signed candidate plus matching native
Omarchy build remains required before the next physical test.

```text
APK SHA-256 b69ae3b84733d8e5db87d24d24be9a02d5c0db10faa9631735df1d5c998a7d75
libmain.so c34205b4b90e2ba8118edaf0f606b605d5a6322f0162e70f232a055015342393
```

APK v2 verification passes; the embedded native library equals the compiled
output byte for byte. No Linux link/deployment or physical match is claimed.

## Next paired scenario and acceptance

1. Reconnect Quest and Omarchy. Recheck serial/package/hash, remote source
   status, executable/hash, math mode, game data and the separate lab profile.
   Preserve the lab's existing Linux compatibility delta. Apply the snapshot
   patch with a checked diff; build/stage the native peer and release-sign the
   Quest candidate. Retain both 10236 artifacts for rollback.
2. Enable the new snapshot on both sides. Use the same stock map, no AI,
   fixed chosen factions/slots, normal rules and known matching retail assets.
3. In one representative scenario, both humans construct a building, produce
   units, move them, engage in combat and use an available unit/general ability.
   Record who performed each phase and collect the match replay if available.
4. If construction already desynchronizes, preserve the paired snapshot and
   mark later phases **not reached**. Diagnose the first field boundary and
   command difference; reproduce the identified arithmetic/state cause in a
   bounded regression before changing simulation code. Rebuild only for an
   evidenced correction, then repeat the same broader scenario. This avoids
   a build per target object/action; it cannot guarantee one test solves all
   independent determinism faults.
5. After all phases pass, sustain at least 15 minutes with both players issuing
   orders, and verify worn-headset controls/stability. Then repeat against the
   **unmodified Steam/Proton** executable and **Windows Steam** separately.
   Steam cannot produce this instrumented snapshot; same-source success is a
   necessary diagnostic milestone, not a substitute for those retail gates.
6. Identical-public-APK Quest-to-Quest remains a separate, untested path. It
   must not expand the current release's support claim without its own devices.

## Research leads, not applied fixes

The upstream [GeneralsX Beta 17 release](https://github.com/fbraz3/GeneralsX/releases/tag/GeneralsX-Beta-17)
explicitly requires compatible GeneralsX builds for deterministic cross-platform
LAN. It does not establish compatibility with an unchanged Steam executable.
[PR #217](https://github.com/fbraz3/GeneralsX/pull/217), merged as
`db399e54327c89f9a765b82afdb6a843850170ce`, addresses float/double wrappers,
NaN/Inf conversions and related deterministic arithmetic. Its `SqrtOrigin`
overloads differ from this branch's still-double wrapper. That is a candidate
to inspect if the new field/raw-state evidence points there, not evidence that
it caused object `000000D3` to diverge.

The upstream [desync guide](https://github.com/fbraz3/GeneralsX/blob/main/docs/HOWTO/INVESTIGATING_DESYNCS.md)
also describes FPU isolation, disabled FMA contraction and rolling deep-CRC
buffers. Our local `XferDeepCRC` remains the older file-oriented implementation;
the new observer instead reads the existing production traversal and names its
field boundaries without switching transfer modes or widening CRC contents.
Blindly adopting the whole upstream simulation delta or masking CRC low bits
would not demonstrate the required retail equivalence and is out of this slice.
Preserving one representative replay may later reduce repeated manual input,
but no replay-based network/headset acceptance is asserted here.
