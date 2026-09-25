# Generals: Zero Hour XR - Current Handoff Status

**Updated:** 2026-09-25
**Audience:** maintainers and coding agents continuing the Quest/XR work  
**Active target:** Meta Quest 3, native OpenXR with OpenGL ES 3  
**Product package:** `com.generalsx.zerohour.xr`

This is the short, maintained entry point for the XR branch. Read it before the
long implementation record. It distinguishes source completion, automated
verification, device installation and actual worn-headset acceptance; those are
not interchangeable.

## Quest-to-Quest LAN release candidate — 2026-09-25

The owner decided to ship Quest-to-Quest LAN as the 1.2.34 preview. Branch
`codex/quest-quest-lan-diag` merged main `286d6c2` into the handoff head,
replaced the LAN branch's Direct Connect keyboard with main's Meta keyboard and
added two fixes: the keyboard reopens only for a press on the focused field, and
the LAN lobby binds INADDR_ANY off Windows plus holds a Wi-Fi MulticastLock so
Quests see each other's hosted games. Two Quest 3 headsets with identical APK,
OS build and game data played a Direct Connect match; both snapshots agree
(148 CRC generations, 134 commands, `reason=match_end`). The release branch
`codex/xr-release-1.2.34-lan` enables `GX_XR_LAN_PREVIEW` and
`SAGE_USE_DETERMINISTIC_MATH` in the `android-vulkan` preset so release builds
match the tested configuration. **Quest <-> PC/Steam remains unsupported**; the
PLAN-025B retail goal is still open and is not claimed by this release.

## Harness handoff — 2026-09-24

Start from the head of `codex/lan-object-crc-trace`; implementation checkpoint
`078f8ea` is unchanged by this documentation handoff. Read
[PLAN-025B_MULTIPLAYER_HARNESS_HANDOFF.md](PLAN-025B_MULTIPLAYER_HARNESS_HANDOFF.md)
for the exact desired outcome, read order, verified evidence, research leads,
non-goals and receiving harness's first assignment. Use a separate continuation
branch. Do not automatically rebase/merge the now-diverged `main` into this
baseline; the handoff PR is experimental and must remain unmerged.

Repository/release state was refreshed today: remote `main` is `286d6c2` and
GitHub's latest public release is now **1.2.33** (2026-09-23), not the historical
1.2.28 baseline. Neither release is changed by the LAN handoff. Similar public
and private diagnostic version numbers are different artifacts; use hashes and
certificates to identify them. No new device check/build/install/match occurred
today; connectivity and test results below retain their original dates.

## Current LAN work — 2026-09-22

The isolated `codex/lan-object-crc-trace` branch resumed from clean/pushed
`4fdda8a`. The fixed-object follow-up is replaced by an opt-in universal
snapshot: the last eight scheduled CRC generations throughout the match,
2048 object/type/field records per generation, raw transforms, global stages,
all six logic RNG words and 4096 executed commands. One bounded dump is emitted
on mismatch or orderly match reset; a strict paired comparator localizes the
first retained difference and shows command context. Production CRC inputs,
simulation rules and network format are unchanged. See
[PLAN-025A](PLAN-025A_UNIVERSAL_DESYNC_SNAPSHOT.md).

Host tests pass (21 new comparator fixtures, production observer/command tests
on ARM64 and x86-64/Rosetta, legacy detector/trace/20 comparator fixtures and
788 workspace checks per LAN gate). Android ARM64 linking and packaging pass.
The local **10237 debug-signed QA APK is not update-installable over the installed
release certificate**; release signing and the matching Omarchy build remain
pending. No device has been updated this session.

Live preflight finds **no Quest via ADB** and **no SSH response from either
recorded Omarchy address**. The last physical evidence remains 10235's matching
idle checkpoints followed by frame-400 object `000000D3` divergence after
construction. The 10236 target-object retest was intentionally superseded.
The representative construction/production/movement/combat/ability match and
all new cause/fix evidence remain open, followed by sustained and unmodified
Steam/Proton/Windows gates. Quest-to-Quest is an independent untested path.

At the 2026-09-22 checkpoint, GitHub listed **1.2.28 as the latest public release**;
see the 2026-09-24 handoff above for the current public baseline. No change is merged
or published; the main workspace's pre-existing dirty state is left alone.
Older baseline sections below are historical milestone records.

## Read order and sources of truth

1. `AGENTS.md` for repository-wide engineering rules.
2. This file for the current XR baseline, open gates and next work.
3. `PLAN-024_QUEST_UI_COMMAND_WINDOWS.md` when working on P21.
4. `PLAN-025_QUEST_PC_LAN_PREFLIGHT.md` for the current Quest-to-PC LAN slice.
5. `PLAN-023_QUEST_TABLETOP_RECOVERY.md` for detailed architecture, decisions,
   dated implementation evidence and rollback boundaries.
6. The newest entries in `docs/DEV_BLOG/2026-09-DIARY.md` for the latest delta.
7. The actual branch, diff and test output. A document never overrides code.

Update this file whenever an XR milestone changes state, a default changes, a
new physical acceptance result is received, or the next priority changes. Keep
the detailed narrative and command transcripts out of this dashboard.

## Repository and handoff contract

- Canonical collaboration repository: private
  `Cesarus85/Generals-Zero-Hour-XR`, branch `main`.
