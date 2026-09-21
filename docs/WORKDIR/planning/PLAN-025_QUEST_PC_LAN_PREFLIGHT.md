# PLAN-025 — Quest ↔ PC LAN preflight

**Status (resumed 2026-09-21):** Quest APK 10231 and the isolated same-source
native Omarchy build now include bounded per-object observations in the existing
CRC traversal. The preceding paired match detected a real CRC mismatch at
validation frame 105: generation agreed completely at frame 0 and first differed
in the object-list checkpoint by frame 100. The next fixed-map, no-AI idle match
will identify the first differing object or traversal order. Earlier Steam/Proton
matches also desynchronized. LAN remains experimental and unsupported; public
offline release 1.2.28 is separate and has not been replaced on GitHub.
**Scope:** One Quest 3 against a PC on the same LAN. The first peer is the user's Steam Zero Hour running through Proton on Omarchy; the planned Windows Steam peer remains a separate validation. If retail gameplay desynchronizes, isolate it with a same-source GeneralsX PC build. Internet services, public matchmaking, replay and reconnect are later gates.

**User priority:** Quest versus the unmodified Steam PC game is the primary
compatibility goal; Quest versus Quest remains a target. A same-source PC
executable is a diagnostic comparator, not an implicit replacement for Steam
support. Do not relax CRC checks or claim retail support from same-source tests.

## Current handoff

1. Keep branch `codex/lan-object-crc-trace` separate from release `main`.
   The unpublished Quest diagnostic is versionCode 10231
   (`1.2.31-lan-object-trace`), SHA-256
   `41b24487ef54540d6890f34285d72752adc8b9357657eca24863f989caddcea2`;
   it is update-installed on Quest `2G0YC5ZG9609PY` with app data retained. The
   native Omarchy lab is
   `/home/stefan/generals-xr-lan-diagnostics-78207e6`, with its own user data,
   copied legitimate game files and executable SHA-256
   `a403f82f9ddf7e8a5e9de6bb3b05699c2098df5736ad886299139f57e01bf48f`.
   The previous PC executable remains recoverable as
   `runtime/GeneralsXZH.pre-object-trace`.
   Original Steam/Proton files and settings were not modified.
2. Preserve the paired logs already collected privately. The match used map
   CRC `DEA9E8E4`, seed `4042777` and CRC interval 100. Both sides generated
   `3263A8D7` at frame 0. At frame 100, the Quest's object checkpoint is
   `6AE75FBB` and the PC's `03972538`; the RNG seed checksum agrees at
   `A82FF014`. At validation frame 105, both sides received Quest CRC
   `E2E3DF5F` and PC CRC `A6E913D4` and classified `different_crc`.
   `scripts/qa/lan-crc-compare.py` reports the first observed difference as
   `objects` at generation frame 100. No missing CRC packet is implicated in
   this particular failure. These rolling checkpoints cannot identify an
   individual object or prove floating-point math is the cause.
3. The bounded observer is implemented in the existing
   `GameLogic::getCRC` traversal. It records order, stable object ID and the
   rolling production CRC immediately after each object snapshot, followed by
   total/captured/truncation metadata. It captures at most 2048 records for each
   of the existing first eight checkpoints. It adds no traversal, CRC write,
   random draw, network field or cadence change. The comparator validates trace
   completeness and reports the first unequal CRC, ID/order or coverage point.
4. Repeat one fixed-map, fixed-faction, no-AI Quest-hosted match with no early
   player orders. Compare the first unequal object (or missing/differently
   ordered ID). Only then add a narrow field/timing probe if needed. Correct a
   demonstrated cause, repeat same-source idle and interactive matches, then
   retest unmodified Steam/Proton and Windows Steam. A clean 15-minute match
   with orders from both humans and headset checks is the minimum LAN gate.
5. Do **not** publish or merge the diagnostic APK as the offline release, turn
   off mismatch detection, or claim that a same-source match proves retail
   compatibility. Automatic LAN discovery is also still open; Direct Connect
   works. Omarchy's earlier `100.123.209.83` was Tailscale, and the isolated
   profile now pins LAN and Online IP to WLAN `192.168.178.158`.

## Decision and evidence

- The Quest port contains the native LAN lobby, direct-IP join, game-start transport and synchronized command path. LAN uses UDP 8086 for lobby traffic and 8088 for gameplay in the inspected source.
- `RETAIL_COMPATIBLE_NETWORKING` and `RETAIL_COMPATIBLE_CRC` default to 1. This makes retail interoperability a worthwhile **test**, not a verified feature. Source changes, platform timing, installed game data, maps or mods may still diverge.
- Zero Hour's LAN join CRC rejection is currently compiled out (`#if !RTS_ZEROHOUR`). Therefore lobby success alone is weak evidence: only a sustained match without CRC mismatch or desynchronization can pass the compatibility gate. Do not remove or alter that legacy check speculatively.
- The accepted Quest release deliberately allows tabletop stereo only for offline Skirmish and campaign. The new `GX_XR_LAN_PREVIEW` Android CMake option defaults OFF and admits `GAME_LAN` only when explicitly enabled. It is a **presentation-only** experiment; it changes no game messages, rules or wire format. Internet and replay stay excluded.

