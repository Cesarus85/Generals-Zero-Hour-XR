# PLAN-025B — multiplayer harness handoff

**Prepared:** 2026-09-24. This is an experimental research handoff, not a merge
or release request. No device availability, installation or new match was
checked or performed while preparing it.

## Desired outcome and exact starting point

Make **Generals: Zero Hour XR on Quest play a sustained synchronized match
against the unmodified Steam PC version of Zero Hour**. Both Steam/Proton and
Windows Steam are explicit acceptance gates. A native PC build from our source
is an instrumented comparison peer, not a replacement for the retail goal.
Quest-to-Quest is a separate promising, untested path.

- Private repository: `Cesarus85/Generals-Zero-Hour-XR`.
- Handoff branch: `codex/lan-object-crc-trace`.
- Latest implementation checkpoint: **`078f8eaef8dbd24126699e1b8994dfe059b65e5e`**.
  The handoff PR adds documentation on top; start from its head and record the
  exact commit before making changes.
- Earlier checkpoint `4fdda8a` records the pause/strategy change; the universal
  observer was added in `67ef17c` and hardened in `078f8ea`.
- Use a fresh clone/worktree and your own continuation branch from this PR's
  head. Do not depend on a surviving temporary directory or the main workspace's
  uncommitted files. Do not start from current `main`, rebase onto it or merge it
  into the diagnostic baseline as an incidental setup step.
- On 2026-09-24, remote `main` is
  `286d6c2bc3ce590d2f59f60d4dec747c666393f2`. Before this documentation update,
  the histories had 75 main-only and 24 LAN-only commits. The full PR includes
  the earlier LAN slice, Linux compatibility work, input/diagnostic UI, math
  correction and universal observer; it is broader than the last two commits.

**Release drift:** `v1.2.28-xr-preview` was the protected public baseline when
this investigation began. GitHub now lists **`v1.2.33-xr-preview`**, published
2026-09-23, as latest. Preserve both the historical baseline and the current
public release. Do not publish or enable experimental LAN in either. Private
diagnostic version names such as 10233 are not the public release with a similar
number: identify artifacts by branch/commit, version, certificate and hash.

The local owner can provide the main checkout and prior isolated lab locations
if needed; a different machine only needs repository access to start source
review. Game data, signing credentials, device access and private raw captures
are **not** supplied by a source clone. Do not commit or attach them to the PR.

## Read order

1. Repository `AGENTS.md` and its applicable instruction files.
2. This handoff and [XR_CURRENT_STATUS.md](XR_CURRENT_STATUS.md), paying attention
   to dates rather than treating historical device observations as current.
3. [PLAN-025_QUEST_PC_LAN_PREFLIGHT.md](PLAN-025_QUEST_PC_LAN_PREFLIGHT.md):
   earlier transport, content and paired-match evidence.
4. [PLAN-025A_UNIVERSAL_DESYNC_SNAPSHOT.md](PLAN-025A_UNIVERSAL_DESYNC_SNAPSHOT.md):
   implemented capture contract, limits, test commands, artifact hashes,
   activation, research links and physical scenario.
5. The newest relevant entries in
   [2026-09-DIARY.md](../../DEV_BLOG/2026-09-DIARY.md).
6. Actual code and diff. Documentation is not evidence that a pending gate passed.

## Confirmed evidence versus open questions

| Evidence | What it establishes / does not establish |
|---|---|
| Direct Connect reached Steam/Proton lobby and gameplay, then mismatched | Initial reachability/map exchange works; no sustained retail synchronization |
| Matching inspected retail archives/scripts | Those files agree; not a proof of all effective assets/overrides or identical simulation |
| Quest/native Omarchy train investigation | Same raw `atan2` inputs produced Android `BF5788E6` versus glibc `BF5788E7`; the difference propagated into a train transform |
| Opt-in GameMath pair, private 10235 idle test | All eight captured CRC points agreed; the demonstrated train fault was removed for this pair, not proven retail compatible |
| 10235 construction test | Equal through frame 300; first recorded difference at frame 400 in existing object `000000D3`, order 14; object count/order and RNG checksum agreed |
| Retargeted 10236 pair | Historically installed/staged, but the user deliberately abandoned its single-object test; do not resume that strategy |
| Universal snapshot in 078f8ea | Implemented and software-tested, **not yet validated in a paired physical match**; no additional gameplay desync fix |
| 10237 QA APK | Native build/package/v2 checks passed; debug certificate differs from XR release signing, so this artifact cannot update-install over the recorded 10236 release certificate |
| Last connectivity check, 2026-09-22 | No Quest via ADB; Omarchy SSH timed out; this is historical, not today's device state |