- Start delegated work from a clean, pushed checkpoint and record the starting
  commit in the task or pull request. Do not work from an uncommitted shared
  checkout.
- Use a separate branch and preferably a separate Git worktree per harness.
  P21's suggested branch is `claude/p21-ui-command-windows`.
- Documentation, code comments and commit messages are English-only. The game
  itself intentionally supports German and English XR text.
- Never commit retail game files, extracted user data, signing keys, room data,
  local absolute paths, generated build trees or headset-private captures.
- Deliver delegated work through a pull request with changed-file summary,
  tests, APK hash where applicable, and a list of physical checks still open.

## Current product baseline

| Area | Current state | Acceptance level |
|---|---|---|
| Product identity | Generals: Zero Hour XR; update-compatible package ID retained | Built, installed and resource-verified |
| Game modes | Campaign and offline AI Skirmish run in the XR tabletop presentation | Repeated user headset use; mission-specific coverage remains incremental |
| Battlefield | Original engine terrain, objects and effects rendered as a stereoscopic miniature world | P7.4 accepted; later graphics/performance steps used in live play |
| View model | Gameplay is tabletop-only; videos and full native dialogs use an upright presentation | Implemented and exercised, not every campaign transition exhaustively tested |
| Controller input | Ray selection, contextual orders, drag-box multi-select, additive selection, camera pan/rotate/zoom and building rotation | Core flow accepted in headset; rare commands remain ongoing coverage work |
| Commands console | Persistent spatial console with direct orders, groups, tactics, camera bookmarks, Communicator and in-place help, redesigned into grouped sections with persistent armed/pending/toggle/disabled states | P21 visual presentation accepted in the headset; focused host tests pass |
| Workspace UI | Spatial settings window for table/build manipulation, graphics, handedness, language, help and play-space setup, regrouped into intent sections with value chips | P21 visual presentation and current interaction accepted in the headset; focused host tests pass |
| Build window | Original production/build UI detached above the tabletop and independently movable, scalable and tiltable | User accepted current arrangement and interaction |
| Localization | XR interface and help support German and English; initial choice follows German OS, otherwise English | Resource/host checks pass in both languages, including long German labels with shrink-to-fit rendering; device use remains a physical gate |
| Play-space setup | Optional free board, detected table/floor and manual-height workflows; no required room binding; unanchored launches use safe HMD-relative geometry | P19.1 and P20/P20.3 startup, transition and explicit-alignment behavior accepted in the headset |
| Game-data setup | Guided Steam or installed/extracted CD/ISO folder import with validation; raw images/installers are not extracted | Host and Quest instrumentation pass; real Steam-based use confirmed |
| Returning launch | Saved valid data is checked inside the XR Activity; setup opens only when data is missing or invalid | Device launch verified; final visual no-flash confirmation remains a physical gate |
| Performance defaults | Balanced resolution, light shadows, Multiview preferred, redundant extra world copy omitted automatically | Reported as relatively smooth and playable; no universal FPS guarantee |

## Current visual and interaction defaults

- Every XR process starts in a safe, free-standing tabletop arrangement relative
  to the first fully tracked HMD pose, once per process. Match entry, campaign
  loading and camera unlock never re-align an existing workspace (P20.3).
  Spatial geometry from a previous room is ignored; preferences remain saved.
- During gameplay, plain X recenters the board and companion windows together,
  preserving their relative arrangement and sizes. In arrangement mode, X still
  resets only the selected surface. The default UI/Windows page also provides
  **Align everything in front of me**; confirmed table/floor/manual-height
  placement requires an explicit leave/cancel choice for this action or X.
  Estimated/untracked poses cannot place/grab
  surfaces; tracking recovery does not reset the workspace.
- Board width: 1.65 m in the photo-inspired default layout.
- Detached build window: 1.8 m wide, above and behind the far board edge, with
  free tilt retained after release.
- Commands console: to the left of the build window, gravity-upright and yawed
  inward toward the player.
- Pointing-hand trigger selects, orders and draws a selection rectangle.
- Support-hand stick pans the map; pointing-hand stick rotates and zooms.
- Support-hand stick click returns the map view to the native command-center
  target (fallback: most expensive owned building), without changing selection
  or physical workspace. Default left stick click, right in left-handed mode.
  Modal UI, editing, building preview, camera locks and tracking/focus loss block
  it. Radar remains native: pointing Grip looks there, Trigger orders selected
  units with standard mouse controls. The controller guide explains both.
- Handedness swaps these roles. The in-game controller guide is authoritative
  for the complete mapping.
- The UI menu identifies the selected manipulation target in orange; cyan is
  reserved for laser hover.

## Historical release and device baseline (through P23)

The newest private Quest preview is `xr-preview-2026-09-16-p23` from PR #5,
merge `ba9169d5c81604aa9fac00508cf2e5dcbf9a4939`; its APK and hash are
recorded in the P23 section below. The earlier 10208 release remains the latest
non-prerelease checkpoint and combines P21, P20/P20.3, base navigation and PR
#2's reproducible DXVK/Android foundation:

```text
tag v1.2.8-xr-preview
version 1.2.8-xr-preview (10208)
source 29fbcb9 (PR #1 merge fc37591; PR #2 merge c01ad1d)
release asset Generals-Zero-Hour-XR.apk
SHA-256 0763b01dd0dfccfc99666a1a4dd614fdc0dde41a2f72282db9258eeee0927660
Android release CI 35032355387
```