## First physical test: Steam Zero Hour on PC

1. Record the exact Quest APK/source commit and Steam Zero Hour version. Use matching unmodified Zero Hour game data and a stock multiplayer map; disable gameplay-changing mods on both ends. Do not copy retail assets into the source repository.
2. Connect Quest and PC to the same ordinary IPv4 LAN. Disable guest Wi-Fi/client isolation. Permit the game through the PC firewall, including UDP 8086 and 8088. Record each endpoint's IPv4 address. LAN broadcast discovery is tested first; direct IP is the fallback, not proof that discovery works.
3. Open the native LAN lobby in the Quest's upright shell. Verify controller pointing, text entry where needed, host/join, lobby selection and chat. Test PC-host/Quest-join first, then reverse the roles if possible.
4. Start a two-human match. Verify Quest tabletop placement, build window and commands; select, build, move, attack and use one control group on both sides. Play at least 15 minutes with visible interaction, compare synchronized events, watch for CRC mismatch/stall and measure Quest frame pacing. Then leave and rejoin safely.
5. Repeat one deliberately mismatched-map/content case only after the clean match; capture its observable result without modifying the networking rules to force a join.

If Steam discovery/join or in-match synchronization fails, reproduce with a PC GeneralsX executable from the **same source commit and game data**. This separates retail compatibility from Quest networking/XR defects. Do not describe the Steam version as supported until the clean Steam match passes. A same-source pass is not evidence that retail Steam works.

## Acceptance gates

| Gate | Evidence needed | State |
|---|---|---|
| Build safety | Default-off and preview-on mode tests; Android native/XR APK build | 10231 Android native/XR package, focused observer/comparator/detector tests and 788 workspace checks per LAN-gate setting pass; installed APK hash verified |
| Lobby | Discovery and direct-IP outcomes, both endpoint IPs, host/join/leave, chat/input | Direct Connect reaches lobby and starts match; automatic discovery still fails |
| Simulation | 15-minute Quest ↔ PC human match, orders from both players, no desync/CRC/stall | Fails against Steam/Proton and the same-source native Omarchy comparator; first observed object CRC difference by frame 100 |
| XR usability | Tabletop and upright shell transitions, controller menu/text entry, headset pause/resume and performance | Open; controller-operated Direct Connect keyboard built in 10212, worn-headset test pending |
| Retail compatibility | Above gates against the user's unmodified Steam Zero Hour | Open |
| Regression | Offline Skirmish/campaign retain accepted tabletop behavior | Host eligibility test passes; device regression open |

The local, **not published**, opt-in APK is
`build/apk/Generals-Zero-Hour-XR.apk`, package
`com.generalsx.zerohour.xr`, version 10210 (`1.2.10-lan-preflight`), SHA-256
`5cc4f84a04eb7b44bab906f8acff3414b1e5d9b808fafc04b557d8fcfb374a4b`.
The native ARM64 target and XR APK build passed; v2 signing and staged native
library equality were verified. Its source is the unmerged
`codex/quest-pc-lan-preflight` branch. It was installed in place on Quest 3
`2G0YC5ZG9609PY` using `adb install -r`, without clearing app data. The APK
read back from the device has the same SHA-256. The user reached the LAN lobby;
tabletop human gameplay and broader headset UX remain untested. Preserve the
prior release APK for rollback.

First user feedback after installation: both machines run explicitly labelled
Zero Hour and share the same Wi-Fi, but neither LAN lobby discovers the other.
Their animated shell backgrounds differ; that does not identify the network
failure or prove an SKU mismatch. Quest has only one active non-loopback IPv4
interface (`wlan0`), and no fixed `IPAddress` entry was found in its options.
The diagnostic next test was native Direct Connect to the peer's IPv4 address.
Avoid changing the simulation or relaxing compatibility checks on discovery
failure alone.
The user subsequently tried an Omarchy laptop at `192.168.178.158` as an
alternate peer, running Steam Zero Hour through Proton (the Windows game,
not a native GeneralsX build). The Quest routes to it via `wlan0` and
received all three ICMP replies (0% loss). This rules out total IP/subnet
isolation for that peer, not a UDP firewall, LAN socket or game protocol
failure. Direct Connect subsequently reached the shared game lobby, proving
that at least the initial Quest ↔ Steam/Proton LAN exchange works. The Omarchy
side then reported that it did not have the selected map. The latest Quest
`Network.ini` names `Maps\\Alpine Assault\\Alpine Assault.map`; the Quest
hosted, and the user believes this was the failed lobby map. SHA-256 of the
Quest and Omarchy Steam/Proton `MapsZH.big` and base Generals `maps.big`
archives matches on both systems. This rules out different bytes in those
archives, not a local map override, map-cache discrepancy or LAN map-path/CRC
mismatch. `hasMap`
is false both when the map is absent from `MapCache` and when its file CRC
differs, so do not treat the warning as proof that the file is missing. Check
the exact lobby map, effective map metadata/CRC and serialized map path before
changing transfer rules or CRC checks. The Quest also needs an XR controller
text-entry path for Direct Connect IP fields; for now the Quest-host direction
avoids typing on the headset.

