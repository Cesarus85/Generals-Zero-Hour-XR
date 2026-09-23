# PLAN-024: Quest UI and Commands Window Presentation

**Milestone:** P21  
**Status:** Implemented, accepted in-headset, merged and released as `v1.2.8-xr-preview` (2026-09-16)
**Target:** Meta Quest 3  
**Runtime:** Native OpenXR plus Android Canvas panel textures  
**Primary languages:** English and German

## 1. Outcome

Turn the functional spatial `UI` and `Commands` windows into a coherent,
attractive and fast-to-read part of the tabletop experience. The result should
feel like a deliberate Generals command interface adapted to XR, not a diagnostic
grid of equally weighted buttons.

This is a presentation, information-architecture and ergonomics milestone. It
must preserve the existing simulation, command dispatch, controller roles,
saved layout, campaign behavior and future multiplayer compatibility.

## 2. Starting point

The current implementation already provides:

- a compact `UI` entry button and an on-demand spatial settings window;
- four main settings pages plus controller help and play-space setup;
- a persistent `Commands` entry button and nonmodal command console;
- sixteen direct orders, ten control groups, group operations, Communicator,
  help, tactics, camera bookmarks and disabled-action reasons;
- laser hover, persistent manipulation-target marking, localized German/English
  labels, left-handed roles and matching ray hit geometry;
- freely positioned/tilted build window and a commands console that stays
  gravity-upright while docking beside it.

The main shortcomings are visual hierarchy, dense text, inconsistent emphasis,
large uniform button fields, weak contextual grouping and fixed layouts that can
either waste space or truncate information. The settings window and commands
console work, but they do not yet look or read like finished game UI.

## 3. Design direction

- Use a restrained, dark command-surface palette with warm metal or amber
  selection accents and limited cyan for ray hover. Preserve strong contrast.
- Echo the original Generals control-bar language without copying retail assets
  into the repository or making text look like a low-resolution desktop crop.
- Prefer clear section cards, dividers, icons made from code/native shapes and
  purposeful negative space over ornamental clutter.
- Keep a visible hierarchy: window title, live context/status, primary actions,
  secondary tools, help/close controls.
- Avoid transparent text over the battlefield. Panels need a stable readable
  background and a subtle spatial edge/depth treatment.
- Do not rely on color alone. Selection and armed modes also need shape, border,
  icon or short textual confirmation.

## 4. Workspace `UI` window

Organize controls by player intent rather than implementation history:

1. **Table and view:** choose the current edit target, change visible map
   coverage, reset the selected surface and enter direct arrangement.
2. **Windows:** size, distance, height, tilt and yaw for table/build surfaces,
   with the selected target unmistakable in the panel and in room space.
3. **Play space:** free-standing, detected table/floor and manual-height setup,
   with the existing sequential wizard retained.
4. **Graphics:** Balanced/High resolution, light/original shadows, Multiview
   state, extra-world-copy state and diagnostic measurement.
5. **Preferences and help:** handedness, language and controller guide.

Requirements:

- Replace the visually uniform 18-button grid with grouped sections and primary/
  secondary emphasis while keeping frequent adjustments directly reachable.
- Show the current value or state next to each setting instead of requiring the
  player to infer it from a long status line.
- Keep `Done`/close persistent and easy to find.
- Do not turn frequent adjustments into deep nested menus.
- Preserve every existing action and the play-space wizard. A new layout may
  change action IDs only if rendering, hit testing and tests change together.
- Size the visible panel to its active page where practical. No page may be
  clipped and no normal page should be dominated by unused grey space.

## 5. `Commands` window

The console must answer three questions immediately:

1. What is selected?
2. What can it do now?
3. Is the player choosing a command, a target, a group operation or a camera
   bookmark?

Requirements:

- Put selection/tactical status at the top in a concise, readable context block.
- Group direct orders by behavior: movement, combat, guard/formation, selection
  utilities and immediate actions.
- Make targeted orders visually different from immediate actions such as Stop.
- Give the armed order a persistent, non-color-only state until target selection
  or cancellation.
- Separate control-group recall from `Save`, `Add` and `Center`. Show the stored
  member count without crowding the number itself.
- Keep Communicator, help and advanced tactics available without competing with
  the primary combat actions.
- Advanced tactics may expand the window, but the top anchor and existing
  controls must not jump. Help may replace the content in place.
- Disabled actions remain visible when useful and expose the existing reason.
- Do not change the native action mapping, control-group meaning, camera bookmark
  behavior or Communicator invocation.

## 6. State model

Define and render these states consistently across both windows:

| State | Required presentation |
|---|---|
| Idle | Neutral surface and ordinary label |
| Ray hover | Cyan or cool highlight plus clear boundary |
| Selected edit target | Amber/orange outline and check/target marker |
| Toggle enabled | Persistent filled/accented state with value text |
| Command armed | Strong persistent command state and target instruction |
| Group operation pending | Operation name remains visible until number/cancel |
| Disabled | Reduced emphasis but readable label; reason remains discoverable |
| Destructive/replace operation | Explicit warning treatment before group overwrite where practical |
| Focus/tracking lost | No stale hover, press or armed visual that can dispatch later |