The release APK is ARM64, reports package `com.generalsx.zerohour.xr`, has the
expected product label/launcher and verifies with APK Signature Scheme v2. CI
verified native artifacts and the packaged `libmain.so` dependency closure.
Release and checksum assets are attached at
`releases/tag/v1.2.8-xr-preview`. Installation of this final 10208 package was
explicitly deferred to the next device session; do not claim it as installed.

An earlier explicitly ADB-verified and worn-headset-accepted Quest candidate
was the content-equivalent predecessor:

```text
version 1.2.7-base-navigation (10207)
source follow-up 5b089cb (base shortcut 6f146a5); integration commit be2fbaf
build/apk/Generals-Zero-Hour-XR.apk
SHA-256 cb05a9fc3d49c089484cfc8a06ffa2e8c00198d935bfe2bfce0b547ddfe47844
```

Native build and both `zh`/`xr` APKs pass; the XR v2 signature, packaged native
dependency closure, staged-versus-packaged libmain match and 89 branding checks
pass. Installation on Quest 3 `2G0YC5ZG9609PY` used `adb install -r` without
clearing user data; package inspection confirms 10207. A launch was requested;
the user subsequently accepted the final presentation, workspace stability and
base-navigation result in the headset. This contains P20.3 plus the base shortcut
and supersedes 10206. The integration commit is historical and local-only; merged
`main` is now the source authority. PR #2 Android CI `35021079076`, PR #1 Android
CI `35027123032` and release CI `35032355387` pass. The repository-wide generic
Linux/macOS workflow remains a known unrelated red baseline and is not evidence
against the Quest APK gate.

## Immediate implementation queue

### Navigation follow-up - base shortcut

Accepted after P20.3: support-hand stick edge dispatches the original local
`MSG_META_VIEW_COMMAND_CENTER`. No simulated keyboard events, new unit orders,
simulation/network changes or multiplayer eligibility expansion. Host tests:
interaction 135, handedness 264, console bridge 1433, bilingual panel payloads
19593, workspace 732 and build-controls 4292 pass. The 10207 APK is built and
installed as above. The user confirmed the shortcut returns to the base and the
overall result feels correct. Both handedness modes and the original radar
Grip-to-look/Trigger-to-order distinction remain useful regression checks rather
than blockers for this merge.

### P20 - safe fresh placement

Implemented on the P21 follow-up branch after headset review. Each new XR
process discards unanchored spatial poses and starts with the complete compact
photo arrangement in front of the first valid HMD pose: 1.65 m board, angled
build window behind it and gravity-upright Commands window on the left. Language,
graphics, handedness and map-coverage preferences still load from disk. Explicit
surface/manual placement and free adjustment remain valid for the current app
session. The user accepted initial placement but reported later separation and
campaign misplacement. P20.2 fixes match-entry alignment, whole-workspace X reset,
fully-tracked pose gating and reference-space changes during nested campaign
video playback. Incident logs were unavailable: these are verified faulty code
paths, not proof that every observed motion had the same cause. Host validation:
workspace 684, interaction 115, scene 1280, movie presenter 147, panel text 18625
and menu routing 317 checks pass; build-controls 4292 also pass. The historical
10205 APK was built and installed. Worn-headset Skirmish/campaign transition, dim-room
tracking recovery and seated/standing recenter checks remain open.

P20.3 follow-up: the user verified that manual placement survives Skirmish to
campaign and resets only after process restart, but observed a late glance-based
relocation. Remove deferred match-entry placement altogether. Initialize geometry
once, expose explicit whole-workspace alignment on the default UI page, and use
the same real-surface confirmation for the button and gameplay X. Host checks:
workspace 732, interaction 120, menu routing 327, menu geometry 2869, scene 1280,
loading presenter 147, bilingual panel payloads 19593 and build-controls 4292
pass. The later 10207 candidate is built and installed as above. Headset testing
confirmed stable Skirmish/campaign placement across a session, reset on a new
process, explicit whole-workspace X alignment and the final presentation. Pull
requests #1 and #2 are merged and the final 10208 APK is published; only
installation of that final package is deferred to the next device session.

### P20.1 - thinner tabletop underbody

Implemented on `codex/p20-1-thin-underbody` (2026-09-16). The soil/base
transition remains `-0.018` board widths while the underside moves from
`-0.036` to `-0.027`; the visible grey/dark lower trim is therefore exactly
halved from `0.018` to `0.009`. New `XrBoardGeometry.h` is the single source for
the soil datum, underside, lip, real-surface clearance and editing-outline
clearance. Terrain registration, world height, map coverage and free-standing
board poses do not move. Real table/floor placement now rests the thinner
underside at the same 2 mm clearance, and the orange editing outline follows
that underside.

Host validation passes: board 74, height/plinth/render-pick 272822, scene 1280,
build controls/outline 4292, menu geometry 2869, menu routing 327, interaction
135 and workspace 732 checks. The Android candidate is versioned
`1.2.9-p20.1-thin-underbody` (10209) so it remains update-installable over the
published 10208 release. Android CI run 35055859028 passed and produced the
43,462,061-byte ARM64 APK with SHA-256
`ef5c544cee9fd9212b71807a62ddfbfc1a33ce7feb22f28ff5dd8a7487ad9742`;
package identity, launcher and APK Signature Scheme v2 were verified locally.
The 10209 APK was update-installed with existing app data preserved, launched
successfully and accepted by the user in the Quest 3 on 2026-09-16. The thinner
edge reads correctly in the headset and no regression was reported. P20.1 is
merged in PR #3 at `1b1ff6d`; retain real-surface placement and underside
artifact checks in later tabletop regressions.