The source inspection identified a concrete retail-compatibility candidate:
`GameInfoToAsciiString` percent-encoded spaces in every map directory, including
LAN announcements and game options. A stock `Alpine Assault` path therefore
sent `Alpine%20Assault` to the Steam peer. LAN call sites now serialize the
literal legacy directory while replay/save callers retain percent encoding;
the parser still accepts both. This explanation is strongly consistent with
the warning but remains unconfirmed until a Steam/Proton lobby retest.

The same candidate adds a controller-operated XR keyboard for the Direct
Connect player-name and remote-IPv4 fields. The keyboard is a native spatial
panel, not an Android 2D IME; numeric/IP and alphabetic layouts share their
paint/hit geometry. Host regressions pass: 3848 menu geometry/capture, 329
menu action/routing, and 22429 bilingual panel-payload checks (including both
keyboard layouts).
The Android ARM64 native build and XR APK package pass. Test version 10212
(`1.2.12-lan-keyboard`), SHA-256
`e8f05cc2c77a3d125117ed7a836e36fdcd159f59d4807d37a89006954fd166fc`,
is at `build/apk/Generals-Zero-Hour-XR.apk` and was installed on Quest 3
`2G0YC5ZG9609PY` with `adb install -r` (data retained). APK v2 signature,
package/version and bundled native-library hash were verified. This is a
test candidate only. The user confirms Direct Connect and map start with
Steam/Proton, but Omarchy then shows: "Game has detected a mismatch. This means
the multiplayer game has lost synchronization data between the players."
The photographed dialog appears over the loaded map and base, so the lobby/map
availability problem is no longer the immediate blocker. The game's CRC
comparison and mismatch handling are implemented in `GameLogic.cpp` and
`Network.cpp`; the message is a simulation desynchronization, not evidence of
an ordinary socket timeout. The Quest log confirms world/tabletop
presentation and contains no explicit local CRC-mismatch event in the inspected
window. Compare gameplay-critical INI/script assets (matching map archives alone
are insufficient), then run a same-source PC peer to separate retail engine
incompatibility from Quest LAN/XR behavior. Do not disable CRC checks or claim a
successful human match.

The Quest files were rehashed after the photo. In the Zero Hour installation
directory on Omarchy, compare `sha256sum INIZH.big PatchINI.big PatchZH.big
PatchData.big Data/Scripts/MultiplayerScripts.scb
Data/Scripts/SkirmishScripts.scb`; also compare base Generals `INI.big` and
`Patch.big`. Quest SHA-256 values:

| File | Quest SHA-256 |
|---|---|
| `INIZH.big` | `1a6d41a7a2cb31e67ad2f868aca9264ad069c275e0074f8a0d970a336071e9a0` |
| `PatchINI.big` | `16028d315c8c4d279beed15f1836a8998ff5c9a4d0d621d4a3d37213a0d9fe62` |
| `PatchZH.big` | `450276fbabd19f79dc0143f70fe755e44a99b22c552bc00c5854fc810e615722` |
| `PatchData.big` | `90952433efe55a774ed3f8375b7700b0a16c8206a760b5cdb3d8707a0e66fc0b` |
| `Data/Scripts/MultiplayerScripts.scb` | `86d6bd295dd56dc17c6c1289f9a530506c755b0dbc3e868448d93dd468738ab6` |
| `Data/Scripts/SkirmishScripts.scb` | `8f93862b751f289b052206b87170cc840044cb66660fbf6ae30d5782c1d73776` |
| Base `INI.big` | `bff8d621088b25fd8b041c8acca020a020fabc66f972ab2bd131fc67d905a72c` |
| Base `Patch.big` | `28dc194412f96dc1f66412430cf74f2d89ad0cdabf70d2c8d1179d8e51743494` |

The user supplied Omarchy hashes for the first six Zero Hour entries; all six
match Quest byte for byte. This rules out differences in those specific
archives/scripts, but not base Generals `INI.big`/`Patch.big`, loose overrides,
other assets, or simulation differences between retail and the port. Compare
the two base archives and check for loose gameplay overrides. Then test Quest
against native Linux GeneralsX built from Quest APK source commit `cfdbc9f`;
use the private XR fork, **not** the upstream clone shown in the generic Linux
build guide. If that match stays synchronized, the Steam/Proton cross-build
compatibility path is the issue. If it also desynchronizes, investigate
Quest/ARM versus native x86-64 determinism and game-state initialization.
An immediate first-CRC failure points to a different initial simulation state
or platform/engine determinism; it does not identify which subsystem without
further instrumentation.

## Post-reinstall desynchronization audit

The user confirmed a successful P23 reinstall. APK 10212 was subsequently
update-installed on Quest `2G0YC5ZG9609PY`; version and installed APK SHA-256
matched the candidate above. The same Omarchy mismatch returned. The latest
Quest stderr log (2026-09-16 15:11 local time) contains Direct Connect and
tabletop rendering, but no detailed synchronization failure record. The release
compile commands define `RTS_RELEASE`/`NDEBUG`; the detailed CRC comparison logs
are guarded by `DEBUG_LOGGING`. No first divergent frame/subsystem is known.