## 7. Geometry and input invariants

The same logical layout must drive:

- Android Canvas drawing in `XrPanelPainter.java`;
- texture dimensions and panel aspect used by native rendering;
- UV hit regions in `XrMenu.h` and `XrCommands.h`;
- spatial panel sizing and placement in `XrMenuUI.h` and `XrCommandUI.h`.

Do not fix visual placement without fixing hit placement. Every visible control
must have exactly one matching hit region; gaps must not activate neighboring
buttons. Panel input remains modal for the settings window and nonmodal for the
commands console. No press may click through into the battlefield.

Preserve:

- the default board/build/commands arrangement;
- the commands console's gravity-upright orientation and inward yaw;
- manual move, scale and free tilt where currently supported;
- left/right controller role swapping;
- comfortable minimum hit targets at the normal seated distance;
- Back/System behavior and neutral-controller rearming after mode changes.

## 8. Localization and accessibility

- Validate English and German independently; never design only around the shorter
  English labels.
- Do not silently ellipsize the only explanation of a command. Use a short label
  plus contextual description when necessary.
- Preserve the current game-language distinction: XR language changes do not
  invent missing retail voice/text assets.
- Use sufficiently large type, high contrast and more than color to communicate
  selection, toggles and disabled states.
- Retain controller-help access from shell and gameplay.

## 9. Performance budget

- No per-eye or per-frame Canvas repaint. Repaint only when content/state keys
  change, as today.
- Avoid new texture uploads for pointer motion unless the visible hover state
  actually changes.
- Keep panel texture memory bounded. If page-specific dimensions are introduced,
  cache/reuse sizes and update the native upload contract explicitly.
- Do not add a WebView, Unity runtime, extra compositor layer dependency or a
  second scene traversal.
- The accepted Balanced resolution, light shadows, Multiview and automatic world
  copy omission defaults remain unchanged.
- A representative live-game comparison must show no measurable sustained frame-
  rate regression attributable to the panel pass.

## 10. Non-goals and protected behavior

P21 must not:

- change simulation commands, game rules, unit AI or network/replay wire formats;
- claim LAN/internet multiplayer safety from offline Skirmish testing;
- redesign the original build/production control bar;
- implement P20 safe-room startup, P20.1 plinth thickness or P22 keyboard/mouse;
- add copyrighted retail artwork or the user's uncropped room photographs;
- reset saved layout, language, handedness or graphics preferences;
- remove an existing command merely to simplify the visual layout.

## 11. Implementation map

Primary files:

- `android/app/src/main/java/com/generalsx/zerohour/XrPanelPainter.java`
- `GeneralsMD/Code/Main/XrMenuPainting.h`
- `GeneralsMD/Code/Main/XrMenu.h`
- `GeneralsMD/Code/Main/XrMenuUI.h`
- `GeneralsMD/Code/Main/XrCommands.h`
- `GeneralsMD/Code/Main/XrCommandUI.h`
- `GeneralsMD/Code/Main/XrStrings.h`
- `GeneralsMD/Code/Main/XrControllerHelp.h`
- `GeneralsMD/Code/Main/XrHello.cpp`

Focused regression entry points:

- `scripts/qa/xr-panel-text-test.sh`
- `scripts/qa/xr-console-bridge-test.sh`
- `scripts/qa/xr-build-controls-test.sh`
- `scripts/qa/xr-scene-test.sh`
- `scripts/qa/xr-menu-test.cpp`
- `scripts/qa/xr-menu-routing-test.cpp`
- `scripts/qa/xr-workspace-test.sh`
- `scripts/qa/xr-input-test.cpp`

## 12. Recommended implementation sequence

1. Capture or generate privacy-safe before images of every UI/Commands page in
   English and German. Record current hit geometry and texture sizes.
2. Produce a compact wireframe for both windows and map every existing action to
   exactly one proposed control. Review this mapping before code changes.
3. Introduce shared layout constants/data so Canvas drawing and native hit testing
   cannot drift silently. Add geometry tests before changing artwork.
4. Implement the workspace UI hierarchy and states, then its hit regions.
5. Implement the Commands hierarchy, armed/group/disabled states and adaptive
   tactics/help presentation, then its hit regions.
6. Run host/Canvas fixtures for both languages and all states. Inspect generated
   images at native texture resolution and at representative angular size.
7. Build both Android flavors. Install the XR APK with data preserved and perform
   the headset matrix below.
8. Update `XR_CURRENT_STATUS.md`, PLAN-023's milestone record and the development
   diary. Open a pull request with before/after images and remaining gates.

## 13. Acceptance matrix

### Static and automated

- Every old action is mapped and reachable.
- Canvas control bounds and native hit bounds match.
- English and German labels fit or use an intentional detail treatment.
- Idle, hover, selected, enabled, armed, pending and disabled states are covered.
- No blank oversized region or clipped expanded page remains.
- Existing XR input, scene, commands, group and localization tests pass.
- Both `assembleXrDebug` and `assembleZhDebug` pass; `git diff --check` is clean.

### Worn headset

