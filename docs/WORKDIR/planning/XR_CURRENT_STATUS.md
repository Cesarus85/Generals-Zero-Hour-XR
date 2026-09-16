# Generals: Zero Hour XR - Current Handoff Status

**Updated:** 2026-09-16
**Audience:** maintainers and coding agents continuing the Quest/XR work  
**Active target:** Meta Quest 3, native OpenXR with OpenGL ES 3  
**Product package:** `com.generalsx.zerohour.xr`

This is the short, maintained entry point for the XR branch. Read it before the
long implementation record. It distinguishes source completion, automated
verification, device installation and actual worn-headset acceptance; those are
not interchangeable.

## Read order and sources of truth

1. `AGENTS.md` for repository-wide engineering rules.
2. This file for the current XR baseline, open gates and next work.
3. `PLAN-024_QUEST_UI_COMMAND_WINDOWS.md` when working on P21.
4. `MULTIPLAYER_STATUS.md` for the paused QTR-MP evidence and exact resume
   sequence; `../audit/RELEASE_PREPARATION_XR.md` for first-release gates.
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

## Release and device baseline

The first release preparation is on `codex/xr-release-safe-build`, based
on `main` at `b515e75`. It preserves the offline P23 game engine and adds a
separate, non-debuggable XR release package path. A privately signed local
candidate is `1.2.15-xr-preview` (10215), SHA-256
`bafff443d77e7b9e925a73fcf16b5bf7234c54f256e2cb7fa0f95d1aad008d2b`;
certificate SHA-256
`a3774568b341adc8abaa1e4200014020e6e2b80ca77c8a66e12bfa7a4498018f`.
It update-installed successfully over the 10214 LAN diagnostic APK on the
Quest 3/API 34 without uninstalling; the original first-install timestamp
remained unchanged. The app is no longer debuggable. A clean ARM64/DXVK native
source build passes; its packaged `libmain.so` matches the newly built file,
and the installed APK SHA-256 matches the local final artifact above.
The maintainer confirmed worn-headset Skirmish and Campaign launch with this
**exact final hash**, with prior settings and game data retained. This is not a
fresh-import or all-missions test. An earlier session with a preliminary signed
repack was interrupted by our ADB installation of the final candidate, not a
proven app crash; avoid installing while the user plays.
The intended source-matched private release tag is `v1.2.15-xr-preview`;
the repository remains private. Verify the tag and uploaded asset digest
against the candidate above before treating the GitHub release as authoritative.
See the [release audit](../audit/RELEASE_PREPARATION_XR.md) for signing,
migration, artifact and publication gates. The tracked development key remains
for debug builds and old-signature lineage only.

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
so do not schedule another generic graphics QA pass as the next feature.
QTR-MP preflight ran on the separate `codex/quest-pc-lan-preflight` branch and
is now **paused** while the first offline release is prepared. Direct Connect
starts a Quest/PC match, but paired Quest/native-Linux diagnostic traces first
diverge in the object CRC by generation frame 100 and both peers report a
different-CRC error at validation frame 105. See
[`MULTIPLAYER_STATUS.md`](MULTIPLAYER_STATUS.md) for the preserved
evidence, the next bounded per-object diagnostic, and the remaining gates. No
LAN diagnostic APK belongs in the P23 offline release. Do not infer human
multiplayer or replay compatibility from offline AI Skirmish. Keep P22
keyboard/mouse secondary; revisit Ultra+ performance only if a real campaign
scene shows a regression.

The first offline release preparation is documented in
[`RELEASE_PREPARATION_XR.md`](../audit/RELEASE_PREPARATION_XR.md). The P23
asset/tag remains a historical debug-signed development preview; the 10215
privately signed candidate is the intended private-release checkpoint. Neither
the LAN diagnostic build nor the now headset-accepted 10215 package alone
establishes a public release.
Its clean source-rebuild and installed-hash gates pass; a private release
checkpoint, signing-key backup and public-visibility review remain separate.

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