### Ranked candidates and concrete evidence

1. **Cross-build numerical differences: strong candidate, not a proven cause.**
   `build/android-vulkan/CMakeCache.txt` has
   `SAGE_USE_DETERMINISTIC_MATH=OFF`; the actual GameLogic compile command uses
   ARM64 Clang and `-ffp-contract=off`. FMA contraction is already disabled,
   so enabling that flag again is not a new fix. `setFPMode()` in
   `GameLogic.cpp` sets round-to-nearest everywhere and x87 precision only on
   x86; it does not emulate x87 operation results on ARM. `Object::crc()` hashes
   the transform matrix and health as raw values, so tiny numerical differences
   can change a CRC before visibly different gameplay appears.
2. **The advertised deterministic-math integration is incomplete.**
   `wwmath.h` still contains TODO GameMath calls, and both preprocessor branches
   call native math. `Trig.cpp` also creates tables with native `sin`/`acos`.
   Merely switching the CMake option ON cannot be called a determinism fix.
   These files are unchanged since the initial XR source snapshot `b99838b`.
   The referenced [upstream PR #2670](https://github.com/TheSuperHackers/GeneralsGameCode/pull/2670)
   is still open when checked; its author reports distinct legacy-x87 and
   modern deterministic math benchmark results and explicitly questions
   whether available replay tests contain CRC messages. Treat it as research,
   not a proven retail-compatible drop-in patch.
3. **Inherited simulation fixes can differ from retail.**
   `ObjectCreationList.cpp` documents a fixed uninitialized position in spread
   formation; `DumbProjectileBehavior.cpp` documents a fixed out-of-bounds
   flight-path read. Both explicitly warn about mismatches with unpatched
   retail clients. No evidence yet ties either path to this early failure;
   they establish that compatibility defines do not guarantee parity.
4. **Missing/incorrectly scheduled CRC messages remain a distinct candidate.**
   `GameLogic::processCommandList()` raises the same mismatch state both when
   connected players outnumber cached CRCs and when CRC values disagree.
   Therefore the photographed dialog alone does not prove two fully received
   values differed. Peer-index mapping, CRC scheduling and serialization need
   observation. Omarchy displaying the error does not identify which endpoint
   introduced the problem. The inspected CRC scalar and object ID paths use
   explicit integer fields; no specific 64-bit wire-layout defect was found.
5. **XR command/client state or effective data overrides remain possible.**
   XR orders inspected in `XrGameBoot.cpp` use the normal message stream, and
   the recent LAN delta does not directly change GameLogic. That is not proof
   that all client/render callbacks are free of simulation side effects.
   Compare an idle start before any command with one simple move; audit local
   state writes or logic-RNG use in rendering if the first mismatch depends on
   interaction. The matching six Zero Hour files plus map archives lower the
   data-mismatch priority, but base archives and loose overrides remain open.

### Bounded diagnostic slice

Before changing simulation behavior, log the first few normal CRC checkpoints
(retain the negotiated interval), local logic frame, connected player IDs,
received CRCs and an explicit `missing_crc` versus `different_crc` reason.
At those same checkpoints, capture rolling CRCs after objects, logic RNG,
partition manager, players and AI. Keep this opt-in and bounded; do not turn on
unlimited object logging or reduce the gameplay CRC interval for a retail test.
Use these traces with a same-source desktop peer or a second Quest. Retail
replay comparison is useful only if the replay actually contains recorded
logic CRCs; a replay that merely finishes is not proof of synchronization.

The observer is implemented in `Core/Libraries/Include/GXLanCRCTrace.h`, with
read-only integration in the existing Zero Hour logic CRC generation and
validation paths. It starts at actual live LAN map-load entry after the game
information is selected, not at the pre-intro/early-return step. Reset and new
match clear its budget. Campaign, offline Skirmish and replay are excluded.

Enable **Setup → Diagnostics → LAN sync checkpoints** (German:
**LAN-Synchronisationsprüfung**) and restart the game. Equivalently, create
`gx_lan_crc.txt` in the selected game-data directory; the XR boot path changes
to that directory before loading the game. Desktop same-source builds can use
`GX_LAN_CRC=1`. Neither mechanism changes network eligibility or makes LAN
supported in a normal offline release.

Each match emits metadata plus at most eight generation and eight validation
records and one additional local failure after that budget. Generation records
include local frame, local network slot, final CRC, RNG seed checksum and
rolling CRCs after objects, RNG, partition, players and AI. They use the
existing normal CRC traversal, without another object scan, random-number draw
or message. Validation records preserve received values and distinguish the
game's actual `detector_reason` from the observer's missing/different check.
The observer maps cached player indices to network slots using the same
read-only name lookup as `onLogicCrc`, but logs no names. `s0/p2:...` means
network slot 0, engine player index 2. These are different index spaces; the
existing mismatch detector is deliberately left untouched. A diagnostic
disagreement with that detector is evidence to investigate, not silently fix.
Do not compare a generation frame directly with a validation frame: the stock
CRC payload has no source-frame field and is processed later.

Quest **View Logs → Share** now includes the complete current and previous
XR stderr files in the existing ZIP, even if the on-screen log preview is
truncated. The new records contain no player names or IPs; the surrounding
existing logs can contain personal paths/network details. Keep raw captures
private. Export promptly, since subsequent launches rotate old logs.

Next physical procedure:

1. With the diagnostic candidate and marker enabled, repeat the same Direct
   Connect match against unmodified Steam/Proton. Retain map, seed/settings,
   factions and start slots where practical.
2. Initially issue no gameplay orders for about 30 seconds (or until mismatch).
   Note which endpoint reports the error and approximately when.
3. Stop and collect the current/previous Quest logs before repeated relaunches.
   If the idle test passes, repeat with one move order, then build/attack.
4. Compare the first normal checkpoints even when only Omarchy reports a
   failure. A Quest-local failure is not required for useful captured values.
5. Use an identically instrumented same-source desktop or second Quest for
   paired subsystem traces if the retail mismatch cannot be isolated.

The host observer test checks bounds, activation/reset and reasons; source
guards check unchanged cadence/message/traversal sites. Full simulation CRC
identity, actual two-peer trace collection and the 15-minute human match remain
device gates. A successful APK build does not close them.

#### 10213 build and device handoff

Version `1.2.13-lan-diagnostics` (10213), package
`com.generalsx.zerohour.xr`, is the current unmerged/unpublished local candidate
at `build/apk/Generals-Zero-Hour-XR.apk`. SHA-256:
`bd781f6462e0f959419ade77faca32b967a6407c08659a046b4547e02e22f0a8`.
Source is the implementation checkpoint accompanying this entry on
`codex/quest-pc-lan-preflight`. The accepted P23 default-off release is unchanged;
version overrides are packaging arguments, not a changed public default.

Verification: `cmake --build build/android-vulkan --target z_generals -j 6`,
local `package-android-zh.sh` for `GX_FLAVORS="zh xr"`,
`bash scripts/qa/lan-crc-trace-test.sh`, and
`bash scripts/qa/xr-workspace-test.sh build/android-vulkan` all pass (788
workspace assertions with preview disabled and again enabled). APK v2 signature,
ARM64 identity and packaged/native `libmain.so` hash equality were checked.
`adb install -r` succeeded on Quest `2G0YC5ZG9609PY`, and the installed APK
hash matches. Game data was retained; a marker was placed in the currently
saved game-data folder, not an obsolete prior import path. Previous APK/logs
were preserved locally before replacement.

The launch attempt was intercepted by Meta's **Controller required** dialog.
No first 10213 match/startup trace is claimed. The existing all-files app-op
remains allowed; no runtime permissions were changed. Activate controllers
and perform the above physical procedure. The full diagnostic records become
available only once a live LAN match begins.

### First 10213 instrumented failure

The user reported the error again. The current and previous XR logs were
read-only captured; installed version 10213 was confirmed. The current log's
SHA-256 is `7bf9c8e33586b2e4bc6a92dd794ae4f449e03c99c3e5dd00010366b3cfbd3f06`; raw logs remain outside the repository
because they contain unrelated private runtime details. Sanitized checkpoints:

| Quest validation frame | Connected network slots | Received CRCs | Observer | Existing detector |
|---|---|---|---|---|
| 105 | 0, 1 | slot 0 / player 2: `FF3C9DF3`; slot 1 / player 3: `EB80E220` | `different_crc` | `none` |
| 207 | 0, 1 | slot 0 / player 2: `C2F2A005`; slot 1 / player 3: `A6E45BA5` | `different_crc` | `none` |
| 305 | 0 | slot 0 / player 2: `2B8C9C08` | `none` | `none` |

Map CRC is `DEA9E8E4`, seed `14391758`, negotiated interval 100. The Quest
generated the matching local CRCs at frames 100 and 200. Frame 0 also has a
local generation record, but no paired validation record: do not claim the
very first simulation tick or first divergent subsystem has been identified.
Both values are present in the failed comparisons, excluding missing CRC
messages as the explanation for those particular checkpoints. Their different
values still require simulation/data/numerical or scheduling investigation.

`onLogicCrc` verifies a name-matched network slot, then stores the value under
`msgPlayer->getPlayerIndex()`. `processCommandList` subsequently passes that
cache key to `TheNetwork->isPlayerConnected()`, which expects a network slot.
Here that means testing slots 2/3 instead of connected slots 0/1, skipping both
values. `git blame` traces this check to the initial XR source import
`b99838b`, not the new diagnostic observer. This explains the local detector's
silence; repairing it alone would detect the mismatch, NOT make the peers
synchronize. No detector or simulation behavior was changed during capture.

The next bounded implementation was to unify the detector's player-to-slot handling,
including missing-connected-peer checks, with production-derived regression
fixtures for non-identity indices, disconnected entries, equal/different CRCs
and missing peers. Preserve wire messages, CRC contents and cadence. Then use
a paired instrumented peer to localize divergence. Until corrected, even two
Quests running without an error popup could be silently desynchronized.
Whether the user issued gameplay orders before this failure is still pending.

### Detector correction and paired-log comparison

The Zero Hour validator now calls `GXNetworkCRCValidation::evaluate`, resolving
cached engine player IDs through the existing name-to-network-slot mapping.
Every connected slot must have exactly one CRC; stale disconnected cache
entries cannot conceal a missing active peer. The shared dispatch cache remains
keyed by engine player index, so the base Generals and replay cache semantics
are not silently changed. The generation inputs, scheduled interval, outgoing
messages, random-number usage and game simulation are unchanged. A true
mismatch now reaches the existing mismatch handling on Quest as intended.
This fixes a false-negative detector, not the unequal Quest/Steam CRCs.

`bash scripts/qa/lan-crc-detector-test.sh` compiles the exact production
evaluator under UBSan with slot/name spies; cases cover non-identity mapping,
both endpoint arrangements, zero/equal/different CRCs, missing active peers,
disconnected stale entries and duplicate mappings. The observer regression
also passes. The source-level dispatcher/replay guards are not runtime proof
of a complete live network or replay session.

The companion read-only comparison tool is ready for paired instrumented logs:

```sh
python3 scripts/qa/lan-crc-compare.py quest.log pc.log
# For logs containing several matches, explicitly select one-based indices:
python3 scripts/qa/lan-crc-compare.py quest.log pc.log --match-a 2 --match-b 1
```

It refuses differing map CRCs, seeds or intervals, and requires actual
generation records at common frames. It reports the first *observed* differing
rolling checkpoint, not a root cause. Missing frame coverage is inconclusive;
even equal samples do not prove a whole synchronized match. Source build,
faction/slot/settings and effective data equality must be established
separately. Stock Steam logs do not expose these generation stages; the tool
cannot invent them. Its ten synthetic parser/comparison tests pass, and the
captured 10213 log self-comparison correctly reads all eight samples (parser
smoke only, not a peer result).

At the detector-only checkpoint, no PC binary or match existed: the Mac's
Docker/Colima VM was stopped and no Linux build tree was configured. SSH
availability on Omarchy was requested before any remote access. The subsequent
authorized native diagnostic build is recorded below; retail Steam/Proton
interoperability remains the goal, not replacement by the custom PC executable.

#### 10214 artifact and remaining gates

APK 10214 (`1.2.14-lan-crc-check`) is built locally at
`build/apk/Generals-Zero-Hour-XR.apk`, SHA-256
`c13aac9a39858771cf0232d29cf396f181fb9a0d105fe43b82617c7e501e34d1`.
ARM64 native and both `zh xr` APK flavors build successfully. APK v2 signature,
package/version/ABI and embedded `libmain.so` equality verify. Production
detector and observer host tests, ten comparator cases, and 788 workspace
checks for each LAN-preview gate setting pass. No GitHub Actions were used.
ADB initially reported no connected Quest. After reconnection, update-install
and package inspection confirm 10214; the on-device APK hash also matches.
Meta's controller-required dialog blocks the requested launch pending active
controllers; worn-headset detection remains pending. The previous
10213 APK was preserved locally before packaging. No release/defaults changed.

### Isolated Omarchy diagnostic preparation

Authorized SSH access succeeded. Docker daemon access is unavailable to the
user, and system CMake is absent; neither permissions nor system packages were
changed. A dedicated user-owned lab contains a source clone of `78207e6`,
checksum-verified portable CMake 3.31.6, pinned vcpkg
`a1cae005c39be7b18ba319fced856b68d7276271`, and an independent copy of the Steam
Zero Hour installation including nested `ZH_Generals`. Build tools may use
normal user caches; this is not an OS sandbox. Original Steam files, Proton
configuration and normal saves remain untouched.

The local test-machine lab is named `generals-xr-lan-diagnostics-78207e6`
under the SSH user's home directory. Its `bootstrap.sh` records tool versions
and configuration, `source/` contains the mirrored source delta, and `logs/`
contains successive configure/build attempts. `start.sh` delegates to the
versioned launcher. The executable and project-library closure are now staged
under `runtime/`. Do not commit this lab or its retail data.

Native configuration uses Clang, `linux64-deploy`, debug OFF, deterministic
math OFF and update checks OFF, matching the relevant Quest choices. SDL_image
3.4.0's actual `SDLIMAGE_AVIF=OFF` option is additionally required on this host;
the older `SDL3IMAGE_AVIF` variable does not disable its optional system AVIF
backend, whose imported target lacks a RelWithDebInfo location. No generated
dependency source was edited. Configuration now passes. The first compile
stopped at incompatible `strlcpy`/`strlcat` declarations against the host libc.
The local authoritative source now applies the existing `HAVE_STRLCPY` and
`HAVE_STRLCAT` guards to declarations in `stringex.h` and weak fallbacks in both
Generals and GeneralsMD `socket_compat.h` copies. The real Linux compile probes
pass with the configured feature macros. Subsequent builds exposed GameSpy's
`min`/`max` macros colliding with libstdc++ 16's chrono and valarray headers.
Scoped push/undef/pop guards at affected GeneralsOnline include boundaries now shield
standard/JSON includes while restoring legacy macro state for engine code.
These exact source edits were mirrored to the diagnostic clone; no dependency
sources, simulation expressions or class layout were changed. A subsequent
failure exposed an inherited build mismatch: the Linux source list compiles
`NetworkMesh.cpp`/`NextGenTransport.cpp`, but the GameNetworkingSockets 1.6.0
dependency was installed and linked only for Android. The diagnostic follow-up
extends dependency availability to Linux without changing the existing
Android-only P2P feature definition. This does not enable desktop internet
match support or change native LAN dispatch. No transport implementation was
removed to make the compiler pass.
The additional user-local abseil/protobuf/utf8-range/GNS builds and final Linux
configuration succeed; the native engine build then resumed with these targets.
Compilation then exposed `OWNERSHIP_COOKIE` declared only inside
`MEMORYPOOL_DEBUG` despite unconditional release-code uses. Its unchanged
`0x47454e58` declaration is moved outside that guard; allocation algorithms,
field layout and ownership checks are unchanged. Debug mode is not enabled
as a workaround.
Both exact Linux memory-pool object targets compile after this correction.
A separate desktop compile failure exposed Quest BACK cancellation referencing
mobile-only `TouchState`/`s_touch` without its platform guard. Only that block
is now guarded by `SAGE_MOBILE_PLATFORM`, matching the declarations; ordinary
keyboard dispatch stays outside and mobile behavior is unchanged.
The final desktop link additionally lacked `GX_XR_OffscreenBoot`: its only
definition was in the Android-only XR bootstrap. Non-Android SDL main now
defines the normal-windowing default `false`; Android retains its original
single definition. No desktop XR capability is being introduced.

The PC source is therefore `78207e6` plus these explicit build-compatibility
fixes, not a byte-identical source snapshot of installed APK 10214. The
simulation and CRC implementation remain unchanged. The header changes pass
Android compilation, including a successful CMake/vcpkg reconfiguration and
final native `z_generals` rebuild after all compatibility changes. Workspace
regressions also pass 788 checks with each LAN-preview gate setting.
Do not silently replace the installed APK or its recorded hash
with outputs of this compile-only regression run.

The versioned `scripts/build/linux/run-lan-diagnostic-zh.sh` accepts an explicit
lab path with staged `runtime/` and copied `game/` directories. It requires a
real graphical terminal, retains `HOME`, redirects XDG paths and DXVK outputs,
disables optional SagePatch preload, enables the bounded CRC observer and keeps
one unique log per run. Its synthetic tests pass on macOS and Omarchy, including
quoted paths/arguments, child exit status and missing-data/symlink/headless
refusal. These tests do not launch the real game or establish synchronization.
The detector/observer UBSan tests and all ten comparator tests also pass on the
actual x86_64 Linux host.

Two additional real-Linux syntax probes use production GeneralsOnline headers
and the Recorder translation unit's actual CMake flags/PCH. With GameSpy
`min`/`max` initially defined, the protected includes compile and both macros
remain intact afterward. With both initially undefined, they remain undefined.
These probes pass on Clang 22/libstdc++ 16; they do not establish runtime LAN
compatibility.

Read-only SSH hashing also confirms the remaining base gameplay archives match
the previously measured Quest values:

```text
ZH_Generals/INI.big   bff8d621088b25fd8b041c8acca020a020fabc66f972ab2bd131fc67d905a72c
ZH_Generals/Patch.big 28dc194412f96dc1f66412430cf74f2d89ad0cdabf70d2c8d1179d8e51743494
```

This reduces archive-mismatch uncertainty; it does not exclude loose overrides
or prove identical effective simulation.

#### Native diagnostic artifact and next physical test

The full Linux `z_generals` build/link passes. Staged `runtime/GeneralsXZH`
is 100603368 bytes, SHA-256:

```text
9bad0faa0c075f4f3de71c63ab8615806818a9f39fbd70bc8558599f5cb6597f
```

DXVK d3d8/d3d9/dxgi, SDL3, SDL3_image, OpenAL and GameSpy are staged alongside
the executable. `LD_LIBRARY_PATH` points to this runtime; `ldd -r` reports no
missing libraries or unresolved relocations. FFmpeg and other system libraries
resolve on this particular host, so this is not a portable Linux distribution.
Copied game/base asset checks pass. No actual game was launched through SSH;
loader validation is not graphical startup or synchronization acceptance.

From a graphical terminal on the PC, run:

```bash
bash "$HOME/generals-xr-lan-diagnostics-78207e6/start.sh"
```

Keep the retail Steam game closed for this diagnostic comparison. Activate
both Quest controllers and start installed 10214. Host a native LAN match on
Quest and join from the diagnostic PC through Direct Connect, using Quest's
current LAN address. Use the same map, fixed factions, no AI and identical
settings; leave both sides without orders for the initial 30 seconds. Record
whether either side reports a mismatch, then capture both logs before restarting.
The PC launcher prints its unique `logs/native-zh-*` path; Quest logging remains
in the current/previous XR stderr logs. Compare only matching map CRC/seed and
generation frames with `lan-crc-compare.py`. Initial agreement is not a sustained
match pass; unit orders, longer play and original Steam/Proton are later gates.

The first graphical Quest-hosted match against this native Omarchy diagnostic
build started but ended with a real CRC mismatch. In its unique PC log
`logs/native-zh-20260916-173034.PDMLqy`, the diagnostic reports map CRC
`DEA9E8E4`, seed `4042777`, interval 100, local slot 1; generated CRCs are
`3263A8D7` at frame 0 and `A6E913D4` at frame 100. Validation frame 105
received slot 0/Quest `E2E3DF5F` versus slot 1/PC `A6E913D4` and classified
`different_crc`, excluding a missing-CRC-message explanation for this failure.
Quest USB ADB was reconnected and both current and previous XR stderr logs
were preserved in a private temporary directory. The Quest log has the same
map CRC, seed and interval; at frame 0 every rolling checkpoint and final CRC
matches the PC (`3263A8D7`). At frame 100 the Quest's first object-list
checkpoint is `6AE75FBB` while the PC's is `03972538`; their final CRCs are
`E2E3DF5F` and `A6E913D4`. The existing paired comparator reports the first
observed difference at generation frame 100, stage `objects`. Both sides
record `different_crc` at validation frame 105. The RNG seed checksum at
frame 100 matches (`A82FF014`). Later rolling checkpoints are downstream of
the object CRC and cannot independently identify other differing subsystems.
This does not yet identify the object, the exact first divergent tick or root
cause. The next diagnostic should bound per-object identification at the
normal CRC checkpoint; do not alter CRC cadence, simulation or wire format.

Omarchy's Direct Connect initially displayed Tailscale `100.123.209.83`, even
though its route to the Quest uses WLAN. The separate diagnostic profile's
`Options.ini` now pins both `IPAddress` and `GameSpyIPAddress` to WLAN
`192.168.178.158`; this did not modify original Steam/Proton settings. The
subsequent match did start and reached CRC validation, so the mismatch is a
simulation-state issue, not simply Tailscale addressing.

Retail Steam/Proton retests remain the goal after localization of divergence.

### Two Quests: expected advantage, unverified

Two Quest 3 devices using identical APK 10212 and matching game data avoid the
retail-versus-port implementation gap and most CPU/compiler/math differences.
This makes synchronization more plausible than Quest versus Steam/Proton, but
does not eliminate command routing bugs, local-state/RNG side effects,
uninitialized memory or missing CRC messages. Test first with Direct Connect
and an idle match, then orders from both players and the 15-minute gate.
Automatic discovery and sustained Quest-to-Quest play have not been verified.

Public offline release 1.2.28 remains the supported baseline; LAN 10231 stays experimental.
Do not enable LAN tabletop by default, merge a network-eligibility expansion into a release, or claim multiplayer support while these physical gates remain open. Keep replay and internet as separate later work. Keyboard/mouse remains secondary to the controller path.

#### 2026-09-21 per-object diagnostic pair

The resumed branch `codex/lan-object-crc-trace` implements the next bounded
observer without altering production simulation or the wire protocol. Android
ARM64 and the XR release package build successfully. The signed Quest artifact
is:

```text
build/apk/Generals-Zero-Hour-XR-1.2.31-lan-object-trace.apk
SHA-256 41b24487ef54540d6890f34285d72752adc8b9357657eca24863f989caddcea2
libmain.so e5c4871698427e4a44a084de021dd9284011e00ddb3f13928381436f4fdeddcd
```

Package/version, release signing certificate, non-debuggable manifest, ABI and
embedded native-library equality verify. It is update-installed on Quest
`2G0YC5ZG9609PY`; the on-device base APK has the same SHA-256. Existing app data
and imported retail data were retained. The marker `gx_lan_crc.txt` remains in
the game's writable data directory.

The same six source/test changes were applied over the Omarchy lab's required
Linux compatibility delta. Its incremental native build/link succeeds and
`ldd -r` reports no missing library or unresolved symbol. The staged executable
SHA-256 is
`a403f82f9ddf7e8a5e9de6bb3b05699c2098df5736ad886299139f57e01bf48f`;
the prior executable is preserved as `runtime/GeneralsXZH.pre-object-trace`.
The launcher continues to isolate configuration and data, pins WLAN
`192.168.178.158`, enables `GX_LAN_CRC=1` and leaves Steam/Proton untouched.

Focused observer/detector tests pass. The paired comparator now passes 14
synthetic tests covering CRC differences, traversal-order differences,
truncation and incomplete records. Workspace validation passes all 788 checks
with the LAN presentation gate disabled and enabled. The physical test remains:

1. On Omarchy's graphical desktop, run
   `bash "$HOME/generals-xr-lan-diagnostics-78207e6/start.sh"`.
2. Let Quest host through Direct Connect on the same stock map and fixed
   factions used for the paired trace; use no AI and issue no orders for at
   least the first 30 seconds.
3. Stop after the mismatch or after enough checkpoints, preserve the unique
   Omarchy `logs/native-zh-*` file and the Quest current/previous XR stderr
   logs, then run `scripts/qa/lan-crc-compare.py` on that matching pair.

Do not interpret the diagnostic install as multiplayer acceptance. After the
first object/timing cause is demonstrated and corrected, repeat same-source idle
and interactive matches before returning to unmodified Steam/Proton and the
separate Windows Steam gate.
