# PLAN-025 — Quest ↔ PC LAN preflight

**Status:** Quest 10212 and Steam/Proton enter a match, then Omarchy reports an in-game synchronization mismatch, reproduced after reinstalling P23 and updating to 10212. Six Zero Hour gameplay-data hashes match; the cause is not yet isolated. Diagnostic APK 10213 is built and installed with opt-in prepared; its first instrumented match is pending.
**Scope:** One Quest 3 against a PC on the same LAN. The first peer is the user's Steam Zero Hour running through Proton on Omarchy; the planned Windows Steam peer remains a separate validation. If retail gameplay desynchronizes, isolate it with a same-source GeneralsX PC build. Internet services, public matchmaking, replay and reconnect are later gates.

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
| Build safety | Default-off and preview-on mode tests; Android native/XR APK build | 10213 native + both APK flavors pass; observer and workspace regressions pass; installed APK hash verified |
| Lobby | Discovery and direct-IP outcomes, both endpoint IPs, host/join/leave, chat/input | Direct Connect reaches lobby and starts match; automatic discovery still fails |
| Simulation | 15-minute Quest ↔ PC human match, orders from both players, no desync/CRC/stall | Failed first Steam/Proton attempt: peer shows the in-game synchronization mismatch dialog shortly after map start |
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

### Two Quests: expected advantage, unverified

Two Quest 3 devices using identical APK 10212 and matching game data avoid the
retail-versus-port implementation gap and most CPU/compiler/math differences.
This makes synchronization more plausible than Quest versus Steam/Proton, but
does not eliminate command routing bugs, local-state/RNG side effects,
uninitialized memory or missing CRC messages. Test first with Direct Connect
and an idle match, then orders from both players and the 15-minute gate.
Automatic discovery and sustained Quest-to-Quest play have not been verified.

P23 remains the planned public offline preview; LAN 10213 stays experimental.
Do not enable LAN tabletop by default, merge a network-eligibility expansion into a release, or claim multiplayer support while these physical gates remain open. Keep replay and internet as separate later work. Keyboard/mouse remains secondary to the controller path.