The current construction divergence has **no demonstrated field-level cause**.
Do not infer a faulty building, RNG, packet, math function or platform from
object ID `000000D3` alone. Synthetic test output naming `Test Building/health`
at frame 400 is a comparator fixture, not the physical game's result.

## Implemented diagnostic and its limits

- `Core/Libraries/Include/GXLanDesyncSnapshot.h`: opt-in bounded ring, last eight
  normal CRC generations throughout the match, 2048 object records per sample,
  existing field boundaries/type names/raw matrices, global stages and six RNG
  words; 4096 dispatched commands, up to 32 typed arguments each.
- `GeneralsMD/Code/GameEngine/Include/Common/LanSnapshotCommand.h`: observes
  network gameplay commands immediately before dispatch, with execution frame;
  excludes CRC messages from gameplay-command comparison.
- `scripts/qa/lan-snapshot-compare.py`: strict paired parser/comparator, explicit
  coverage failures, first retained difference and command context.
- Enable `GX_LAN_SNAPSHOT=1` or the `gx_lan_snapshot.txt` marker in the actual
  engine working directory. Explicit `GX_LAN_SNAPSHOT=0` overrides the marker.
  The existing Setup checkbox alone still enables the **legacy** observer.
- Dump once on mismatch or orderly match reset. A forced kill can lose the
  in-memory window. Capture both full logs before rotations overwrite them.
- CRC fields are cumulative boundaries, not independent field hashes; eight
  samples do not expose the exact first divergent tick or all older state.
  Exit 0 means retained evidence agrees, never full-match/Steam acceptance.
- This diagnostic adds no production CRC input/traversal, random draw, packet
  field, gameplay rule or CRC interval change. Keep that property during review.

Recorded software validation: 21 new comparator fixtures, UBSan observer and
command-adapter tests on ARM64 and x86-64/Rosetta, matching synthetic emitter
logs, existing detector/trace/20 comparator fixtures, launcher tests and 788
workspace checks per LAN gate. Android native linking and QA packaging passed.
These checks were performed on 2026-09-22; rerun relevant checks after changes.

## First assignment to the receiving harness

1. Verify the checkpoint and review the observer/comparator for missed state,
   misleading comparisons, reset/overflow behavior and unintended simulation
   effects. Report concrete gaps; do not rewrite it or assume its correctness
   merely because the tests pass.
2. Check actual device availability only when the owner announces readiness.
   Use `adb devices -l` and an explicit serial. Verify installed version,
   certificate/hash and the remote lab's source/diff/executable before building.
   Preserve the native Omarchy lab's required Linux compatibility delta.
3. Build/stage matching diagnostic source for Quest and native PC, using the
   recorded LAN presentation and deterministic-math settings. Obtain the proper
   release signing through the owner/local secure setup; use a versionCode above
   the actually installed package, not a blindly hardcoded historical value.
   Keep rollback artifacts and all imported data. Do not uninstall to bypass a
   signing conflict or change original Steam/Proton files/profile.
4. Enable the universal snapshot on both ends. With the owner operating the
   headset, run a fixed stock-map/no-AI scenario covering construction,
   production, movement, combat and an ability. If it mismatches early, record
   later phases as not reached. Preserve both logs and replay if available.
5. Compare the complete paired captures. Establish the first observed field
   and command context, then a reproducible underlying cause. Only then make
   a focused correction and regression test. Repeat the same broader scenario;
   do not return to one diagnostic build per object ID or action.
6. After that passes, complete sustained Quest/native play, at least 15 minutes
   with both humans issuing orders, then unmodified Steam/Proton and Windows
   Steam gates. Keep worn-headset behavior separate from automated evidence.
7. Update status, PLAN-025/025A and the journal for material changes. Return
   source commits, build settings/hashes, tests, actual device evidence, first
   differing frame/object/field and every still-open gate.

Upstream Math/FPU corrections and a replay-driven reproduction are research
leads already linked in PLAN-025A. The upstream requires compatible port builds;
blanket adoption does not prove the required unchanged-Steam behavior. An
identical-APK Quest-to-Quest test can be pursued separately, without changing the
primary target or implying current multiplayer support.

**Never merge this handoff into `main`, publish it as supported multiplayer,
replace a public release, disable mismatch checking, mask CRC differences, or
claim a real match from synthetic fixtures.** Implementation, installation,
physical acceptance and retail compatibility must remain separate facts.