Hosted CI policy was tightened after the private-account Actions allowance was
exhausted by inherited cross-platform matrices. Linux/macOS desktop builds are
now manual compatibility/release checks only. A pull request receives the full
Android/Quest build only while labeled `quest-build`, or it can be started via
`workflow_dispatch`; concurrency cancels obsolete Android work for the same PR.
Local host checks and local Quest packaging remain the normal iteration path.

### P21 - UI and Commands window presentation

Rework both spatial windows into a coherent, attractive and quickly readable
Generals-inspired interface without changing command semantics. The complete
delegation contract is `PLAN-024_QUEST_UI_COMMAND_WINDOWS.md`.

Implementation (2026-09-15, `claude/p21-ui-command-windows`) is complete,
host-verified and accepted in the headset. P20 and its alignment/navigation
follow-ups were added on the same branch and accepted as the stable presentation
baseline. Android CI passes, PRs #1/#2 are merged and private release
`v1.2.8-xr-preview` is published. P20.1 is the recommended next implementation;
no P20.1 or P22 changes are included here.

### QTR-MP - retained multiplayer and replay milestone

Offline AI Skirmish is the primary supported test mode but is not evidence for
human networking. LAN/internet matches, deterministic synchronization, reconnect,
chat/team chat, diplomacy/player controls, observer/replay access and their XR
interaction need a separately authorized compatibility and headset-validation
slice. P20-P22 must not change simulation commands or wire formats casually.
On 2026-09-16 the user authorized beginning this slice and chose a Windows PC
running Steam Zero Hour as the first counterpart to one Quest. Branch
`codex/quest-pc-lan-preflight` starts from `a5cb2d0393d9778bc0fcbb013020b29d1e750325`.
`PLAN-025_QUEST_PC_LAN_PREFLIGHT.md` records the two-endpoint procedure and
gates. The source now has a default-OFF Android-only `GX_XR_LAN_PREVIEW` flag
that can admit `GAME_LAN` to tabletop rendering in a deliberately built test
candidate; internet and replay remain excluded. Host eligibility tests pass
with the flag both off and on. The local native build and XR APK package pass:
`build/apk/Generals-Zero-Hour-XR.apk`, version 10210
(`1.2.10-lan-preflight`), SHA-256
`5cc4f84a04eb7b44bab906f8acff3414b1e5d9b808fafc04b557d8fcfb374a4b`.
APK v2 signing and native staging match are verified. On 2026-09-16 the test
APK was update-installed on Quest 3 `2G0YC5ZG9609PY` with app data retained;
the installed `base.apk` reads back with that exact SHA-256. It has not been
published. Direct Connect now reaches a Steam/Proton LAN lobby; tabletop human
gameplay and a sustained match are not verified. The accepted P23 release APK
remains unchanged.
The first user Quest ↔ Windows Steam Zero Hour test found that the LAN lobbies
do not automatically discover each other despite the same Wi-Fi. Both menus
explicitly identify Zero Hour; their differing animated shell backgrounds are
not proof of a network/SKU mismatch. Quest has one active WLAN IPv4 interface
and no saved fixed LAN IP. This initial discovery failure alone does not prove
Steam incompatibility or usable LAN gameplay.
An alternate Omarchy laptop (`192.168.178.158`) runs Steam Zero Hour through
Proton and is reachable from Quest WLAN (3/3 ICMP replies). Direct Connect
reaches the shared game lobby, but Omarchy reports that it lacks the selected
map. The Quest hosted and the user believes the selected map was Alpine
Assault, matching its last `Network.ini`. Quest and Omarchy SHA-256 values for
`MapsZH.big` and base Generals `maps.big` are identical, so reimporting those
archives is not the next step. Effective map lookup/cache/path/CRC still need
diagnosis; the match has not started. Quest Direct Connect IP entry also lacks
a usable controller-triggered virtual keyboard in the 10210 test. A new
unmerged 10212 candidate changes LAN map serialization to literal legacy
paths (retaining encoded replay/save metadata) and adds a controller-operated
spatial keyboard for Direct Connect player name and IPv4 entry. Native and
APK builds, v2 signing, embedded-library verification, 3848 menu geometry,
329 menu routing, and 22429 bilingual panel-payload checks pass. Version
10212 (`1.2.12-lan-keyboard`), SHA-256
`e8f05cc2c77a3d125117ed7a836e36fdcd159f59d4807d37a89006954fd166fc`,
was update-installed on Quest 3 `2G0YC5ZG9609PY` with data retained. The user
now confirms Direct Connect and actual map/game start against Omarchy Steam
Zero Hour via Proton. A photo shows Omarchy's in-game dialog: "Game has
detected a mismatch. This means the multiplayer game has lost synchronization
data between the players." The loaded map/base is visible behind it. This is
a simulation synchronization failure, not the earlier map warning or an
ordinary connection timeout. The Quest log confirms tabletop world
presentation but no explicit local CRC-mismatch event in the inspected
window; the cause is pending. The user-supplied Omarchy SHA-256 values for
`INIZH.big`, `PatchINI.big`, `PatchZH.big`, `PatchData.big`, and both loose
multiplayer/skirmish `.scb` scripts all match Quest. Base Generals INI/patch
archives and loose overrides remain unchecked. Test a same-source PC build
after that before treating retail Steam
compatibility as viable. No sustained human match has passed. Automatic LAN
discovery remains broken.

