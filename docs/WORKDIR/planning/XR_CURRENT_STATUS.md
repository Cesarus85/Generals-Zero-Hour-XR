# Generals: Zero Hour XR - Current Handoff Status

**Updated:** 2026-09-14  
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
| Commands console | Persistent spatial console with direct orders, groups, tactics, camera bookmarks, Communicator and in-place help | Functional; visual and information-design redesign is P21 |
| Workspace UI | Spatial settings window for table/build manipulation, graphics, handedness, language, help and play-space setup | Functional; visual and information-design redesign is P21 |
| Build window | Original production/build UI detached above the tabletop and independently movable, scalable and tiltable | User accepted current arrangement and interaction |
| Localization | XR interface and help support German and English; initial choice follows German OS, otherwise English | Resource/host checks plus device use; long-label layout remains part of P21 |
| Play-space setup | Optional free board, detected table/floor and manual-height workflows; no required room binding | P19.1 user accepted as working; recognition depends on Meta room data |
| Game-data setup | Guided Steam or installed/extracted CD/ISO folder import with validation; raw images/installers are not extracted | Host and Quest instrumentation pass; real Steam-based use confirmed |
| Returning launch | Saved valid data is checked inside the XR Activity; setup opens only when data is missing or invalid | Device launch verified; final visual no-flash confirmation remains a physical gate |
| Performance defaults | Balanced resolution, light shadows, Multiview preferred, redundant extra world copy omitted automatically | Reported as relatively smooth and playable; no universal FPS guarantee |

## Current visual and interaction defaults

- A fresh compatible layout starts in tabletop mode.
- Board width: 1.65 m in the photo-inspired default layout.
- Detached build window: 1.8 m wide, above and behind the far board edge, with
  free tilt retained after release.
- Commands console: to the left of the build window, gravity-upright and yawed
  inward toward the player.
- Pointing-hand trigger selects, orders and draws a selection rectangle.
- Support-hand stick pans the map; pointing-hand stick rotates and zooms.
- Handedness swaps these roles. The in-game controller guide is authoritative
  for the complete mapping.
- The UI menu identifies the selected manipulation target in orange; cyan is
  reserved for laser hover.

## Last reproducible device artifact

The newest local Quest artifact containing the silent returning-launch fix is:

```text
build/quest-direct-start/Generals-Zero-Hour-XR.apk
SHA-256 1c6ed6f5e53b57613c7457d705900a5c4f9f32942fbe5f7639ac21e3351afaf9
```

Both XR and ordinary Android debug packages built and verified. Twenty-one host
game-data checks and twenty-one Quest instrumentation checks passed. Installation
used `adb install -r` without clearing user data. The native game library is
unchanged from the accepted gameplay/graphics baseline. See
`build/quest-direct-start/validation.md` in the development checkout; generated
build evidence is not part of the source repository.

## Immediate implementation queue

### P20 - safe fresh placement

If no play surface is confirmed for the current session, start with the board
and build window comfortably in front of the player instead of blindly applying
a world-relative pose saved in another room. Preserve the saved arrangement as
a recoverable preference; this is a safe session fallback, not a data reset.

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
| `android/app/src/main/java/com/generalsx/zerohour/XrPanelPainter.java` | Android Canvas artwork, typography, buttons and panel pixels |
| `GeneralsMD/Code/Main/XrMenuPainting.h` | Dynamic UI/Commands text, status, labels and texture invalidation keys |
| `GeneralsMD/Code/Main/XrMenu.h` | Workspace panel dimensions and hit geometry |
| `GeneralsMD/Code/Main/XrMenuUI.h` | Workspace-menu routing, placement and actions |
| `GeneralsMD/Code/Main/XrCommands.h` | Commands panel dimensions, hit geometry and action mapping |
| `GeneralsMD/Code/Main/XrCommandUI.h` | Commands placement, visibility and input dispatch |
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
  of code. P21 is incomplete unless all three stay aligned.
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