- Default seated layout: both windows are readable without leaning or raising the
  arm excessively and do not obscure the tabletop or build window.
- Standing and seated checks retain comfortable facing and spatial placement.
- Laser hover and actual trigger activation agree at every corner and row.
- UI target selection is obvious both in the window and around the edited object.
- Select multiple units; issue Move, Attack Move, Guard, Force Attack, Stop and
  Scatter; cancel every targeted mode once.
- Save, recall, add to and center a control group; verify clear pending feedback.
- Open tactics, use a camera bookmark, open help and invoke Communicator.
- Switch German/English and right/left-handed modes.
- Move/tilt the build window and verify Commands remains upright and readable.
- Open play-space setup, native game menu, general-powers tree and a campaign
  video transition; return without stale/click-through input.
- Compare a representative battle before/after with the accepted graphics defaults.

## 14. Definition of done

P21 is complete only when:

- code, Canvas artwork and hit geometry are reviewed together;
- all existing actions and protected behavior remain available;
- automated checks and both APK builds pass;
- a Quest candidate and hash are recorded;
- the user accepts the visual hierarchy, readability and interaction in the
  headset;
- the status dashboard, development diary and pull request state exactly which
  physical checks passed and which remain open.

Build success, screenshots or a flat Canvas fixture alone do not constitute
worn-headset acceptance.

## 15. Implementation record (2026-09-15)

Branch `claude/p21-ui-command-windows`, starting from `f27d450`.

- New `GeneralsMD/Code/Main/XrPanelLayout.h` is the single layout source:
  control tables for the Commands console (compact/tactics-foldout/help),
  the workspace window (pages 0-3 plus shared help geometry) and the
  controller guide; roles/states, one shared UV hit test and the 8-int JNI
  packing consumed by Java `paint2`. Java owns no geometry anymore.
- `XrCommands.h` / `XrMenu.h` hit testing reads the shared table; action IDs
  and `applyMenuAction` / `applyCommandAction` semantics are unchanged.
- `XrMenuPainting.h` builds per-control labels and states (armed order via
  read-only `XrGameBoot_TacticalState`, waypoint/group pending, toggles,
  disabled tactics, edit-target selection, hover) and repaints only when the
  content/state key changes. Milestone tags (`P11.1`, `P19.1`) were removed
  from visible panel text.
- `XrPanelPainter.java` keeps legacy `paint` for kinds 0/2/6/7 and renders
  kinds 1/3/4/5 from the shared table with section headers, context cards,
  chevron/armed markers, value chips, count badges and shrink-to-fit labels
  instead of silent ellipsis.
- Tests moved to the new contract in the same change: table-driven geometry
  (`xr-menu-test.cpp`), new hit coordinates (`xr-menu-routing-test.cpp`),
  structured `paint2` capture validation in both languages
  (`xr-panel-text-test.cpp`), wizard fixture title (`xr-scene-test.cpp`).
- Host results: panel-text 18625, menu 2831, menu-routing 317,
  console-bridge 1404, scene 1280, workspace 565, build-controls 4292,
  input 27, tactics 2030, board 64, height 272818, math 111, camera 43,
  comfort 260, interaction 109 — all PASS. `git diff --check` clean.
- Both APK flavors, signature, dependency closure and branding checks pass on
  the integrated candidate. Version 1.2.7-base-navigation (10207), SHA-256
  `cb05a9fc3d49c089484cfc8a06ffa2e8c00198d935bfe2bfce0b547ddfe47844`,
  was installed with preserved data on Quest 3.
- The user accepted the visual redesign, compact default arrangement, explicit
  whole-workspace alignment, session-stable Skirmish/campaign transitions and
  home-base navigation in the headset. Exhaustive combinations in §13 remain a
  regression matrix, not a blocker for the accepted P21 presentation merge.
- Publication complete: PR #2 merged as `c01ad1d`, PR #1 merged as `fc37591`,
  the artifact-retention correction is `29fbcb9`, and Android release CI
  `35032355387` published `v1.2.8-xr-preview`. Stable APK SHA-256:
  `0763b01dd0dfccfc99666a1a4dd614fdc0dde41a2f72282db9258eeee0927660`.
  Installation of final version 10208 is deliberately deferred to the next
  Quest session; headset acceptance above refers to the content-equivalent
  installed 10207 candidate.

## 16. Visual follow-up: PR #40 (2026-09-23)

PR #40 repaints the existing shared-table UI and Commands panels as a military
command console: chamfered gunmetal/olive surfaces, brass frames and rivets,
stencil-style headers and clearer active/hover/state accents. It changes only
Android Canvas rendering in `XrPanelPainter.java`; native control tables, ray
hit geometry, panel placement and command actions remain unchanged.

The repaint is integrated on `codex/xr-keyboard-pr40` together with the
separately scoped native Quest system-keyboard bridge. Pixels outside the three
chamfered board-side shortcuts are transparent, removing their rectangular
black quads without changing physical hit surfaces. Host panel/geometry/routing
checks, the full ARM64 native build and release-signed test APK assembly pass.
Final visual readability, shortcut transparency, state recognition and native
system-keyboard text entry still require a worn-headset check before merge.