The mismatch was reproduced after the user's successful P23 reinstall and our
verified update to 10212 (installed APK hash matches the candidate). Source
audit finds native platform math active and the alternative GameMath wrappers
still partly TODO; `-ffp-contract=off` is already present. Inherited retail
simulation differences remain candidates. The same mismatch handler also
handles missing expected CRC messages, so the diagnostic must distinguish
missing messages from different values and collect rolling subsystem
checkpoints for comparison. PLAN-025 contains the ranked audit. Two identical Quest 3 peers
should avoid several cross-build differences but remain untested. P23 remains
the planned public offline preview, with LAN development continuing separately.

The first **unpublished diagnostic candidate** was 10213
(`1.2.13-lan-diagnostics`) on `codex/quest-pc-lan-preflight`, at
`build/apk/Generals-Zero-Hour-XR.apk`, SHA-256
`bd781f6462e0f959419ade77faca32b967a6407c08659a046b4547e02e22f0a8`.
It adds opt-in first-eight generation/validation CRC records and one later
local failure, without changing simulation rules, messages or CRC cadence.
Records distinguish network slots from engine player indices and actual
detector reasons from the observer's classification. Setup has an EN/DE
diagnostic switch; View Logs shares current and previous full XR stderr logs.

The native ARM64 build and both Android APK flavors pass; observer UBSan tests,
source guards and 788 workspace checks for each LAN-gate setting pass. APK v2
signing, package/version/ABI, bundled native-library equality and the installed
APK hash verify. Update-install on Quest `2G0YC5ZG9609PY` retained data. The
`gx_lan_crc.txt` marker was placed in its saved game-data folder for the next
test. The initial launch was blocked pending controllers; the user subsequently
reproduced the error and the real 10213 LAN trace was captured successfully.
No runtime permissions were changed.

At validation frame 105, both peers' CRCs arrived but differed (Quest
`FF3C9DF3`, peer `EB80E220`); frame 207 confirms another unequal pair. The
observer reports `different_crc` while the original detector reports `none`.
Source inspection confirms an inherited index-space defect: cached engine
player indices 2/3 are passed to a connected-network-slot check expecting 0/1,
so the Quest skips these CRC comparisons. At frame 305 only slot 0 remains
connected. This explains a missing Quest-side error, not the original differing
CRC values. The first divergent simulation tick/subsystem remains unknown;
an idle-before-error test has not yet been confirmed by the user.

The detector mapping is now corrected in source: every connected network slot
requires exactly one CRC, regardless of internal player index; stale
disconnected entries cannot mask missing active peers. Its production evaluator
and the bounded observer pass UBSan tests. No simulation math, CRC inputs,
generation cadence, message format or shared replay cache was changed.

**Current local APK:** version 10214 (`1.2.14-lan-crc-check`),
`build/apk/Generals-Zero-Hour-XR.apk`, SHA-256
`c13aac9a39858771cf0232d29cf396f181fb9a0d105fe43b82617c7e501e34d1`.
Native ARM64 and both Android flavors build; v2 signing, package/version/ABI
and bundled-library equality verify. Workspace regressions pass 788 checks
with each LAN gate setting. **Installed:** the subsequent Quest reconnection
allowed an update-install of 10214 without clearing data; package inspection
confirms the version and on-device APK SHA-256 matches the value above. Meta's
controller-required dialog blocks the requested launch until controllers wake.
Worn-headset detection and paired-match acceptance remain open.

A paired-log comparator (`scripts/qa/lan-crc-compare.py`) is ready and passes
ten synthetic tests. It requires matching map CRC/seed/interval, handles
explicit multi-match selection, compares generation rather than validation
frames and reports only the first observed rolling-checkpoint difference.
Omarchy SSH access now works. An isolated native PC diagnostic lab contains a
clone of engine checkpoint `78207e6` plus the build-compatibility fixes in PLAN-025,
a separate copy of Steam game data and user-local build tools. Its launcher
has passed asset-free tests on macOS and
Omarchy and redirects engine configuration, saves and diagnostics to the lab.
**Native PC build is complete and staged.** Executable SHA-256:
`9bad0faa0c075f4f3de71c63ab8615806818a9f39fbd70bc8558599f5cb6597f`.
Project libraries are staged beside it; `ldd -r` reports no missing libraries
or unresolved symbols, with FFmpeg supplied by the current host. Final Android
native regression compilation and 788 workspace checks per LAN-gate setting
pass; the installed 10214 APK is unchanged. **Graphical PC startup and a paired
match are now verified, but LAN still fails synchronization.** In the first
Quest-hosted match against this native Omarchy build, the PC received both
players' CRCs at validation frame 105 and reported `different_crc`:
Quest/slot 0 `E2E3DF5F`, PC/slot 1 `A6E913D4`. The PC generated its own
frame-0 CRC `3263A8D7` and frame-100 CRC `A6E913D4`; map CRC `DEA9E8E4`,
seed `4042777`, interval 100. Quest USB ADB was reconnected and both current
and previous XR stderr logs were preserved. The paired comparator confirms
identical frame-0 CRC and all rolling checkpoints (`3263A8D7`). At frame 100
the first observed difference is already in the object-list checkpoint:
Quest `6AE75FBB`, PC `03972538`; the RNG seed checksum still agrees
(`A82FF014`). Both sides report `different_crc` at validation frame 105.
This localizes the first recorded divergence to object state by frame 100,
not to a particular object, tick or root cause. Omarchy's displayed
`100.123.209.83` was its Tailscale address;
the isolated diagnostic profile now pins both LAN and online interface choices
to WLAN `192.168.178.158`. The match did start, so this mismatch is not a
Direct-Connect reachability failure. No original Steam files or Proton
settings were changed. The two remaining
base Generals INI/Patch archive hashes also match Quest (details in PLAN-025).

