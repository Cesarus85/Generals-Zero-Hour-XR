# Generals: Zero Hour XR - Current Handoff Status

**Updated:** 2026-09-15
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
4. `PLAN-023_QUEST_TABLETOP_RECOVERY.md` for detailed architecture, decisions,
   dated implementation evidence and rollback boundaries.
5. The newest entries in `docs/DEV_BLOG/2026-09-DIARY.md` for the latest delta.
6. The actual branch, diff and test output. A document never overrides code.

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
| Commands console | Persistent spatial console with direct orders, groups, tactics, camera bookmarks, Communicator and in-place help, redesigned into grouped sections with persistent armed/pending/toggle/disabled states | Implemented on `claude/p21-ui-command-windows`; all host tests pass; worn-headset acceptance open |
| Workspace UI | Spatial settings window for table/build manipulation, graphics, handedness, language, help and play-space setup, regrouped into intent sections with value chips | Implemented on `claude/p21-ui-command-windows`; all host tests pass; worn-headset acceptance open |
| Build window | Original production/build UI detached above the tabletop and independently movable, scalable and tiltable | User accepted current arrangement and interaction |
| Localization | XR interface and help support German and English; initial choice follows German OS, otherwise English | Resource/host checks pass in both languages, including long German labels with shrink-to-fit rendering; device use remains a physical gate |
| Play-space setup | Optional free board, detected table/floor and manual-height workflows; no required room binding; unanchored launches use safe HMD-relative geometry | P19.1 user accepted; P20 source/host validation complete, renewed headset acceptance open |
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

## Last reproducible device artifact

The newest installed local Quest candidate combines P21, P20 and PR #2's
reproducible DXVK gitlink/CI foundation:

```text
version 1.2.6-p20-alignment (10206)
source follow-up 5de20bd; integration commit 922b6cd
build/apk/Generals-Zero-Hour-XR.apk
SHA-256 84f383e69212ad7e16339932cd256cf1e3158db63126e567da30218eb87ab5d8
```

Native build and both `zh`/`xr` APKs pass; the XR v2 signature, packaged native
dependency closure, staged-versus-packaged libmain match and 89 branding checks
pass. Installation on Quest 3 `2G0YC5ZG9609PY` used `adb install -r` without
clearing user data; package inspection confirms 10206. A launch was requested;
this is not a verified worn-headset gameplay boot. This contains P20.3 and
supersedes 10205. Worn-headset stability is still
open. The integration commit is local-only and must not replace the PR branches
as source authority. PR #2 CI run `35007254486` failed from runner disk exhaustion
during native compilation; neither that CI gate nor the PR #1 merge is complete.

## Immediate implementation queue

### Navigation follow-up - base shortcut

Implemented after P20.3: support-hand stick edge dispatches the original local
`MSG_META_VIEW_COMMAND_CENTER`. No simulated keyboard events, new unit orders,
simulation/network changes or multiplayer eligibility expansion. Host tests:
interaction 135, handedness 264, console bridge 1432, bilingual panel payloads
19593 pass. New APK/device check pending; 10206 above predates this shortcut.

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
pass. The 10206 APK is built and installed as above; renewed physical acceptance
is pending. Test looking sideways during loading, the direct UI button, and
leave/cancel on a confirmed surface. PRs remain unmerged.

### P20.1 - thinner tabletop underbody

Reduce the visible grey/dark lower plinth to no more than half its current
thickness. The current mesh places the soil/base transition at `-0.018` board
widths and the underside at `-0.036`; the lower trim is therefore `0.018` board
widths thick. Target at most `0.009` for that lower trim while leaving the table
top, terrain registration and apparent terrain-cut depth unchanged. Update
surface-placement clearance, editing outlines and numerical tests to use the
same new underside. This is a near-term visual geometry task, separate from P21.

### P21 - UI and Commands window presentation

Rework both spatial windows into a coherent, attractive and quickly readable
Generals-inspired interface without changing command semantics. The complete
delegation contract is `PLAN-024_QUEST_UI_COMMAND_WINDOWS.md`.

Implementation (2026-09-15, `claude/p21-ui-command-windows`) is complete and
host-verified; the visual redesign was accepted in the headset. P20 was then
added as a focused startup-safety follow-up on the same unmerged branch. CI APK
builds and P20 worn-headset acceptance remain open. No P20.1 or P22 changes are
included.

### P22 - keyboard and mouse investigation

Audit Quest Bluetooth/USB keyboard and mouse behavior as a secondary input path.
Controllers remain primary; do not let this delay controller, campaign or UI
quality work.

### QTR-MP - retained multiplayer and replay milestone

Offline AI Skirmish is the primary supported test mode but is not evidence for
human networking. LAN/internet matches, deterministic synchronization, reconnect,
chat/team chat, diplomacy/player controls, observer/replay access and their XR
interaction need a separately authorized compatibility and headset-validation
slice. P20-P22 must not change simulation commands or wire formats casually.

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