**Resumed on 2026-09-21 with a bounded per-object diagnostic pair.** Branch
`codex/lan-object-crc-trace` records object traversal order, stable ID and the
rolling production CRC after each object at the existing first eight normal
checkpoints, capped at 2048 objects with explicit count/truncation metadata. It
adds no second scan, CRC input, cadence, random draw or network field. The
paired comparator validates complete records and now passes 14 fixtures.
Signed Quest diagnostic 10231 (`1.2.31-lan-object-trace`), SHA-256
`41b24487ef54540d6890f34285d72752adc8b9357657eca24863f989caddcea2`,
is update-installed with retained data and its device APK hash matches. The
Omarchy lab has the matching observer over its required Linux build-compatibility
delta; staged executable SHA-256 is
`a403f82f9ddf7e8a5e9de6bb3b05699c2098df5736ad886299139f57e01bf48f`,
with its previous executable preserved. Native link and `ldd -r` checks pass.
The first 10231 physical capture is conclusive at object granularity. On stock
map CRC `DEA9E8E4`, seed `24456948`, both peers match fully at frame 0. At frame
100 each has 223 objects in the same order and matching RNG seed checksum, but
the first object (ID `000000DF`, order 0) ends with Quest CRC `AA85737D` versus
Omarchy `2AC9737D`; final CRCs are `0602409A` versus `55929D3A`. Both detectors
report `different_crc` at validation frame 105. A missing CRC message, differing
object count/order or later-only subsystem is not the immediate cause.

The installed follow-up is Quest 10232 (`1.2.32-lan-field-trace`), SHA-256
`8f9c348a9c20a4327d07922fcc4d80c0d62c360d139fcd0182e46b344a0a09e9`;
the on-device hash matches. It reads the starting and existing major field CRC
boundaries for only order 0 and names its template, without a second object CRC
call or simulation/network write. Omarchy executable SHA-256 is
`6e9d715019674f5d0b837fbd16c24f6cade6278d4a65283ff538499abf4d68a8`;
the 10231 executable is preserved. Seventeen comparator fixtures and all focused
and workspace tests pass. Public release 1.2.28 and `main` remain unchanged.

The 10232 physical pair is complete. Frame 0 agrees fully. At frame 100, both
peers enter the same first object (ID `000000DF`, template
`TrainCabUngarrisonable`) with the same CRC and remain equal through private
status, then first differ at its transform: Quest `E46FFD73`, Omarchy
`2770FDF3`. Counts/order and RNG seed CRC agree, and both peers detect the
resulting mismatch at validation frame 105. This points to train
position/orientation calculation, not LAN delivery, but does not yet identify
the responsible matrix component or arithmetic operation.

The immediate physical gate is one more same-source idle match using a bounded
raw 12-word transform observer for that already-selected first object. Use the
result to isolate railroad translation, rotation or height math; do not disable
CRC checking or remove/freeze the train. Only a demonstrated deterministic fix
followed by idle and interactive same-source passes allows returning to
Steam/Proton. Public 1.2.28 and `main` remain unchanged.

The 10233 raw-matrix test is complete. At frame 100 the first differing word is
rotation `m01`: Quest `BF6050E9` versus Omarchy `BF6050E6` (about 1.79e-7),
followed by symmetric `m10` and a one-ULP X-position difference; Y/Z agree.
Frame 0 is fully equal. Both builds already use `-ffp-contract=off`; the train
path still uses platform CRT `atan2` and `sinf`/`cosf`. The deterministic-math
option is presently only a TODO-backed CRT fallback and must not be presented
as a solution.

The prior observer was deployed as private Quest 10233
(`1.2.33-lan-transform-trace`), APK SHA-256
`795cb998be8e6a8530581f36fa882468d0022742ee5a8de372683be7c63bc036`.
It update-installed with retained app data and matching device hash. Omarchy's
matching staged executable SHA-256 is
`5f8ce7ec91812bae805fe0ad27bf9c4ae87bb3d53546d074ff702a5fb247f9e1`;
10232 is preserved as `runtime/GeneralsXZH.pre-transform-trace` and `ldd -r`
is clean. Its successor is now deployed as private Quest 10234
(`1.2.34-lan-railroad-trace`), APK SHA-256
`39bc84d7e4131f18f09548e3e7edef47ec652bade2ab877f3feb3cec78620642`,
with embedded `libmain.so` SHA-256
`67a3739a830c6484f041f4db50c3de4af1112531a7aa5e156b22727dea3bc720`.
It retains app data and adds only the railroad-intermediate trace bounded to
frames 0-105. Omarchy's matching executable SHA-256 is
`4c6cbdb95d83b5ad802c38192b236c4239adb6ba382bb4833d521eae46b17498`;
10233 is preserved as `runtime/GeneralsXZH.pre-railroad-trace`. Run the same
stock-map, no-AI, no-order match once; no settings or assets need to be
reimported.
The 10234 result identifies the initiating operation. At frame 1, object
`0000000D` enters the railroad angle calculation with every logged input
bit-identical on Quest and Omarchy. Native `atan2` alone returns Quest
`BF5788E6` versus Omarchy `BF5788E7`; the difference propagates from frame 2.
The experimental branch is therefore wiring the pinned GameMath software path
behind the existing opt-in `SAGE_USE_DETERMINISTIC_MATH` switch and routing the
railroad angle/matrix rotation through it. This must first pass another paired
same-source run. It is not yet a Steam retail compatibility result, and public
1.2.28 remains unchanged.
That pair is now deployed as private Quest build 10235
(`1.2.35-lan-deterministic-math`) and the matching staged Omarchy executable.
Quest APK SHA-256 is
`f871b075ea9ffe2e5cd2c6e78fc4f4cbcb1cc069a0f8bd1601b1639b3a023454`;
embedded `libmain.so` SHA-256 is
`f160d56d268398e7de4c9f62d2d9ac55e8d81d852cbb174844597a15bf5e6259`;
Omarchy executable SHA-256 is
`d11817b4be3207dfe5c73629694311605ddbbc8270885d44ea6aa8db67a0e38c`.
App data and the 10234 Omarchy rollback binary are retained. The idle gate has
now passed all eight sampled checkpoints without mismatch. In the follow-up,
both players built one building: both peers still agree through frame 300 but
first differ in the object stage at frame 400. Object count/order and RNG seed
agree; the first unequal rolling object is existing ID `000000D3`, order 14.
The next private build retargets the bounded per-field observer to that object
for one repeat of the same construction action. Public 1.2.28 and `main` remain
untouched; LAN is still experimental and unsupported.
That diagnostic is now installed/staged as 10236
(`1.2.36-lan-object-d3-trace`): Quest APK SHA-256
`44ae069a4957b5f000d5b67e5ba566a0bb7b113ce0ff6432b5f50f94a71382b3`,
embedded `libmain.so`
`ab0a695ddad20efefee39248ee007af76eb13d81dcbf615b783ecc9224a5ac0d`,
and Omarchy executable
`25da958cae8ad18a8065a6ca840f41f7284f95da03a48764b7726e4c3338833a`.
Multiplayer work is paused at the user's request. Do not ask for the pending
10236 single-object repetition on resume. First implement the universal,
bounded desync snapshot described in PLAN-025 so one representative match can
diagnose all captured objects/field boundaries and synchronized command
context without per-event rebuilds. Quest-to-Quest with two copies of the
public 1.2.28 APK is plausible because architecture, binary and game-data basis
can be identical, but it remains untested and unsupported; the public build
also predates the corrected CRC-slot validation and deterministic-math work.

The user's primary goal is Quest versus the
unmodified Steam PC version, with Quest peers retained. The custom PC build
is a diagnostic tool, not a replacement compatibility promise. A no-popup
match on old 10213 is NOT proof, since both Quest peers could skip checks.
Do not merge or publish LAN as supported; keep P23 separate. PLAN-025 records
the sanitized trace evidence and next test boundaries.

### P22 - deferred keyboard and mouse investigation

Audit Quest Bluetooth/USB keyboard and mouse behavior only as a later secondary
input path. The user explicitly moved this behind the current tabletop visual,
content, campaign and controller-quality work on 2026-09-16. Controllers remain
primary; P22 is not the next implementation candidate after P20.1.

### P23 - controller command usability and completeness

The source audit found that the persistent Commands window plus the original
context-sensitive build/control window already cover most useful offline
commands. P23 therefore improves and accepts those paths instead of creating a
second command system. On 2026-09-16 the first confirmed usability defects were
addressed locally: the native waypoint state now stays active for the complete
XR plotting session so its route is visible, and group extension is reduced to
select additional units → **New / Extend** → number. An empty slot creates a
new group; an occupied one extends it. The old reverse operation
(`Add group to current selection`, followed by another save) is no longer the
primary XR workflow. The full ARM64 native build and XR APK packaging pass; the
132 MiB candidate reports package `com.generalsx.zerohour.xr`, version 10209,
verifies with APK Signature Scheme v2 and has SHA-256
`24d2b1f8a9a1d95ac98b90cbffd596489b44a2b72feecaa4ef73c460b93e1f17`.
The user reports that this candidate runs well in the Quest and confirms that
group creation via the former Extend button works. They requested a clearer
**New / Extend** label and an optional XR resolution above High. The follow-up
source change retains Balanced as default, cycles Balanced → High → Ultra+,
preserves v10 spatial poses during v11 preference migration, and updates the
DE/EN help. The optional Ultra+ target is 2304 pixels wide per eye at the
runtime aspect (bounded to 2560 per dimension); it is not the original game's
separate "Ultra" graphics setting. The ARM64 native build, focused host checks
and signed XR APK build pass. PR #5 merged at
`ba9169d5c81604aa9fac00508cf2e5dcbf9a4939`. The current APK is published
as private prerelease `xr-preview-2026-09-16-p23`, version 10209, SHA-256
`978c627e276ab1627d014f68a8e1ad436ecaae215e8946ef900e5b999077273f`.
GitHub reports the uploaded `Generals-Zero-Hour-XR.apk` as 138,815,473 bytes
with that same SHA-256 digest. The user reports extensive play with good
readability/visibility and confirms waypoint plotting. No device-side APK hash
or sustained Ultra+ busy-campaign frame-time capture has been supplied. The
earlier hash above is the previously accepted P23 candidate, not this release.

The user considers the important everyday commands covered. Defer the remaining
P23 edge-command audit (formation/force-move/guard variants and faction-specific
coverage). Extensive play has not exposed a pressing visual/readability problem,
so do not schedule another generic graphics QA pass as the next feature. The
active next slice is QTR-MP Quest ↔ Windows Steam LAN validation: build an
opt-in Quest candidate, attempt lobby and sustained human match, then use a
same-source Windows GeneralsX build only if needed to isolate retail mismatch.
Do not infer human-multiplayer or replay compatibility from offline AI Skirmish.
Keep P22 keyboard/mouse secondary; revisit Ultra+ performance only if a real
campaign scene shows a regression.

## P21 key implementation map

| File | Responsibility |
|---|---|
| `android/app/src/main/java/com/generalsx/zerohour/XrPanelPainter.java` | Android Canvas artwork, typography, buttons and panel pixels; `paint2` renders primitives from the shared native control table and owns no geometry |
| `GeneralsMD/Code/Main/XrPanelLayout.h` | Single layout source: control tables, roles/states, shared hit test, JNI packing (new in P21) |
| `GeneralsMD/Code/Main/XrMenuPainting.h` | Dynamic UI/Commands text, status, labels, per-control states and texture invalidation keys |
| `GeneralsMD/Code/Main/XrMenu.h` | Workspace panel dimensions and hit geometry |
| `GeneralsMD/Code/Main/XrMenuUI.h` | Workspace-menu routing, placement and actions |
| `GeneralsMD/Code/Main/XrCommands.h` | Commands panel dimensions, hit geometry and action mapping |
| `GeneralsMD/Code/Main/XrCommandUI.h` | Commands placement, visibility and input dispatch |
| `GeneralsMD/Code/Main/XrGameBoot.h` / `XrGameBoot.cpp` | Unchanged command semantics plus read-only `XrGameBoot_TacticalState` for persistent panel states |
| `GeneralsMD/Code/Main/XrStrings.h` | German/English XR strings |
| `GeneralsMD/Code/Main/XrControllerHelp.h` | Controller and command guidance |
| `GeneralsMD/Code/Main/XrHello.cpp` | Panel texture upload and rendering integration |
| `scripts/qa/xr-panel-text-test.*` | Text/label coverage |
| `scripts/qa/xr-menu-test.cpp` and `xr-menu-routing-test.cpp` | Menu geometry and routing |
| `scripts/qa/xr-console-bridge-test.*` | Group, command and Communicator bridge coverage |

## P20.1 key implementation map

| File | Responsibility |
|---|---|
| `GeneralsMD/Code/Main/XrBoardMesh.h` | Physical soil edge, lower plinth and underside mesh |
| `GeneralsMD/Code/Main/XrSceneSurface.h` | Real-surface clearance based on plinth underside |
| `GeneralsMD/Code/Main/XrMenu.h` | Editing outline depth for the physical board |
| `scripts/qa/xr-board-test.cpp` | Board mesh geometry checks |
| `scripts/qa/xr-height-test.cpp` | Shared height/plinth/render-ray contract |
| `scripts/qa/xr-scene-test.*` | Surface-fit and placement-clearance checks |

## Known boundaries and risks

- Meta room recognition uses stored scene data, not live object recognition. A
  cluttered or stale room scan can omit a real table; floor/manual placement is
  the supported fallback.
- No persistent spatial anchor binds the board to furniture across restarts.
- High resolution and original volume shadows can reduce campaign performance.
  Do not change the accepted Balanced/light-shadow defaults without same-scene
  device evidence.
- Canvas pixels, panel render geometry and ray hit geometry are separate pieces
  of code. P21 is incomplete unless all three stay aligned. Since the P21
  implementation, one shared native control table (`XrPanelLayout.h`) drives
  Canvas pixels (via `paint2`), UV hit regions and state rendering, so the
  three can no longer drift apart silently.
- The user's room photographs are useful visual references but contain private
  surroundings. Do not add them to GitHub. Use cropped/redacted panel captures
  or synthetic Canvas fixtures for a pull request.

## Minimum verification before handoff

Run the focused host tests for the changed panel or board geometry, then the
existing XR regression scripts named in `scripts/README.md`. Build both flavors:

```sh
JAVA_HOME='/Applications/Android Studio.app/Contents/jbr/Contents/Home' \
  ./gradlew :app:assembleXrDebug :app:assembleZhDebug
```

For a Quest candidate, verify the APK signature, record SHA-256, install with an
explicit serial and `adb install -r`, and preserve application data. Automated
success does not close the worn-headset gate. The pull request must state which
of these were run and which were not.

## Session close-out checklist

- Update this dashboard only for changed current facts or priorities.
- Add a concise newest-first entry to `docs/DEV_BLOG/2026-09-DIARY.md`.
- Update the active plan's checklist and acceptance status.
- Run `git diff --check` and the relevant focused tests.
- Commit on the delegated branch; do not leave the handoff as an unnamed dirty
  working tree.
- Report start commit, end commit, test commands/results, APK hash and remaining
  physical gates.
