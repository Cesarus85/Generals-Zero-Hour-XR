# PLAN-023: Quest Tabletop Recovery and Hybrid UI

## P11.1: reliable workspace exit, science tree and campaign video (2026-09-14)

Address the user's P11 defects without changing game rules, assets, window
defaults or multiplayer scope. Arrangement has a direct DE/EN Done button and
unconditional Back/A exit before UI capture. Science purchase uses a full UI
canvas. Blocking campaign/Challenge loading and repeated cinematic camera draws
yield fresh XR presentation without recursively advancing the engine. See
section 31 for evidence, rollback and the remaining physical campaign gate.

## P10.3: upright console, group guidance and DE/EN (2026-09-14)

User's P10.2 photo proves that inheriting build-panel pitch before inward yaw
also rolls/leans the command console. Keep docking position, but use gravity-up
and heading-only orientation. Include native diplomacy in full-canvas UI,
clarify native control-group semantics, add in-place help, and localize the XR
UI with a persisted German/English choice. See section 29 for scope, command
coverage and the separate original-game language-data requirement.

## P10.2: photo arrangement, sharper world, complete dialogs (2026-09-14)

Implement the user's photo as the new tabletop default: build bar above the
far table edge, slightly tilted; command console on the left, turned inward.
Start eligible offline matches in stereo. Expand native game menus only when
needed, retain compact building controls in ordinary play, and clarify the
second target click for guard/movement commands. See section 28 for evidence,
migration behavior, artifacts and physical acceptance.

## P10.1: input comfort and handedness (2026-09-14)

User tested P10 and reports that it initially feels very good; complete command
coverage is still open. Treat the following as one bounded input-quality step
before continuing graphics/content work: relaxed-arm object picking, useful
support-stick map navigation, and persisted left-handed controller roles.
Implementation and acceptance details are in section 27. P10's terrain-space
drag threshold below is historical and superseded by P10.1's ray-facing metric.

## P10 implementation contract: direct army interaction (2026-09-13)

User requires drag selection and direct access to special commands; P9's
two-corner/menu-first path is not the desired primary interaction.

1. Trigger click retains context selection/orders. Hold and move the terrain
   ray at least 2 cm in table space to start a live selection rectangle; release
   commits the army selection. No mode menu is required. Left grip at press
   means additive selection; clicking a selected army unit then removes it.
2. Armed construction/abilities and explicit targeted orders take precedence
   over automatic drag classification. No accidental movement command follows
   a box selection. Tracking loss, panel crossing, camera motion or cancel
   drops the gesture without an order and requires a fresh press.
3. A nonmodal persistent **Commands** console beside the build panel exposes
   commands and ten group buttons directly. A toggles it in live play; shell
   and explicit arrangement retain existing behavior. Its pose follows the
   freely tilted build window. Game clicks outside it remain available.
4. Group save/add/center use one-shot modifiers followed by a numbered group;
   ordinary group buttons recall immediately. Existing build/ability controls
   remain in the original command bar. UI settings are for workspace changes,
   not required to reach army commands. Persist console visibility in v5.
5. Verify production routing, threshold/cancel edge cases, console geometry,
   group mappings and migration on host/ARM64, build both APKs, preserve P9
   rollback and install on the explicitly identified Quest. In-game usability
   and all actual unit commands remain worn-headset acceptance gates.

Status: Accepted for implementation

Latest checkpoint (2026-09-14): P11.1 Locale native and both APK builds pass;
6842 UBSan C++ plus 51 Java host checks pass. The earlier focused 1442 Quest
ARM64 checks pass; they do not prove real locale/first-install acceptance.
P11.1 Locale is installed and hash-verified; P11 and the current v8 layout
are backed up. Sections 31–32 record startup, language and physical gates.
No live gameplay or worn-headset comfort is inferred from isolated fixtures.
Historical inspection findings below describe their dated checkpoint, not the
current implementation. See the P5/P5.1/P6/P7.0 entries for evidence and limitations.

Owner: GeneralsX Android / XR port

Target: Meta Quest 3 first, Android build parity preserved

Runtime decision: Native OpenXR with OpenGL ES 3

Last reviewed: 2026-09-14

## 1. Outcome

Turn the current XR proof of concept into a usable mixed-reality version of
Zero Hour in which:

- the battlefield is presented as a stable planar command table in the room;
- the player can freely move, rotate, and uniformly scale the board and each
  independently rendered UI panel, with optional grouping and horizontal snap;
- world-bound feedback remains registered with the battlefield;
- the control bar, radar, production/build UI, resources, dialogs, and shell
  menus are rendered as sharp conventional 2D panels;
- the original simulation and command semantics remain shared with Android;
- controller input is translated into the existing logical mouse/keyboard
  command path instead of creating Quest-specific game rules.

The first product milestone is a polished monoscopic command table. A true
stereoscopic miniature battlefield is a later, separately gated milestone.
First evaluate the existing engine camera for a convincing 2.5D tabletop
appearance; the stereo milestone is not a prerequisite for that experiment.

## 2. Product decision

### 2.1 The requested hybrid is technically feasible

The existing draw order already provides a useful separation point:

1. `W3DDisplay::drawViews()` draws terrain, units, effects, and world-bound
   overlays.
2. `TheInGameUI->DRAW()` draws the screen-space HUD and `.wnd` hierarchy.
3. Mouse, video, cinematic text, and diagnostics are drawn after the HUD.

The XR path can therefore preserve the world result in one texture, clear a
second transparent target, and draw the screen-space UI into that target. The
OpenXR compositor then presents the two textures independently.

This is not a zero-change switch. The current XR path exposes only one finished
1280x720 texture containing both world and UI. It needs two render outputs and
two composition layers before the requested presentation is real.

### 2.2 Layer ownership

| Content | Initial owner | Reason |
|---|---|---|
| Terrain, water, buildings, units, particles | Command-table layer | Spatial game content |
| Selection decals, move/attack markers | Command-table layer | Must stay registered with terrain |
| Unit health bars and object captions | Command-table layer initially | They depend on world-to-screen projection |
| Control bar and build/production palette | UI layer | Readability and direct pointing |
| Radar/minimap | UI layer | Readability; later may become its own panel |
| Money, power, general points, timers | UI layer | Screen-space status |
| Tooltips, confirmation dialogs, pause menu | Front UI layer | Must remain readable and modal |
| Main menu, options, load/save, skirmish setup | Front UI layer only | No tabletop is needed in shell state |
| Cursor/ray reticle | XR interaction layer | Must not be baked into both textures |
| Loading and fatal error state | Dedicated front panel | Engine may not yet have a valid frame |

### 2.3 Presentation modes

The app has explicit modes rather than treating every game screen alike:

- `ShellPanel`: menu texture on a world-locked front panel; no command table.
- `TabletopGame`: battlefield table plus separate control UI.
- `ModalPanel`: gameplay can remain visible, but a modal UI panel is placed in
  front and owns input.
- `LoadingPanel`: lightweight native/OpenXR status while engine initialization
  is running.
- `FallbackPanel`: the current combined game frame, retained as a recovery path
  until hybrid mode passes acceptance.

### 2.4 Camera presentation before true stereo

The user's requested first experiment is to reuse Generals' existing camera
rotation, tilt, and zoom to improve the tabletop impression. Source inspection
confirms `View::userSetAngle`, `userSetPitch`, `userSetZoom`, and their reset
functions. `W3DView::setPitch` recalculates terrain/camera constraints and
`setCameraTransform` updates terrain and clipping. Use those entry points and
honor scripted-camera locks instead of patching projection matrices.

- Add live, reversible camera presets: classic oblique RTS, steeper tabletop,
  and near top-down. Expose yaw, pitch and game zoom separately, with a reset
  to the original camera and a visible indication of the active preset.
- Compare the same populated scene (buildings, infantry, vehicles, terrain
  slopes, shadows) from seated and standing positions on the real headset.
  Tune actual camera elevation from the engine transform; do not assume a
  slider's numeric pitch is identical to the player's viewing angle.
- This supplies depth cues inside one image: perspective, shading, occlusion,
  and camera rotation. It does not produce binocular depth for game objects
  above the physical display plane. The board itself remains correctly posed
  stereoscopically in the room.
- A fixed oblique view can look convincing near a preferred viewing position.
  Walking around the board does not reveal new sides of buildings inside that
  image. Test for excessive combined foreshortening of the game camera and the
  physical panel; a steeper engine view may work better than a shallow one.
- Keep automatic head-driven changes of the game camera disabled for this
  first experiment. Any later monoscopic motion-parallax experiment needs its
  own calibration and picking checks; copying headset yaw/pitch directly into
  the game camera is not a geometrically correct tabletop projection.
- Decide from the visual A/B whether this 2.5D treatment is sufficient for
  Milestone A. True stereo remains an optional next quality tier.

### 2.5 Free board and window arrangement

Free arrangement is a required capability, extending the previous
place-once/recenter-only scope:

- Grab an explicit panel frame/handle or enter an arrangement mode to move
  the board or a UI window in all three axes and rotate it. Keep gameplay
  trigger/grip actions separate from arrangement so moving a panel never
  selects units, pans the map, places a building, or clicks a menu.
- Two-controller manipulation scales the selected surface uniformly about a
  stable anchor; offer a one-controller scale control as well. Preserve aspect
  ratio. Starting a second-hand gesture must not jump the surface; releasing
  either hand commits a stable pose without a size jump.
- The board defaults to horizontal, with an optional horizontal snap. Allow
  free tilt/rotation explicitly. UI windows default upright but remain movable
  and rotatable. Comfortable size bounds and a reset prevent lost or unusably
  small panels; exact bounds are tuned on the headset.
- Store a pose and physical size for each surface. Allow independent movement
  and optional grouped movement of the board plus its associated UI windows.
  Modal focus rules continue to apply after panels overlap or are rearranged.
- Distinguish physical panel scaling (changes the size of everything shown)
  from game-camera zoom (changes the visible game area). Moving a panel must
  not change the camera, simulation, or selected units.
- Rendering and hit testing use the same current pose, dimensions, and logical
  viewport mapping throughout a gesture. On tracking/focus loss, release
  manipulation capture and retain the last valid pose; suppress game actions
  until the relevant buttons are released.
- Preserve the player's layout across shell/game/modal transitions rather
  than relatching every surface. Persist size and relative layout in a
  versioned configuration; reconstruct safely after recenter/relaunch. Do
  not persist raw LOCAL-space coordinates as if they were room anchors.
  Persistent placement in the same real room across launches is a separate
  spatial-anchor capability.
- UI surfaces become independently movable when the render split exposes
  them. Until then, only the existing combined panel can be manipulated.

## 3. SDK architecture decision

### 3.1 Decision

Continue with direct native OpenXR for the game runtime. Do not migrate the
renderer into Unity and do not replace the XR activity with Meta Spatial SDK
during the recovery milestone.

Spatial SDK may later host Android-native companion panels, and Unity remains
valid for a separate reimplementation, but neither is the shortest or safest
route for this codebase.

### 3.2 Decision matrix

| Criterion | Direct OpenXR | Meta Spatial SDK | Unity + OpenXR |
|---|---:|---:|---:|
| Reuse current native GLES engine/context | Strong | Medium | Weak |
| Reuse current working XR session/passthrough | Strong | Weak | Weak |
| Sharp compositor quad layers | Strong | Strong | Strong |
| Android View/Compose panels | Manual | Strong | Medium |
| Existing Generals UI reuse | Strong | Medium | Medium |
| Native engine interop complexity | Lowest from current state | Higher | Highest |
| Cross-device OpenXR portability | Strong | Quest-specific | Strong with constraints |
| Risk of a second lifecycle/input system | Low | Medium | High |

### 3.3 Rationale

The repository already boots the full ARM64 game inside an OpenXR-created EGL
context, steps one game frame per XR frame, obtains the engine texture, submits
stereo projection layers, and enables Quest passthrough. Replacing that bridge
would discard the highest-risk work already completed.

OpenXR core provides `XrCompositionLayerQuad`, which is designed for 2D content
positioned in 3D space. The runtime-reported `maxLayerCount` must be honored;
the MVP needs only passthrough, battlefield, UI, and optionally a cursor/loading
layer.

Unity would add an engine host, managed/native lifecycle, texture-sharing, and
input translation layer without improving the original W3D renderer. Spatial
SDK is attractive for new Android UI panels, but the game currently owns its UI
and rendering in native C++. It is an optional later enhancement, not a
prerequisite for the hybrid table.

## 4. Current evidence and constraints

The 2026-09-13 inspection established:

- a Quest 3 can launch the XR flavor and boot the engine;
- the offscreen capture contains a valid 1280x720 Zero Hour frame;
- the headset path currently maps that complete frame to a horizontal mesh;
- there are no OpenXR action sets or controller rays;
- the panel re-latches after head translation, so it can move unexpectedly;
- XR debug readback runs synchronously every 600 frames;
- the shared user `Options.ini` currently selects Low LOD and reduced textures;
- SDL event initialization reports an error but boot continues;
- pause destroys the activity and process; shutdown has thrown a filesystem
  exception;
- only the shell map has been observed in the current XR run; a real skirmish
  is not yet accepted.

These observations prove a rendering bridge, not a playable XR release.

## 5. Target rendering architecture

### 5.1 Milestone A: planar hybrid command table

```text
Generals simulation and client update (once per game tick)
                         |
                         v
               W3DDisplay::draw()
                         |
          +--------------+--------------+
          |                             |
          v                             v
 drawViews + world overlays       screen-space UI pass
          |                             |
          v                             v
  world RGBA texture              UI RGBA texture
          |                             |
          v                             v
 world-locked OpenXR quad       front/world-locked OpenXR quad
          +---------------+-------------+
                          v
                  Quest compositor
                          |
                          v
                     passthrough
```

The battlefield remains a planar texture in this milestone. A spatially posed
quad still has correct binocular position and perspective as a physical plane,
but the content inside it is not yet stereoscopic 3D.

### 5.2 Milestone B: true stereoscopic battlefield

```text
simulation/update once
        |
        +--> world render left eye  --+
        +--> world render right eye --+--> projection layer + depth (optional)
        +--> UI render once ------------> sharp quad layer(s)
```

Milestone B requires eye-specific cameras, two world renders or a validated
multiview path, correct culling, particles, shadows, picking, and performance
work. It is not required to ship Milestone A.

### 5.3 Texture and alpha contract

- World texture: RGBA8 or sRGB-equivalent selected after a color-ramp test;
  alpha is opaque inside the board.
- UI texture: transparent clear, straight-alpha output unless runtime testing
  proves premultiplied alpha is required.
- UI swapchain: sampled without foveation and at a resolution that preserves
  menu text at normal viewing distance.
- World swapchain: fixed aspect ratio matching the logical game view; start
  with 1920x1080 only after a performance A/B against 1280x720 and 1600x900.
- Filtering: clamp edges; use mipmaps or compositor sharpening only after
  measuring oblique shimmer and UI halo artifacts.

## 6. Work preservation before implementation

The current checkout contains a large uncommitted Android/XR work in progress.
Before broad edits:

1. Record branch, HEAD, `git status --short`, and `git diff --stat`.
2. Preserve all tracked and untracked XR files in a named WIP checkpoint or
   patch bundle chosen by the repository owner.
3. Do not mix unrelated Android performance experiments into XR recovery
   commits.
4. Split subsequent work into reviewable commits by subsystem: plan,
   diagnostics, composition, render split, input, lifecycle, and quality.

No existing WIP file may be discarded to make the tree clean.

## 7. Implementation phases

### Phase R0: freeze evidence and remove self-inflicted quality loss

Goal: establish a trustworthy visual and performance baseline.

Tasks:

- Add an explicit XR diagnostic switch; disable periodic `glReadPixels` in
  normal XR runs.
- Log build identity, render resolution, swapchain format, layer limit, refresh
  rate, engine detail settings, and presentation mode once per session.
- Add a color/alpha test-pattern mode.
- Add performance counters for engine frame, world copy, UI copy, layer upload,
  and total XR frame.
- Keep 1280x720 as the control case; compare 1600x900 and 1920x1080.
- Use XR-owned defaults rather than inheriting a Low-detail phone profile.

Exit gate R0:

- no synchronous readback in a normal 30-minute session;
- repeatable screenshots/captures can still be requested explicitly;
- a baseline table records frame rate and timings for shell map and one real
  skirmish at each tested resolution;
- color ramp and alpha edges show no double-gamma or dark fringe.

### Phase R1: replace the projection-mesh poster with native quad layers

Goal: make the current combined frame sharp and stable before splitting it.

Tasks:

- Add a reusable `XrSwapchainSurface` abstraction for acquire/wait/FBO/release.
- Create one `XrCompositionLayerQuad` swapchain for the combined frame.
- Copy the engine texture to that swapchain once per changed game frame.
- Submit passthrough below the quad.
- Query and validate `XrSystemGraphicsProperties::maxLayerCount`.
- Use `LOCAL` space and latch placement once; remove automatic translation
  re-latching during normal head movement.
- Add temporary keyboard/ADB tunables for width, distance, height, pitch, and
  yaw until controller calibration exists.
- Retain the old projection-mesh path behind a fallback switch.

Exit gate R1:

- board remains fixed while the user leans or turns their head;
- menu text is materially sharper than the projection-layer mesh control;
- the board can be viewed for 30 minutes without visible judder;
- a runtime layer failure automatically falls back and is logged.

### Phase R2: split world and UI render outputs

Goal: implement the requested hybrid presentation.

Tasks:

- Extend d3d8gles XR configuration with a presentation mode and two exported
  texture handles: world and UI.
- Capture or resolve the world texture immediately after `drawViews()` and
  before `TheInGameUI->DRAW()`.
- Render screen-space UI into a transparent UI target.
- Keep world-projected UI with the world pass. Audit selection rings, health
  bars, captions, move/attack markers, placement previews, and cinematic
  overlays individually.
- Add shell/game/modal state reporting to the XR host.
- Submit world and UI as independent quad layers.
- In shell state, suppress the table and center the menu panel.
- In gameplay, pose the UI panel above or slightly behind the near board edge;
  avoid permanently head-locking a large panel.
- In modal state, move the dialog panel to the front and route all input to it.
- Preserve a combined-frame fallback for screenshots, movies, and any screen
  not yet classified.

Exit gate R2:

- main menu and skirmish setup appear as readable front panels without a table;
- a skirmish shows world content on the table and control UI separately;
- no element is duplicated, missing, or receives input through a modal dialog;
- radar, build buttons, money, power, tooltips, and pause menu are usable;
- a scripted combined-vs-split image comparison detects unintended omissions.

### Phase R3: controller actions, ray input, and table placement

Goal: make the hybrid composition playable.

Tasks:

- Create OpenXR action set and bindings for aim poses, select, context command,
  cancel/back, drag, scroll/zoom, rotate, and recenter/place-table.
- Render a ray and focus cursor in an XR-owned layer.
- Intersect controller rays with each active quad and convert hit UVs to the
  existing logical screen coordinate space.
- Route table hits through the existing world picking/command translator.
- Route UI hits through existing mouse/window dispatch.
- Implement input capture: the nearest active modal panel wins; otherwise UI
  wins over a board only where its alpha/hit region is interactive.
- Add one-time table placement with controller confirmation and an explicit
  recenter command.
- Add seated/standing presets; do not silently move the table with the head.
- Implement the free arrangement contract in section 2.5: frame handles,
  independent/grouped pose changes, one/two-controller uniform scaling,
  horizontal snap, reset, persisted relative layouts, and capture priority.
- Add the camera controls and comparative presets specified in section 2.4.

Exit gate R3:

- launch, skirmish setup, unit selection, box selection, move, attack, build,
  production queue, radar navigation, pause, save, load, and quit work using
  controllers;
- input cannot pass through menus into the battlefield;
- table remains stable after leaning 0.5 m and after headset recenter;
- left-handed mode is usable.
- board and UI panels can each be moved, rotated and scaled without triggering
  game commands; pointing remains accurate at the center and every corner;
- adding/removing a second hand, losing tracking, reopening a menu and
  recentering do not jump or stretch a user-arranged surface;
- physical size and game-camera zoom can be changed independently.

### Phase R4: lifecycle and fault recovery

Goal: behave as an Android/Quest application rather than a one-shot demo.

Tasks:

- call the supported SDL main-ready initialization before events setup and
  treat initialization failure as actionable;
- keep polling OpenXR frames while long engine loading proceeds, or move engine
  boot to a controlled worker with a strict GL ownership boundary;
- implement session focus, headset removal, Android pause/resume, and audio
  focus without unconditional `System.exit(0)`;
- make engine shutdown idempotent and fix the scratch-map filesystem exception;
- provide loading, missing-data, and fatal-error panels;
- preserve saves and imported user data across XR/2D flavors.

Exit gate R4:

- ten cold starts and ten Home/resume cycles complete without process death,
  corrupted state, or lost audio;
- headset removal pauses simulation and resumption is deterministic;
- missing data and boot failures produce an actionable visible error;
- shutdown has no uncaught exception.

### Phase R5: performance and comfort acceptance

Goal: select a shippable planar-table configuration.

Tasks:

- test real skirmishes at early, mid, and late game with representative AI;
- tune LOD, shadows, water/reflections, particles, table size, text scale, and
  panel distance as XR-specific settings;
- decouple game simulation cadence from headset refresh if one game frame per
  XR frame proves unstable;
- avoid rendering hidden world content behind opaque shell/modal panels;
- measure thermals, memory, missed frames, app motion-to-photon behavior, and
  controller accuracy over 90 minutes;
- test at least Quest 3; Quest 3S remains a separate physical gate.

Exit gate R5:

- selected refresh target is sustained for the agreed scenario, with no
  recurring judder or thermal collapse;
- UI remains readable at the selected pose;
- a 90-minute full skirmish session completes;
- saved game reload and Home/resume pass in the same candidate build.

### Phase S0: true-stereo discovery only after R5

Goal: determine whether a miniature 3D battlefield is worth its cost.

Tasks:

- inject eye-specific OpenXR view/projection matrices into the W3D camera;
- render a representative world twice while updating simulation only once;
- audit culling, shadows, water, particles, billboards, terrain LOD, and
  world-to-screen overlays;
- prototype controller ray-to-terrain and ray-to-object picking;
- compare multi-pass CPU cost with possible GLES multiview work;
- keep the R2 UI quad unchanged.

Decision gate S0:

- proceed only if stereo materially improves the product and a late-game scene
  can meet the agreed frame budget without removing essential gameplay cues.

## 8. First implementation slice

The first code slice is deliberately small and reversible:

1. make XR framebuffer capture opt-in instead of periodic;
2. expose and log the runtime compositor-layer limit;
3. introduce presentation mode names and stable placement configuration;
4. build and run existing Android/Quest compile checks;
5. create a physical Quest baseline before changing visible composition.

After the confirmed QTR-P2 projection repair, the next interactive slice
evaluates the existing engine camera and adds free manipulation of the current
combined panel (`QTR-017` and `QTR-018`). It can use the corrected mesh as a
bounded experiment. Native quad surfaces remain the production quality step;
the world/UI split then makes independent window arrangement possible
(`QTR-019`). Compare the 2.5D camera treatment on the headset before deciding
whether true stereo is necessary for the requested tabletop experience.

## 9. Planned code map

| Area | Planned responsibility |
|---|---|
| `GeneralsMD/Code/Main/XrHello.cpp` | Temporary host; shrink as components move out |
| `GeneralsMD/Code/Main/XrRuntime.*` | Instance, system, session, frames, spaces, lifecycle |
| `GeneralsMD/Code/Main/XrSwapchainSurface.*` | Swapchain image/FBO ownership and texture copy |
| `GeneralsMD/Code/Main/XrComposition.*` | Passthrough, table, UI, cursor/loading layer submission |
| `GeneralsMD/Code/Main/XrInput.*` | Actions, poses, bindings, ray intersections, focus |
| `GeneralsMD/Code/Main/XrGameBoot.*` | Engine bridge, state, texture access, safe shutdown |
| `d3d8gles.h/.cpp` | Stable XR render-output API only |
| `gles_pipeline.*` | World/UI render targets, alpha contract, optional diagnostics |
| `W3DDisplay.cpp` | Minimal draw-pass boundary hooks |
| `XrHelloActivity.java` | Android lifecycle and visible error/loading handoff |

XR code must not leak into game logic or network simulation.

## 10. Test strategy

### 10.1 Automated and host-side

- Android `arm64-v8a` configure and compile for both `zh` and `xr` flavors.
- Link check for native OpenXR entry points.
- Unit tests for pose math, ray/quad intersection, UV conversion, layer hit
  priority, and input state transitions.
- Render-split capture test: combined reference approximately equals world
  composed below UI within a defined tolerance.
- Alpha/color ramp image test.
- `git diff --check` and repository formatting checks.

### 10.2 Device smoke matrix

| Scenario | Build proof | Worn-headset proof |
|---|---|---|
| XR cold launch | log and activity start | loading/error panel visible |
| Main menu | capture contains UI | readable front panel and working ray |
| Skirmish setup | capture and input log | complete setup using controllers |
| Early game | world/UI texture evidence | selection, move, build, radar |
| Late game | performance trace | stable, comfortable, playable |
| Pause/Home/resume | lifecycle log | correct pause and restored interaction |
| Save/load | file and log evidence | restored match is controllable |
| 90-minute run | trace and no crash | thermal/comfort acceptance |

An APK build, installation, or log line does not substitute for worn-headset
acceptance.

## 11. Metrics

Record at minimum:

- XR refresh rate and missed/late frames;
- engine update, world render, UI render, layer copy, and total CPU time;
- world and UI texture dimensions/formats;
- draw calls and triangles in shell, early game, and late game;
- memory high-water mark and thermal state;
- table drift/reprojection symptoms;
- controller ray error at near/far table edges;
- session duration and lifecycle transition counts.

Exact performance thresholds are selected after R0 baselines. The plan does not
claim 90 Hz before a representative physical skirmish proves it.

## 12. Principal risks and mitigations

| Risk | Mitigation |
|---|---|
| UI calls depend on world depth/state | transparent UI target plus explicit render-state reset; audit each overlay |
| World-to-screen labels land in wrong layer | keep them with world initially; migrate only with registration tests |
| UI alpha is incorrect | test pattern, clear-to-transparent contract, explicit blend mode |
| Too many compositor layers | query `maxLayerCount`; atlas related UI into one texture |
| Large texture copies cost too much | measure GPU copy/resolve; update UI only when dirty where possible |
| Engine frame time exceeds XR budget | decouple simulation cadence; render hidden layers conditionally |
| Table moves with the user | one-time room-space placement and explicit recenter only |
| Unity/Spatial migration consumes the project | keep direct OpenXR as accepted architecture for R0-R5 |
| Existing dirty WIP is lost | preserve checkpoint and patch from current files only |
| Game assets/licensing are mixed into binaries | keep user-supplied assets external and distribution process compliant |

## 13. Release boundary

The planar hybrid can be called playable only after R3. It can be called a
release candidate only after R4 and R5. True stereo is never implied by a
planar table screenshot.

Fallbacks, in decreasing order of ambition:

1. hybrid command table plus separate UI;
2. polished combined-frame OpenXR quad;
3. conventional Android 2D app on Quest;
4. Android phone/tablet release only.

## 14. Immediate backlog

- `QTR-001` Preserve and inventory current XR WIP.
- `QTR-002` Make XR framebuffer capture explicit and opt-in.
- `QTR-003` Log system graphics properties and maximum layer count.
- `QTR-004` Add presentation state and placement configuration.
- `QTR-005` Implement reusable OpenXR quad swapchain.
- `QTR-006` Submit the combined game frame as a native quad layer.
- `QTR-007` Add stable placement and manual recenter.
- `QTR-008` Add d3d8gles world/UI texture contract.
- `QTR-009` Split `drawViews()` from screen-space UI output.
- `QTR-010` Implement shell/game/modal layer policy.
- `QTR-011` Add controller action set and ray/UV translation.
- `QTR-012` Complete playability command matrix.
- `QTR-013` Repair SDL initialization and Android/OpenXR lifecycle.
- `QTR-014` Fix safe shutdown and visible boot errors.
- `QTR-015` Run resolution, quality, performance, and comfort matrix.
- `QTR-016` Decide whether to start true-stereo discovery.
- `QTR-017` Evaluate and expose existing game-camera yaw, tilt and zoom with
  reversible tabletop presets and a populated-scene headset A/B. P3 controls
  implemented and compiled; populated-scene headset acceptance remains open.
- `QTR-018` Implement free placement, rotation and uniform scaling of the
  current board/panel using explicit manipulation capture and reset controls.
  P3 implemented with saved relative layouts; physical manipulation acceptance open.
- `QTR-019` Extend arrangement to independent/grouped UI surfaces after the
  world/UI split; preserve relative layouts and verify transformed picking.

## 15. Definition of done for this plan

This plan is complete when R5 is accepted on physical hardware or the project
explicitly selects one of the documented fallbacks. Until then, individual
builds must report the highest passed gate and the still-open physical gates.

## 16. Implementation record

### 2026-09-13: recovery start

- `QTR-001`: current tracked changes were preserved as a binary patch and the
  untracked XR implementation as a separate local archive before recovery
  edits. Existing WIP was not reset or discarded.
- `QTR-002`: implemented. Periodic XR `glReadPixels` capture is disabled by
  default and can be enabled explicitly with `GX_XR_CAPTURE_PPM`.
- `QTR-003`: implemented in source. The runtime now logs system identity,
  maximum swapchain dimensions, maximum composition-layer count, and tracking
  capabilities. Native ARM64 compilation and `assembleXrDebug` passed.
- The resulting XR debug APK was installed successfully on Quest 3 device
  `2G0YC5ZG9609PY`.
- Device runtime verification of the new log is still open: Horizon OS blocked
  launch at its controller-required system dialog while no controller was
  active. Installation is not counted as worn-headset acceptance.

### 2026-09-13: first visible recovery correction

- The device run confirmed the new build and reported `maxLayers=32`, but also
  exposed `passthrough extension: MISSING (opaque fallback)`.
- Root cause: the XR flavor did not declare Meta's required
  `com.oculus.feature.PASSTHROUGH` manifest feature, so the runtime correctly
  withheld `XR_FB_passthrough`.
- The XR manifest now opts into passthrough and the contextual passthrough
  loading background.
- Frame submission now follows Meta's passthrough composition contract:
  `XR_ENVIRONMENT_BLEND_MODE_OPAQUE`, passthrough below the projection layer,
  and source-alpha blending on the application layer.
- Automatic head-translation re-latching was removed. The board now latches
  once in `LOCAL` space and stays fixed until explicit controller recenter is
  implemented in `QTR-007`.
- Worn-headset verification of passthrough activation and board stability is
  still required after rebuilding and installing this slice.

### 2026-09-13: shell versus gameplay presentation

- Worn-headset feedback showed intro and shell content still lying on the
  tabletop, far away and strongly foreshortened during lateral head movement.
- The engine bridge now reports whether `GameLogic` is in an interactive game.
- Intro and every shell/menu state use `ShellPanel`: 1.35 m wide, upright,
  centered near eye height, and 1.10 m ahead.
- Entering an interactive match switches once to `TabletopGame`: 1.10 m wide,
  horizontal, 0.70 m ahead, and 0.45 m below eye height.
- Presentation-state transitions re-latch placement once. Head movement alone
  never repositions either surface.
- This is an interim mesh presentation. Native compositor quads and the
  world/UI texture split remain `QTR-005` through `QTR-010`.
- Native ARM64 compilation, `assembleXrDebug`, packaging, and installation on
  Quest 3 passed for this slice. Installed APK SHA-256:
  `aa7a726fda264dd6b6ec2721c79b1472a75e0abe5f5eb3b0e0796f4c9daa6161`.
- Fresh runtime-log verification is pending because the headset was doffed and
  Horizon OS intercepted the ADB launch with its controller-required dialog.
  The next worn-headset check must confirm `shell-panel`, active passthrough,
  stable world placement, and the `tabletop-game` transition after match load.

### 2026-09-13: QTR-P2 projection root cause and usable controller pointer

- Repeated worn-headset feedback exposed the underlying projection error:
  `matPerspectiveFromFov()` multiplied the X/Y scale by `nearZ=0.05`, although
  its bounds were already FOV tangents. That shrank the image by 20x and made
  the submitted image inconsistent with the runtime's declared FOV. Physical
  placement adjustments did not address it.
- Corrected the tangent-based projection, matching the OpenGL ES convention in
  [Khronos xr_linear.h](https://github.com/KhronosGroup/OpenXR-SDK-Source/blob/main/src/common/xr_linear.h).
  Extracted production math to `GeneralsMD/Code/Main/XrMath.h` and added
  `scripts/qa/xr-math-test.cpp`: 111 host checks cover asymmetric frustum edges,
  near-plane independence, physical screen scale, near/far clipping, translated
  head poses, yaw sweeps, and picking on upright/horizontal panels. The launch
  self-check now validates frustum output as well as identity transforms.
- Reject invalid head poses before placing or rendering the panel. Honor
  explicit LOCAL-space recenter events at their effective time.
- Added native OpenXR Touch actions, ray-to-panel UV mapping, a high-contrast
  cursor with press feedback, and a thin panel border. Input uses the exact
  transform used to draw the panel. The engine receives top-origin pixels.
- The Android `SDL3Mouse::createStreamMessages()` override previously discarded
  all pointer events. It now runs the inherited pointer pipeline in XR mode
  after the first controller pointer event. Touch-only behavior is unchanged.
  The ray has persistent hover and balanced button transitions; aiming away or
  losing tracking releases buttons, and re-entry requires a released trigger.
- Controls: right trigger = left click/drag, right grip = right click,
  B or left Menu = Escape/back/pause, X = re-place in front of the head,
  left stick = arrow-key pan, right stick up/down = wheel zoom.
- Y toggles an interactive game's whole frame between tabletop and upright
  presentation. This provides readable access to the existing in-game UI
  until independent world/UI textures are implemented. It is not the final
  simultaneous tabletop-plus-upright-control-bar design.
- Native ARM64 build and host geometry checks passed. Runtime acceptance must
  still confirm visual stability, cursor alignment, menu activation, gameplay
  commands, and comfort on the worn headset; a successful build is not enough.

#### Physical feedback and final verification

- The user confirmed that the corrected display looks good on the worn Quest.
  They also correctly identified that the battlefield is still a flat image;
  this is the expected current presentation, not a stereo world renderer.
- Device log captured menu pointer presses/releases, a transition into
  `tabletop-game`, further pointer drags/secondary presses, and explicit
  recenter requests. The stereoscopic device capture showed the battlefield,
  control bar, cursor and real-room passthrough together.
- The regression test was also run with the original projection scale restored
  in a temporary copy: it failed on the very first frustum-edge assertion
  (`-0.01` instead of `-1`). Corrected production math passed all 111 checks.
- Final input safeguard: disable screen-edge camera scrolling in XR (sticks
  and right-grip dragging remain available), and cancel scrolling when the ray
  leaves the panel or the secondary button is released. This prevents a stale
  edge position from moving the camera after tracking loss.
- Final native build and APK packaging passed. APK SHA-256:
  `783ec216d7a447a53a2814ff8fe5fdbd969a796437b5436f548f602b34030911`.
  Installation on Quest 3 `2G0YC5ZG9609PY` succeeded after the user's visual
  confirmation of the preceding build (same corrected projection).
  Post-install startup confirmed `projection-v2` frustum self-check PASS,
  Touch action attachment, and passthrough ACTIVE. Android delivered an
  immediate `onPause`, so the existing activity lifecycle then stopped this
  automatic launch; normal worn-headset relaunch and lifecycle recovery remain
  separate checks (`QTR-013`).
- This acceptance covers the corrected appearance of the presentation, not a
  full match/playability or long-session acceptance. The final scroll safeguard
  still needs the relevant worn-headset behavior check. The previously known
  SDL events initialization warning remains in the boot log (`QTR-013`);
  native controller events currently enter the engine buffers directly.
- Next implementation priority: separate world and screen-space UI textures so
  the battlefield can remain horizontal while the control bar remains upright.
  Actual raised terrain/buildings/units require rendering the game world for
  each eye; changing the orientation of the current image cannot supply depth.

### 2026-09-13: user camera and free-arrangement clarification

The user proposed exploiting the original game's rotatable/tiltable camera
before attempting a stereo renderer, and explicitly requested freely movable,
scalable windows and board. These are now explicit requirements in sections
2.4/2.5, the R3 acceptance gate, and tickets QTR-017 through QTR-019. The next
slice prioritizes camera A/B and combined-panel manipulation, followed by
independent windows when their render surfaces exist. These capabilities are
planned; QTR-P2 currently provides fixed size, recenter and whole-panel
upright/tabletop switching only.

Reproduce the host geometry test from the repository root:

```sh
c++ -std=c++17 -Wall -Wextra -Werror \
  -I GeneralsMD/Code/Main \
  -I build/android-vulkan/vcpkg_installed/arm64-android/include \
  scripts/qa/xr-math-test.cpp -o /tmp/generals-xr-math-test
/tmp/generals-xr-math-test
```

### 2026-09-13: QTR-P3 camera and arrangement implementation

The user authorized building the camera-first and free-arrangement slice.
The existing native OpenXR/GLES host remains the implementation basis; no
Unity or Spatial SDK rewrite and no changes to the unrelated Apple project.

Implemented:

- Camera preset cycle: classic engine defaults, 65-degree tabletop view,
  85-degree top-down view. A match initially requests tabletop. Each preset
  restores default yaw/zoom and uses the engine's user camera actions, respecting
  scripted-camera locks. The pending preset is retried when the camera unlocks.
- Right-stick horizontal rotation; left trigger plus right-stick vertical
  tilt; existing right-stick vertical wheel zoom otherwise. Game pitch and
  zoom retain engine constraints. Camera manipulation is separate from physical
  surface size and does not create binocular world depth.
- A toggles an explicit arrangement mode. Either controller grip captures
  a rigid hand-to-surface offset for translation and rotation. Two grips
  rotate/translate around their midpoint and scale uniformly from separation.
  Adding/removing a hand rebases the capture without jumping. Near-coincident
  hands and invalid grip tracking cancel the gesture until release.
- Physical width is bounded to 0.45–2.50 m. The right stick scales without
  gripping, so one-controller arrangement remains possible. Left-stick
  vertical input moves the surface along the right controller's aim direction.
- Y toggles level snap/free orientation in arrangement. Level snap is applied
  at gesture completion: horizontal for the tabletop, vertical for the screen.
  X resets reachable placement; in arrangement it also restores default size.
  Outside arrangement Y still switches the whole in-game surface upright/flat.
- Upright and tabletop layouts are kept separately. Versioned, validated
  app-private `xr-layout-v1.cfg` stores transforms relative to a launch-heading
  frame, not raw room coordinates. Saving uses a temporary file and rename.
  A future launch or runtime reference-space reset reapplies the layout against
  the current head heading; these are not persistent room/physical-table anchors.
- A visible attached three-line legend identifies `P3 PLAY` or `P3 ARRANGE`,
  camera/level state, physical width and controls without covering game pixels.
  Picking uses the same moved/rotated/scaled matrix as rendering.
- Arrangement consumes game input. Mode transitions, focus loss and pointer
  tracking loss release game controls and require neutral input before rearming.
- JNI boot now calls `SDL_SetMainReady()` before initializing SDL events;
  initialization failure is no longer silently ignored. Activity `onPause`
  no longer destroys the game on a transient system overlay. OpenXR visibility
  and focus govern suspension; actual destroy still requests process teardown.
  These lifecycle changes are build-verified, not yet physically accepted.

#### P3 controls

| Input | Play mode | Arrangement mode |
| --- | --- | --- |
| A | Enter arrangement | Finish arrangement |
| B | Back / pause | Finish without sending Back to game |
| Right trigger | Select / drag | No game input |
| Right grip | Existing secondary mouse action | Grab / move / rotate |
| Left grip | No game action | Grab / move / rotate |
| Both grips | No arrangement | Midpoint movement / rotation / uniform scale |
| Left stick | Pan game camera | Vertical: distance along right aim |
| Right stick | Horizontal: yaw; vertical: zoom | Vertical: scale without gripping |
| Left trigger + right stick vertical | Tilt game camera | No game input |
| Right stick click | Cycle tabletop / top-down / classic | No game input |
| X | Recenter current surface, preserve width | Reset pose and width |
| Y | Switch whole game surface upright / tabletop | Toggle level snap / free |

#### Verification and remaining gates

- Native ARM64 `z_generals` build passed.
- Both `assembleZhDebug` and `assembleXrDebug` passed (normal non-fatal
  dependency/Java deprecation warnings remain). The 2D Android APK was built,
  not installed or gameplay-tested during this slice.
- Production projection/picking regression: 111 checks passed.
- Production placement/layout/legend regression: 76 checks passed, also with
  UndefinedBehaviorSanitizer and no reported undefined behavior.
- Production interaction orchestration with a mocked engine: 31 checks passed
  for mode isolation, focus/tracking rearm, camera-lock retry and view switching.
  These mocks are not a substitute for Touch runtime/engine integration tests.
- AddressSanitizer could not run on this host: process sampling showed a
  recursive allocator lock during ASan/dyld initialization before `main`.
  The two test processes were terminated; this is not recorded as an ASan pass.
- XR APK SHA-256:
  `36f2d954733b67c36c69a7a3e3a16627ec381b572d632204793c0c7010b9c173`.
- 2D APK SHA-256:
  `2613afa98c48c0dbd4eb4ea21f9c65aa8cd7c3ef4c8b8fefdead0415c82594c1`.
- `adb devices -l` identified Quest 3 `2G0YC5ZG9609PY`. XR replacement
  installation returned `Success`. Launch is currently gated by the Quest
  `LaunchCheckControllerRequiredDialogActivity`; the user must activate Touch
  controllers and open Generals XR. No fresh P3 runtime/visual pass is claimed.

Headset acceptance still required: see the P3 legend; try all camera presets
in a populated match; compare tilt and zoom; grab with each hand; add/remove
a hand during scaling; test bounds, snap/free and reset; click known game UI
after transformations; leave/reenter arrangement while holding a trigger;
lose/recover tracking; open a system overlay and resume; relaunch and verify
both saved layouts. Check readability, comfort and frame pacing while playing.
Independent UI windows (`QTR-019`), real table detection, stereo world rendering,
full-match command coverage and lifecycle/soak acceptance remain open.

Reproduce the additional tests from the repository root (each fixture is
created under a fresh temporary directory and removed by its test):

```sh
test_dir=$(mktemp -d /tmp/generals-xr-tests.XXXXXX)
for test in xr-placement-test xr-interaction-test; do
  c++ -std=c++17 -Wall -Wextra -Werror \
    -I GeneralsMD/Code/Main \
    -I build/android-vulkan/vcpkg_installed/arm64-android/include \
    "scripts/qa/$test.cpp" -o "$test_dir/$test" || exit 1
  "$test_dir/$test" "$test_dir/$test.cfg" || exit 1
done
```

### 2026-09-13: QTR-P4 perspective findings and personal camera default

#### What the user/device evidence establishes

The user described P3 as "very good" and requested an appropriate tabletop
default and an honest assessment of the camera's limit. The persisted Quest
log confirms a real match, cycling 65/85/37.5-degree camera views, arrangement
mode changes, translation, and saved board sizes including 0.62 and 1.00 m.
This is positive P3 usability evidence, not a full-match/soak acceptance.

#### Camera-only improvement versus a real miniature world

The current game texture is identical for both eyes. Only the physical plane
has stereo placement. In `W3DView::buildCameraPosition()` the camera is built
from height, zoom, pitch and yaw, then aimed at the game pivot. The result is
projected once by the game and projected again when viewed on the XR board.
A more top-down game camera reduces the *additional* ground foreshortening but
cannot raise the buildings above the plane or reveal their sides when leaning.

Near the game-camera target on flat terrain, the ground-axis projection scale
is proportional to `sin(pitch)` relative to the transverse axis. Changing
65 to 78 degrees increases this local scale by `sin(78)/sin(65) = 1.079`.
That is only about 8 percent; it is not a global screen-distortion formula and
does not account for terrain relief, perspective variation or the user's pose.
Even 85 degrees remains a picture of rooftops on a table, not stereo miniatures.

Recommendation: use 78 degrees as a **candidate planar-table default**, retain
65 degrees for more visible facades and 85 degrees for map-like readability,
and let the user save a preferred pitch/yaw/zoom. Do not present 78 as a
scientifically optimal or physically accepted setting. The next substantial
visual improvement should be independent world/UI rendering, then an
eye-relative world projection proof, rather than endlessly adjusting angles.

| Approach | What it can improve | Hard limitation / decision |
| --- | --- | --- |
| Existing perspective camera, steeper pitch | Ground readability, less duplicate foreshortening | P4 candidate; still a shared flat image |
| Narrow FOV / orthographic camera | More uniform model scale, map/architectural appearance | Does not create binocular depth; requires correct projection/culling/picking integration |
| Head-driven yaw/pitch on the existing texture | Motion on the image | Not geometrically correct viewing through the table; risks visible scene sliding |
| Eye-relative stereo world, separate UI | Actual raised terrain/models, lean-around parallax | Recommended target for a miniature-diorama expectation; needs bounded rendering work |

A narrow-FOV experiment is deliberately **not** bundled in P4: the inspected
`W3DView::setFieldOfView`/`setCameraTransform` paths only invalidate/apply the
FOV through the shown `RTS_DEBUG` branches. Simply setting the field in this
release-style build would not establish a real new lens. An orthographic
camera also needs explicit world-camera projection, view bounds and picking
validation, not an image stretch. Neither is a shortcut to true stereo.

The geometric basis for a future table-window proof is an asymmetric frustum
derived from board corners and each eye, not just an arbitrary orbit angle.
See the primary implementation documentation for
[generalized perspective projection and stereo pairs](https://psychopy.org/api/tools/viewtools.html#psychopy.tools.viewtools.generalizedPerspectiveProjection).
This informs the proposed design; it is not a dependency or shipped P4 feature.

#### Implemented in P4

- Default `TABLE 78`; right-stick click cycles `OBLIQUE 65`, `TOP 85`,
  `CLASSIC` (map/engine default, observed 37.5), then `TABLE 78`.
- Presets now call the engine's `userSetZoom(1.0)` to actually reset desired
  camera height to its maximum. The previous `userSetZoomToDefault()` reached
  `W3DView::setZoomToDefault()`, which only invalidates state and does not
  reset height/zoom. This fix is isolated to the XR bridge.
- Left trigger + right-stick click saves current yaw, pitch and desired
  camera height as a personal default. The legend shows `FAVORITE` on success
  or `SAVE FAILED` on failure. Camera locks prevent capturing a scripted view.
- Left trigger + X restores the personal default (78 degrees until one is
  saved), without moving the physical board. Plain X remains board recenter.
- A new match or app launch uses the personal default when available.
  `xr-camera-v1.cfg` is app-private, versioned, finite/range-validated and
  atomically replaced. It is independent of the existing board-layout file.
- Restore uses the camera's desired height through `userZoom(target-current)`
  and preserves engine height limits; it does not restore a map coordinate.
  `getZoom()` and `setZoom()` do not form a round-trip pair on elevated terrain,
  hence the explicit desired-height representation. A different map/mod may
  clamp the saved height to its permitted range.
- Camera save/reset gestures suppress stick deflection for that frame.
  The legend identifies P4 and shows the current requested pitch in degrees.
  The reported pitch is the user-camera value, not a measurement of a
  potentially script/terrain-adjusted final optical axis.
- Physical placement, size, level snap, game rules, world rendering backend,
  XR projection matrices and original menu composition are unchanged.

#### Verification

- Native ARM64 build passed; both 2D Android and XR debug APKs packaged.
- 111 projection checks + 76 placement/layout/legend checks + 39 mocked
  interaction checks + 43 camera profile checks = **269 passed**.
- Camera profile tests also ran with UBSan, including invalid version,
  truncated/non-finite/out-of-range/trailing input and failed-load preservation.
- XR APK SHA-256: `edbc0eaf96ef429eae84207bd15497c41800f941ed4433097f0daec0c48dcdc7`.
- Replacement install on Quest 3 `2G0YC5ZG9609PY` returned `Success`.
  Automatic launch currently stops at the Quest controller-required system
  dialog; P4 in-match optics, saved-camera round trip and relaunch acceptance
  still need worn-headset verification. The good P3 report is not a P4 pass.
- Existing tests remain reproducible using the commands above. The new
  `scripts/qa/xr-camera-test.cpp` uses only the main-header include directory
  and takes one fresh temporary fixture-file path, like the placement test.

#### Next bounded implementation gates

1. **P5 — separate surfaces, same proven rendering.** Introduce explicit
   world/UI offscreen targets around `W3DDisplay::draw()`'s `drawViews()` and
   `TheInGameUI->DRAW()` boundary. Keep all shell/video/loading screens upright.
   Classify selection circles, building ghosts, health labels and world-anchored
   overlays before moving passes: not everything called UI belongs on a menu
   panel. Reuse existing layout/picking for independently placed surfaces.
   Accept when a populated match has a horizontal world image and a readable,
   separately movable command panel with correct commands and menu transitions.
2. **P6 — stereo geometry proof behind an opt-in switch.** Render a calibrated
   floor grid and known-height objects from the actual two eye poses relative
   to the board. Validate floor alignment, eye order, depth scale and leaning
   from multiple positions with the old planar mode still available. This
   isolates the projection contract before coupling it to the full game.
3. **P7 — integrate the real world render twice, update once.** Audit and extract
   a render-only world pass; do not call the full engine frame or display draw
   twice. The current display path even decrements cinematic counters while
   drawing. Ensure per-eye culling, terrain coverage, shadows, particles,
   world-space picking and frame pacing. Start on one offline populated map;
   keep the planar fallback until worn-headset comfort/performance acceptance.

Do not start a Unity/Spatial SDK migration for these gates. The native renderer
and engine models already exist; the missing capability is the world-camera
and layer contract, not a replacement game engine.

### 2026-09-13 — P5 separate battlefield and command UI

Implementation checkpoint, not a completed playability/comfort gate.

#### Render contract

- W3D still updates and draws once. After `preDraw`, selection rectangles and
  move/attack/placement-angle hints, the pending batch is flushed. The GLES
  backend snapshots the world color attachment with a GPU blit.
- Subsequent screen-space draws write simultaneously to the original composed
  attachment and a transparent UI attachment (GLES3 MRT). No duplicate window
  callbacks, simulation steps, readback loop or video-counter advancement.
- The normal D3D RGB blend is retained. Separate alpha accumulation provides
  UI coverage; ordinary alpha UI is composited over a dark readable backdrop
  on the detached panel. Destination-dependent/additive UI assets and mods
  still require visual checks. The complete original RGB frame stays available.
- The tactical viewport is cropped for the board; display and ray coordinates
  share that crop. The detached UI initially keeps the **whole logical canvas**
  so HUD, tooltips, radar and popup widgets are not discarded. This is one UI
  surface, not independent placement of every retail window. Compact command
  bar/radar extraction and grouped manipulation remain later work.
- Both panels are depth-tested in the eye render. Only the nearest hit panel
  receives the pointer. The engine mouse graphic is not baked into both images.
- Shell, loading, videos, letterboxing and modal dialogs use the upright
  composed fallback. Y explicitly selects the combined presentation in-game.
  Skipped renders retain the last published layer pair rather than flickering
  between split and combined layouts.
- The first on-device run exposed an overly strict shell test: `hideShell()`
  retains the menu stack. `getScreenCount()==0` therefore prevented split mode
  in a match. The corrected build tests `isShellActive()` instead. The user's
  initial `P5 COMBINED` report belongs to that first build.

#### Input and saved layout

- Mouse routing defaults to the original behavior on all ordinary platforms.
  XR world hits bypass window occlusion; UI hits are consumed by windows even
  when no widget uses them, preventing click-through world commands.
- Surface changes insert neutral frames. The previous recipient consumes its
  queued button-up before routing switches; a held trigger cannot become a new
  click on the other panel. Camera sticks do not operate while pointing at UI.
- Layout v2 stores composed screen, board and detached UI independently.
  Existing v1 position/size data migrates without modification; the v1 file
  and `xr-camera-v1.cfg` are preserved. New UI default: 0.95 m wide, to the right,
  upright and turned toward the player. No real-world plane anchor is inferred.
- A enters/exits arrangement. In split arrangement, right-stick click switches
  BOARD/UI; grips move/rotate, both grips scale, right stick scales, left stick
  adjusts distance. Y toggles level/free; X resets the selected surface; B exits.
- Outside arrangement, right-stick click and left-trigger camera save/default
  chords remain as in P4. Y switches SPLIT/COMBINED. Menus correctly say COMBINED.

#### Reproducible verification

- Native ARM64 `z_generals` build passed; both `zh` and `xr` APKs packaged.
- All production-header host tests passed with `-Wall -Wextra -Werror` and
  UBSan: 111 geometry, 90 placement/layout, 45 mocked interaction, 43 camera,
  48 crop/input-handoff checks = **337 checks**. These are not engine gameplay tests.
- Standalone `scripts/qa/xr-mrt-device-test.cpp` passed **32 checks on the Quest
  3 Adreno 740**: GPU world copy, dual render targets, RGB composition, alpha
  accumulation, and resetting draw buffers without damaging retained UI.
  This isolated GLES test does not validate W3D content classification.
- Corrected XR APK SHA-256:
  `c2b5949ab68d6b76de290e8bb26b110d756168c7f28f69794a84b719ec49f6fb`.
  Replacement install returned `Success` on Quest 3 `2G0YC5ZG9609PY`.
- Installed `base.apk` hash matches the corrected artifact. The corrected
  session log reached shell/skirmish setup and then `stopHello`, not an
  interactive match. Thus activation of the real split frame is still open.
  Saved layout inspection confirms upright-screen level snap was enabled,
  explaining why that window returned upright after a grip rotation; the
  independent saved board remains horizontal. Use FREE to tilt a screen.
- Build output contains existing compiler/deprecated-Gradle warnings; these
  were not silently described as a warning-free build. No release/commit made.

Run `xr-layers-test.cpp` like the existing host tests (no fixture argument
needed). Build the hardware test with the NDK's
`aarch64-linux-android29-clang++ -std=c++17 -Wall -Wextra -Werror -static-libstdc++
-IGeneralsMD/Code/Main scripts/qa/xr-mrt-device-test.cpp -lEGL -lGLESv3
-o <temporary-executable>`, then
push to a dedicated `/data/local/tmp` filename on an explicitly selected device.

#### Remaining P5 checks (deferred by user for P6 graphics work)

1. Confirm the corrected build shows P5 SPLIT in a populated match, with the
   tactical world on the board and readable command UI at the right.
2. Test select, drag selection, move, attack, production, building placement,
   radar and abilities while alternating surfaces; include held-trigger
   crossing, loss of tracking and overlapping/moved panels.
3. Audit the existing Android touch-only build/radius preview path: it depends
   on `m_touchAimKnown`, which the XR mouse bridge does not currently publish.
   Do not claim that moving the draw boundary fixes those inherited previews.
   Avoid simply setting a touch validity flag true without engine validation.
4. Verify pause/options/load/save, movie and modal transitions, then relaunch
   with both surface layouts and personal camera intact. Check 2D Android on
   device too; its successful APK compile is not interaction acceptance.
5. Measure representative populated scenes and sustained thermal performance.
   The new two RGBA8 textures consume about 7 MiB at 1280x720; the eye renderer
   also owns one reusable depth buffer. No performance equivalence is assumed.

Buildings still do not project stereoscopic height above the board. P6 remains
the opt-in, eye-relative geometry calibration proof; P7 is the real game-world
stereo integration. Do not combine either with an engine/SDK migration.

### 2026-09-13 — P5.1 readability and compact presentation

The user now confirms that the detached P5 UI appears in a match. They report
excess empty grey panel area, unreadable flat control help and washed-out
desert terrain. Split activation is therefore confirmed by user feedback;
the remaining input, lifecycle and performance gates above are not closed.

#### Implemented scope

- Corrected the XR presentation color space, not terrain lighting or the
  shared engine gamma setting. The engine's display-encoded RGBA8 image is
  decoded to linear RGB before the eye framebuffer. An sRGB attachment then
  encodes it once; a linear RGBA8 fallback receives linear shader output.
  The old path encoded display RGB a second time. Android 2D is unchanged.
- The opaque detached console is cropped to the live ControlBar parent top
  plus a small margin, with bounded fallback geometry. The complementary
  upper canvas remains transparent and unframed above it, preserving status
  text, tooltips and modeless popovers. Both regions retain the same pixel
  scale and form a continuous canvas; this is not independent widget packing.
- Rendering and ray hit testing use the same crop and physical offsets.
  Empty upper HUD regions are skipped via live GUI hit testing, allowing rays
  to reach the board. Actual widgets still route exclusively to windows.
- The legend hinges up by 55 degrees relative to a horizontal board, smoothly
  reducing the hinge for an upright panel. It stays attached to the selected
  surface rather than following head pose. For the command console the legend
  is below the panel, away from upper HUD popovers.
- Existing saved placement, scale and camera favorites are preserved. The
  78-degree default and 65/85/classic alternatives are deliberately unchanged
  for a clean visual comparison. After this readability pass, compare 65
  degrees on the flat board; no camera preset creates stereoscopic height.

#### Reproducible evidence

- Native ARM64 `z_generals` compiled and both `zh`/`xr` APKs packaged.
- **685 host checks** passed with UBSan: previous 337 plus 348 presentation
  checks covering band bounds, continuous UV/physical seams, ray mapping,
  hinged legend transforms and display-to-linear reference values.
- **72 isolated GLES hardware checks** passed on Quest 3 Adreno 740: previous
  MRT coverage plus the shared production GLSL color decoder. A source grey
  value of 128 became approximately 188 through the old sRGB output path;
  the corrected path returns approximately 128. Additional near-black,
  threshold, midtone and white samples passed. This proves the transport
  error and its correction, not full-game desert visibility acceptance.
- Host presentation test: compile `scripts/qa/xr-presentation-test.cpp` with
  C++17, `-Wall -Wextra -Werror -fsanitize=undefined` and
  `-IGeneralsMD/Code/Main`; run the resulting temporary executable.
- XR APK SHA-256:
  `d88842a513563a9ad8dfce7b07ee947cecab3a327d6c0ce140f94d0cc6e1c893`.
  Android 2D APK SHA-256:
  `2c74f4e776e13d3b7d6d1b9dc7cee61374d5d56c60f66f3361d1b2cf8a9bba18`.
- Replacement installation on Quest 3 `2G0YC5ZG9609PY` returned `Success`.
  Installed `base.apk` SHA-256 matches the XR artifact above; launch returned
  successfully and the application process is running.
  The on-screen version identifier is `P5.1`. Existing compiler and Gradle
  warnings remain; this is not a release or a warning-free build.

#### P5.1 worn-headset acceptance still required

1. On the same desert map, compare infantry/terrain separation and dark units
   after the color correction. Check menus/video too; do not compensate with
   arbitrary terrain darkening before this comparison.
2. Confirm the grey panel is limited to the command band, with readable radar,
   production buttons, tooltips, status text and popovers. Test clicks through
   empty upper HUD space and clicks on real upper HUD widgets, including
   overlapping panels. Alpha/additive UI assets still need visual inspection.
3. Put the board fully horizontal and confirm the 55-degree help legend is
   legible and does not cover important world content. Move/scale/rotate both
   surfaces and relaunch to verify saved poses remain usable.
4. Continue the P5 command, build-preview, lifecycle, 2D-device and sustained
   performance checks before claiming a polished gameplay milestone. The user
   subsequently authorized P6 graphics work without closing these checks.

### 2026-09-13 — P6 stereo miniature proof and freely tilted windows

#### Priority decision

The user reports that P5.1 looks better and explicitly asks to skip the P5
completion pass for now, prioritizing tabletop appearance and spatial depth.
Treat the deferred checks as unverified, not failed or passed. They no longer
block this opt-in graphics prototype; they still matter before release.

#### Implemented graphics slice

- An asset-free miniature scene replaces the shell screen only when opted
  in: framed plinth, buildings with facade details, tanks, a river/bridge,
  low-poly ridge and trees. This is a calibration scene, not converted retail
  assets or a preview of final art quality. No game entities or orders added.
- A single static 1,304-triangle mesh is rendered using each OpenXR eye's
  actual pose and asymmetric FOV. +Z is board-local height. Height, width
  and depth scale uniformly with the user's board size (unlike the planar
  panel matrix, whose normal is intentionally unit-length).
- Opaque geometry uses depth testing, linear vertex colors and simple
  directional shading. No additional game-world render or simulation step,
  no dynamic shadow system, no multiview dependency and no SDK migration.
- Calibration bars are 2/5/10 cm high for a 1m-wide board; the legend shows
  the actual scaled heights. Ground ticks are spaced 5cm at that same width.
- Scene and legend use the saved board pose, independent of moving head pose.
  The existing arrangement/grab/scale controls work on the demo board.
- Enter from the shell/main menu with **left trigger + Y**, outside arrangement.
  Exit with the same chord or **B**. A enters arrangement; B first leaves
  arrangement, then a fresh B press exits the demo. X recenters the board.
- Demo input is neutralized before reaching the engine. The demo cannot be
  entered during an interactive match (including its pause menu); a later
  asynchronous match transition disables it. The shell continues its normal
  frame loop, so no unsafe suspension of network simulation is introduced.
- Default remains the original game presentation on every launch. Shader
  or mesh initialization failure leaves that presentation available.

#### Window tilt correction

The previous release snapped all LEVEL surfaces on grip release, including
the detached command window. Windows now default to FREE. A manual window
grab overrides even an older persisted LEVEL flag, and releasing it preserves
its exact orientation. Y can still explicitly align a window; a later manual
grab is free again. Board-only automatic horizontal snapping is retained.
Existing positions, sizes, saved camera favorites and layout file version
remain unchanged; new poses/flags are saved through the existing v2 path.

#### Verification

- Native ARM64 build passed; both `zh` and `xr` APKs packaged successfully.
- **732 UBSan host checks** passed: 111 math, 90 placement/layout, 63
  interaction, 43 camera, 48 layers, 348 presentation and 29 stereo geometry.
  New regressions exercise legacy-window LEVEL override, tilt persistence,
  explicit re-alignment, board snap retention, demo entry/refusal, neutral
  input, held-button rearming, arrangement and return to the shell.
- **23 standalone P6 GLES checks** passed on Quest 3 Adreno 740, using the
  production mesh and shaders with synthetic eye poses and an sRGB/depth
  framebuffer. 35,653 pixels differed between left and right images; 64,011
  changed when depth testing was disabled. These are controlled pixel
  comparisons, not headset depth/comfort or real-game performance acceptance.
- The captured 600x600-per-eye stereo pair was visually inspected: geometry,
  color, facade details, contact surfaces and stepped heights are visible.
  This is GPU-rendered test evidence, not an OpenXR headset screenshot.
- Reproduction sources: `scripts/qa/xr-diorama-test.cpp` and
  `scripts/qa/xr-diorama-device-test.cpp`; compile commands and the optional
  PPM output argument are documented in their file headers. Include the
  project's OpenXR headers as well as `GeneralsMD/Code/Main`.
- XR APK SHA-256:
  `bb245936269539faf35b5a06eb516dd89f111cecc02d92e610a2bfbc252a35d1`.
  Android 2D APK SHA-256:
  `9e0f96fe0709bd1b6257df20325048a1baa3039fb8f98fce1c0bdc016f7ac886`.
- Replacement install on Quest 3 `2G0YC5ZG9609PY` returned `Success`; the
  installed `base.apk` hash matches the XR artifact. The launch request is
  currently intercepted by `LaunchCheckControllerRequiredDialogActivity`.
  Activate the Touch controllers and launch/continue in the headset. No
  P6 in-app startup or worn-headset pass is inferred from the standalone test.
- Existing packaging warnings about unstripped `libmain.so` and deprecated
  Gradle features remain. No release, commit or upstream publication made.

#### Next graphics acceptance and P7

1. Tilt the command window slightly, release and relaunch: its angle must
   remain. Check board horizontal snap independently.
2. From the main menu enter the P6 demo, lean sideways and look down across
   the miniature roofs: correct eye order, positive height, stable ground
   registration and comfortable scale must be confirmed in the headset.
3. Move and scale the board; height must change proportionally. Return to the
   normal game with B and confirm the prior game presentation is intact.
4. Once P6's projection contract is accepted, prioritize P7: extract a
   render-only W3D world pass, apply eye-relative camera/frustum and board
   volume clipping, and render terrain/models twice while updating game
   state once. Then integrate shadows, particles and world picking without
   changing the existing conventional command UI. Keep the planar fallback.

### 2026-09-13: P7.0 actual terrain/model stereo prototype

#### User feedback and demo artifact correction

The user reports that the P6 test worked and authorizes proceeding to actual
game geometry. This is useful qualitative projection feedback, not a measured
performance/comfort pass. P5 completion remains explicitly deferred.
The additional report of black flutter exposed coplanar full-area surfaces:
the dark plinth and sand both ended at board-local Z=0. The plinth now ends
0.005 board-width units below the sand. A regression requires exactly two
full-area top triangles at Z=0. This removes that specific z-fighting source;
it does not prove that every observed black artifact had the same cause.

#### Implemented and deliberately limited scope

- Native OpenXR/GLES architecture retained. `XrWorld.h` maps the current
  tactical terrain center and right axis into board coordinates, with uniform
  XYZ scaling. Both eye projections use the current OpenXR poses/FOV used for
  compositor submission, including after board movement in that frame.
- An audited W3D display boundary brackets the existing `drawViews()` once.
  Re-running scene traversal is unsafe: `RTS3DScene::Customized_Render()`
  invokes render-object frame updates, and camera/filter code also mutates
  state. Instead, each eligible uploaded GPU draw is immediately repeated for
  the two eye targets while its dynamic buffers, textures and uniforms remain
  valid. The ordinary game render is retained for fallback. No second engine
  frame or second animation/scene traversal is introduced.
- Eligible categories: perspective, depth-tested world-space terrain, rigid
  models and skinned meshes. Skins use deformed world-space vertices and an
  identity world matrix. Offscreen reflection passes, sorted camera-space
  effects, screen-space UI and stencil shadow draws are excluded.
- P7 shader snippets override final clip-space position and clip fragments
  to the board volume. Existing material/light/fog evaluation stays intact.
  Coverage alpha and the existing display-to-linear decoder composite the
  RGBA8 per-eye captures into the XR view; UI remains on conventional panels.
- The original central-camera visibility list is still used. A central 65%
  width footprint provides a margin, not a guarantee of per-eye visibility.
  Missing edge/occluded geometry is a known prototype limitation. Effects,
  shadows, camera-facing assets and shared depth between world and UI need
  further integration. This is not the finished miniature art pass.
- Default on launch remains planar. In an offline skirmish or single-player
  interactive split view, **left trigger + Y** toggles P7.0. The legend reads
  `P7.0 STEREO WORLD | VIEW ONLY`, or explicitly reports planar fallback.
  The same chord returns. In the main menu it still opens the synthetic demo.
- Pointer orders and UI clicks are suppressed while the prototype is on;
  B remains available to pause, and arrangement/move/scale remains available.
  The offline simulation continues normally. No stereo world picking has
  been implemented, so this must not be advertised as playable P7. Network
  modes are excluded. Existing free window tilt and saved layouts remain.

#### Verification and deployment

- Native ARM64 build and both Android `zh`/Quest `xr` APK packages passed.
- **760 UBSan host checks**: math 111, placement 90, interaction 65, camera 43,
  layers 48, presentation 348, diorama 30, world mapping/projection 25.
- **26 standalone GLES checks** passed on Quest 3 Adreno 740 using production
  P7 shader snippets and synthetic test geometry: 34,243 stereo-different
  pixels and 60,636 depth-sensitive pixels, complete outside-volume clipping
  and disabled-hook fallback. The stereo pair was visually inspected.
  This tests shader contracts, not the complete live-engine capture path.
- `git diff --check` passed. Existing COM override and packaging/Gradle
  warnings remain; no commit, publication or release was performed.
- XR APK SHA-256:
  `a9013ac7a4499788d14db0bdc9e058c9f19c6484e44772d788f3f5f4886440e9`.
  Android 2D APK SHA-256:
  `556899c6aa9e4d378d996e63d3efd3e983834d39bede3c01e707a0eabfc89b8b`.
- Installed on Quest 3 `2G0YC5ZG9609PY`; `Success` and exact installed
  `base.apk` hash verified. Launch is intercepted by the system Touch
  controller-required dialog. No live P7 engine shader, match, performance or
  worn-headset pass is inferred from the successful package and GPU tests.

#### Immediate device acceptance and next implementation

1. Activate Touch controllers, launch, start an offline skirmish and enter
   P7.0 with left trigger + Y. Confirm the stereo-world label, actual terrain,
   buildings and units, positive height and stable head-relative perspective.
   Capture `P7.0 geometry targets` and `published ... source world draws x2`
   from the game stderr log; inspect shader failures and missing geometry.
2. Move/scale the board; confirm correct scale, board boundaries and no
   accidental orders. Return to planar mode and verify normal input/UI.
3. Recheck the P6 sand/plinth while moving the head for the reported flutter.
   Treat any remaining artifacts separately, with location and movement clues.
4. Follow with conservative stereo-aware engine culling, then depth-aware
   effects/shadows and ray-to-terrain picking. Measure GPU/frame time and
   memory on representative maps before choosing render resolution/multiview.
   P7 is complete only after the world is interactive and these device gates
   pass; P7.0 merely establishes the first real-geometry rendering path.

### 2026-09-13: P7.1 invisible-world investigation and correction candidate

The user confirms the P6 black flutter is gone. The first report of synthetic
geometry was verified as P6 shell-demo activation. A subsequent correct
offline-match P7 activation hides the board. Device logs at 20:32-20:33 show
`stereo world=ON` and repeated `80 source world draws x2`, without shader
failure messages in the inspected output. This disproves the previous
draw-count-based readiness check, not the user's activation procedure.

Code audit found two coverage hazards: engine RGB-only masks can leave the
eye texture's alpha clear, and an opaque D3D material may legitimately emit
zero alpha because its original framebuffer does not use it for composition.
The XR compositor discards precisely those zero-alpha pixels. P7.1 enables
coverage-alpha writes only on the additional stereo draws, preserves RGB
masks, skips alpha-only/no-color passes, and writes alpha=1 for unblended
fragments AFTER the original alpha test. Blended material alpha, cutout holes
and the ordinary Android/planar draw retain their original semantics.

Readiness now also requires at least 64 nonzero-alpha pixels in BOTH eye
targets. A full readback occurs at initial activation/allocation; an empty
result retains planar fallback and retries after 120 capture frames. Once
validated, steady rendering does not read back. This diagnostic prototype
gate can cause an initial hitch; it is not continuous visibility validation
and cannot certify the subsequent compositor, model correctness or comfort.
Logs report actual alpha/RGB pixel counts, GL errors and the first draw's
category, color mask, blending, culling and depth function.

The GPU regression now reproduces RGB-present/alpha-empty output, checks that
enabling alpha writes alone cannot repair zero material alpha, then tests the
coverage repair, cutout rejection, translucent alpha and unchanged ordinary
rendering. It compiles with the NDK but has NOT yet executed on the device:
ADB lost Quest 3 `2G0YC5ZG9609PY` before the push. The exact live cause is
therefore still a supported hypothesis, not a measured alpha diagnosis.

Native ARM64 build and the existing 760 host checks pass; diff whitespace
checks pass. The new GPU regression and actual offline-match headset test
remain mandatory. Labels distinguish `P7.1 STEREO WORLD` / `PLANAR FALLBACK`
from `P6 TEST DIORAMA`. The headset still has P7.0 until reconnection/install.

Both APK packages built successfully. P7.1 artifact SHA-256:
- XR: `633b35fb31d8d15922e617625c8ae5606d2bf0138a4523d9e04f8ed7c607ee32`.
- Android: `030015da6281705fcabf0243214bad33ef1132f26f6f944bf35fefe9b29e5967`.

#### Reconnection and deployment follow-up

Quest 3 `2G0YC5ZG9609PY` reconnected. The expanded standalone GLES regression
passed all **35 checks** on Adreno 740, including invisible RGB/alpha output,
opaque coverage repair, cutout preservation, blended material alpha and the
ordinary-path control. It produced 34,243 stereo-different pixels and 60,636
depth-sensitive pixels. This is controlled GPU evidence, not yet proof of the
live engine's alpha values or final headset visibility.

Replacement install returned `Success`; the installed `base.apk` SHA-256
exactly matches the XR hash above. `XrHelloActivity` resumed and process 29100
rendered frames with two valid OpenXR views. The user is asked to enter an
offline match and toggle left trigger + Y, reporting the P7.1 label and actual
terrain/buildings. Match alpha/RGB probes and worn-headset acceptance remain
open; earlier disconnected/uninstalled statements describe the prior attempt.

### 2026-09-13: P7.2 model inclusion and camera-only navigation

User reports real desert terrain/tire tracks in P7.1 but no buildings/units,
apparent extreme zoom and inability to scroll. Live P7.1 probes report nonzero
alpha/RGB in both eyes with GL error 0; first terrain draw has colorMask=7,
confirming the live RGB-only condition that P7.1 now handles.

Found a separate over-restrictive draw filter: it rejected EVERY enabled
stencil test. `RTS3DScene` normally renders buildings and units while writing
player/occlusion stencil tags with `STENCILFUNC=ALWAYS`. These are ordinary
color/depth draws, not stencil shadow volumes. P7.2 accepts color-writing
world-category draws when stencil is disabled OR its comparison is ALWAYS.
The extra eye draws disable stencil because their independent depth targets
do not contain the original central-camera tags; the original fixed state is
restored before the next normal draw. Alpha-only/no-color, restrictive stencil,
sorted-effect, shadow-category and offscreen draws remain excluded. Existing
depth testing and P7.1 coverage-alpha handling remain active.

Left stick now pans through `userScrollBy`, right-stick vertical zooms through
`userZoom`, and right-stick horizontal retains camera rotation. Navigation no
longer requires a planar pointer hit in stereo mode. Native camera locks,
offline/split/modal gates, focus/tracking loss and arrangement capture still
apply. Left-trigger pitch and preset/reset chords do not also zoom. Building
placement, unit orders and UI clicks remain disabled in stereo; B pauses and
left trigger + Y returns. Zoom retains original engine height constraints.

No arbitrary scene-scale multiplier was added. The 65% source footprint is
unchanged pending evidence with actual models visible. Every 180 capture
frames the bridge logs world center/span, viewport and physical board width;
the backend separately logs submitted terrain/model counts. These count
submissions, not visible models, and are not a replacement for headset QA.

Verification: native ARM64 build passed; **830 UBSan host checks** passed
(111 math, 90 placement, 71 interaction, 43 camera, 48 layers, 348 presentation,
30 diorama, 89 world/policy). New tests cover all 16 color masks with enabled,
disabled and restrictive stencil cases, plus pan/zoom without a pointer route,
modifier separation, camera lock, focus loss, tracking loss and arrangement.
The P7.1 production shaders are unchanged; their previously passed GPU test
does not independently validate P7.2's live-engine model integration.

Next headset gate: activate in an offline match, confirm original buildings
and units, pan with left stick and zoom with right-stick vertical; compare
normal and stereo scale. Inspect `P7.2 mapping` and separate model counts if
the scene remains empty or close-up. Shadows/effects and actual world picking
remain later milestones, not part of this correction.

Both APK flavors packaged successfully. XR SHA-256:
`f824ec0c3b0cef985d9e03b45eb3cf466c4b28a7dbbe51fb743c09ae2635d2eb`.
Android SHA-256:
`1711226ef173b9b9dc7bb4ea03c710190c26f12ef0b38a528a4bf6c602746dea`.
Quest 3 `2G0YC5ZG9609PY` replacement install returned `Success`; installed
base.apk hash matches exactly. Process 3736 entered shell-panel presentation
and rendered two valid OpenXR views. P7.2 actual-match acceptance is still
pending. No commit, publication or release was made.

### 2026-09-13: P7.3 first spatial gameplay candidate

The user positively accepts P7.2 ("Mega gut") and authorizes the next playable
step. This supersedes the prior open qualitative graphics gate, not measured
performance or full gameplay acceptance.

#### Framing and spatial picking

- Increase the central source footprint from 65% to 80%: approximately 23%
  more world width, 52% more ground area at the same aspect. Physical board
  size remains independent (A arrangement, grips or right stick, 0.45-2.5m).
  Existing camera zoom/favorite storage remains; no preference file reset.
- Invert the exact board similarity transform used for rendering, transform
  the controller ray into game coordinates and intersect it with the same
  bounded board volume. Cast against terrain and visible original models;
  use `RTS3DScene::castRay`'s shortened Ray.P1, not its unpopulated caller
  result fraction. Require a valid ground hit and an on-screen original-camera
  projection before enabling a command. Off-board targets are rejected.
- Override only the active pointer pixel's W3D pick ray; other camera queries
  and Android input retain their original rays. Bypass the screen-to-terrain
  cache for spatial pointers because their ray can change without a pixel
  change. Explicitly bypass this override for render-map calibration as well.
- A cyan/gold room-space target ring marks the actual hit on terrain/models.
  It is drawn before conventional UI panels. This is an aim/press indicator,
  not a replacement for the engine's build-validity or attack-validity checks.

#### Commands and UI ownership

- Right trigger press/release on the world invokes existing `TouchInput::tap`
  context rules: select an owned object, move/attack/interact using an existing
  selection, or clear selection as appropriate. No new game/network orders or
  independent command legality rules were invented.
- Armed abilities and building placement commit through the existing paired
  mouse events/GUI and placement translators. The mouse queue drains the
  small event batch during the game frame. Spatial hover supplies the existing
  preview aim point, and all subsequent picks use the tracked ray. Placement
  validity and resource costs remain owned by the original engine.
- Right grip press/release over the world cancels an armed state or deselects.
  B retains the normal Back behavior. Left trigger + Y returns to planar.
- Detached UI is clickable again and has first refusal over world hits.
  Existing frame-delayed route changes deliver releases to the old recipient.
  World actions fire only on a release after a valid world press; crossing
  UI, losing the ray, camera stick motion or modifiers cancels that gesture.
  UI-originated presses cannot become world commands on release.
- Native stick navigation remains; no duplicate arrow keys or mouse-wheel
  events are emitted from spatial hover. Arrangement/focus rearming remains.

#### Verification and remaining gates

- Native ARM64 build and both `zh`/`xr` APK packages pass. **965 UBSan host
  checks** pass: math 111, placement 90, interaction 71, camera 43, layers 64,
  presentation 348, diorama 30, world/picking 188, production input routing 20.
- The 188 world/picking and 20 input-routing checks also pass as NDK-built
  executables on Quest 3 ARM64. They validate transforms/routing with spies,
  not real retail-model collision or the full command translator chain.
- Require a worn-headset offline test: pick a bulldozer, move it, open the
  command panel, choose and place a power plant, then produce a unit from a
  suitable production building. Test red/invalid placement, cancellation,
  menu crossing while holding trigger, and move/scale the board before picking.
- Expanded framing still uses central-camera visibility, not a stereo-aware
  union frustum. Edge pop-in, effects/shadows, selection-area feedback, drag
  selection and placement-rotation UX remain follow-ups. Do not call the
  build/production flow accepted until the actual device test succeeds.

P7.3 XR APK SHA-256:
`ff02a917f883c62466b6e5a21cb0bb8f12848200593d85f3e26b096897cfad6f`.
Android APK SHA-256:
`41779a7370548932c6da62f668f0e4429cd44c104c642ea43652c278ba6f2ad8`.

Replacement install on Quest 3 `2G0YC5ZG9609PY` returned `Success`; installed
base.apk hash matches the XR artifact. Process 9792 renders two valid XR views
and entered an actual offline match/stereo mode. The mapping log measured a
619.7-unit board span at 1.31m physical width. No shader-failure lines appeared
in the inspected output. No successful spatial commit/build cycle is claimed
from these startup/render logs. Some inherited diagnostics still use older
P7.0/P7.2 prefixes (including obsolete "input disabled" toggle text); use the
verified APK hash and P7.3 on-screen label to identify this build.

### P7.4 — visible aim, inside-volume collision and higher stereo resolution

User feedback: no ray to aim at the bulldozer; no usable commands. Also a
clipped console popup after interacting with the displayed LT+Y hint, and
soft stereo rendering. The user explicitly deferred clarification of the
popup, so its clipping is **not claimed fixed** in this increment.

Changes:

- Always draw the tracked right aim as a 2.5mm eye-facing room-space ribbon,
  including misses. Gray means no actionable target; cyan plus a ring marks
  a world/UI target, orange indicates pressing. It vanishes on tracking or
  focus loss and while arranging; it cannot create commands by itself.
- Use the exact same attached-legend pose for rendering and input blocking.
  A click on the controller hints no longer reaches a menu behind them.
  LT+Y remains a physical-controller chord, not a clickable shell button.
- Fix `BaseHeightMapRenderObjClass::Cast_Ray` for endpoints inside the broad
  terrain box. `StartBad` from ray-vs-box means the endpoint is inside, not
  that the ground was missed. Keep that endpoint and compute its terrain
  cell bounds before the convergence exit. This also fixes subsequent
  original-engine `screenToTerrain` calls using the spatial pointer ray.
- Add read-only loaded-map center probes at high and low controller height
  and rate-limited pick-failure counters. No selection, unit order, map
  mutation or extra game traversal is performed by the probes.
- Increase eye capture width 1024 -> 1536 (1.5x linear, about 2.25x pixels).
  Preserve OpenXR view aspect and cap both dimensions at 2048. No world
  scale change or artificial sharpen filter. Original textures, source
  camera visibility/culling, effects and shadows are unchanged.
- Identify the build as `P7.4 ... HQ 1536 | R-RAY`; remove obsolete
  "gameplay input disabled" wording from the stereo-toggle log.

Verification:

- Native ARM64 build and both Android `zh` / Quest `xr` packages succeeded.
- 988 host UBSan checks passed; 231 projection/routing checks also passed
  as ARM64 executables on Quest 3 `2G0YC5ZG9609PY`.
- Added `scripts/qa/xr-terrain-device-test.sh` / `.cpp`: extracts the actual
  terrain function verbatim, links built WWMath, uses flat height samples.
  All seven cases passed on Quest. The same harness against HEAD's old
  function reproduces `FAIL start=75 end=-50 hit=0 expected=1`, with ground
  at z=40. Includes outside/inside/both-inside endpoints and genuine misses.
- Installed package hash matches the built XR APK. Initial launch was
  blocked by Quest's controller-required dialog; after controllers woke,
  app process 12484 started. These facts do not prove worn-headset aim,
  unit selection, construction, perceived sharpness or sustained HQ fps.

P7.4 XR APK SHA-256:
`8fd45a83180327e7f555ad5580c0042b2dbd570bd30ab59333c8375fc3610046`.
Android APK SHA-256:
`ef985c6303ebdb9497e7926bc8ce9d27445baf8a584d383353ef7faa2bf8b6cb`.

Next physical gate: enter an offline match, hold left trigger and press Y
for stereo; aim the **right** controller at a bulldozer, press/release right
trigger, then point at free ground and press/release again. Confirm selection
and movement, then building preview/placement/cancel. Check near-board and
higher controller poses, UI/legend crossings, focus loss and board movement.
Assess 1536-eye clarity and frame pacing on the same map. Revisit the clipped
popup once the exact action/content can be reproduced.

Live follow-up during this turn: process 12484 entered an actual offline
stereo match. Both high/low center probes report HIT; live pointer windows
include 180 controller hits plus two probe hits, with occasional genuine
terrain misses. Multiple `P7.4 spatial commit` events reached the existing
input path (for example pixels 357/363 and 716/325). This proves dispatch,
not which unit was selected or whether a particular command succeeded.
Actual eye allocation is **1536 x 1609**. Several sampled renderer counters
report approximately **90 fps** in this scene; sustained load/thermal and
perceived clarity acceptance remain open. No shader compile failure appeared
in the inspected new-session output. The checked-in terrain-test build script
also compiled successfully; its three inherited compatibility/WWMath warnings
do not affect the seven passing ARM64 cases.

User headset feedback after the live P7.4 test: **"Es laeuft hervorragend!"**
P7.4 is therefore the user-accepted working tabletop baseline. Retain this
version's ray/input/quality behavior for subsequent graphics work. The reply
does not individually certify the complete construction/production checklist
or long-duration thermal behavior; do not silently close those separate gates.

## 24. P8: World graphics, overview, hover cards and workspace menu

### Accepted scope and implementation

The user explicitly requests all four proposed graphics steps, build-option
hover descriptions, a wider map overview and a larger physical table. They
also prefer the Tiberian Dawn Android/Quest on-demand UI menu pattern over
the persistent floating controller legend. The sibling implementation was
inspected read-only; no Tiberian Dawn code was changed.

1. **Grounding and shadows:** the stereo draw path now includes the complete
   projected/decal and stencil-volume sequence. Each eye has its own packed
   depth/stencil target. Volume geometry is not cut by the tabletop clip box,
   which would unbalance stencil faces. Shadow color preserves destination
   alpha, so a shadow cannot create a black patch in the real room. Prepare
   shadow resources in the XR process before stereo is enabled; force render
   settings only within a stereo frame and restore normal settings afterward.
2. **Spatial feedback:** selection decals, existing native movement markers
   and building/placement geometry enter the expanded stereo draw path.
   The previously empty attack-feedback implementation gets an XR-only red,
   terrain-following ring, driven by actual attack messages (not hover or
   synthetic simulation commands), expiring after 32 game-client frames.
   Reset/destruction releases its scene objects. Construction rules, validity,
   queues, costs and simulation remain the original engine's responsibility.
3. **Visibility and map coverage:** a head-centered rendering camera supplies
   billboard orientation and render traversal. Gameplay/UI keep the tactical
   camera. Board-volume culling replaces its narrow visibility footprint,
   preserving hidden/shrouded-object checks. Quantized terrain allocations
   expand coverage up to 513 cells, while game-space table span is bounded
   to 200–3000 units. Spatial picking no longer rejects targets solely for
   lying outside the old logical camera rectangle.
4. **Effects:** sorted and other depth-tested perspective world draws are
   mirrored for both eyes. Camera-space PointGroup vertices are transformed
   back to world space using the rendering camera. Alpha-tested vegetation
   and blended particles retain their material behavior. Ordinary orthographic
   UI stays on its separate panel. Per-effect visual correctness, especially
   dense smoke/explosions, remains an explicit device gate.

### User-facing workspace controls

- A small **UI** button next to the menu/build window replaces the permanent
  controller instructions. Right trigger press/release opens an upright,
  world-locked **Workspace** panel; **Close** or **B** closes it.
- Select **Table/Display** or **Build window**, then change size, distance,
  height, pitch and yaw. **Grab / Arrange** retains the established controller
  arrangement mode; **Reset position** recovers a surface.
  Manual pitch disables snapping, so the chosen window angle survives release.
- **More map / Less map** changes the world span independently of
  the old tactical camera zoom, within 0.5x–3x. Right-stick zoom controls the
  same quantity in stereo. Script camera locks remain respected.
- Physical board width is 0.45–4.0 m; ordinary panels remain 0.45–2.5 m.
  Increasing table width also increases covered game area, rather than only
  stretching the same image. The final 3000-unit span cap still applies.
- Version-3 layout data stores zoom and placement atomically, accepting old
  v1/v2 files. The established filename is retained for migration; an older
  binary cannot read v3, so preserve the accepted v2 backup for rollback.
- **Tabletop on / off** remains opt-in for eligible offline matches. Making
  tabletop the default is recorded as a later user preference, not enabled
  silently by this patch.
- Build/command hover cards appear after 300 ms and use native localized
  title/description labels. Resolve actual visible push-button gadget data,
  including disabled options, and validate it against the command registry.
  Cards are not cropped into the command-bar texture. Original ambiguous
  LT+Y popup reproduction remains deferred, as requested by the user.

The menu is modal with respect to gameplay input, including empty menu areas.
Tracking/focus loss disarms held buttons; moving a held trigger between buttons
cannot activate a different action. Existing controller shortcuts remain.
Canvas text is rendered locally at 768x1024 for settings and 768x384 for cards,
using the system font rather than the previous bitmap legend alphabet.

### Validation and acceptance boundaries

- Native ARM64 build and both `zh`/`xr` APK builds succeed. Packaging reports
  inherited Gradle deprecations and unstripped `libmain.so`, not build failures.
- **1105 UBSan host checks** cover geometry, layout migration, camera profiles,
  presentation, world/picking and production input/menu routing. Of these,
  **438 checks also pass on Quest ARM64**, including the 4-m/v3 round trip.
- **48 actual Quest Adreno 740 GLES checks** pass: stereo/depth/board clipping,
  camera-space transformation, uncut stencil geometry and darkening without
  changing MR alpha. These use production shader snippets with fixture meshes;
  they do not certify the full game's shadow pipeline or combat effects.
- An attempted standalone Canvas fixture via `app_process` was killed by the
  device before rendering. No Canvas screenshot success is claimed from it;
  actual in-app UI readability still needs headset inspection.
- P7.4 rollback APK and pre-P8 layout are saved in the local verification
  directory `/private/tmp/generals-p8.tKAGDi` (temporary, not a portable release).
- P8 XR APK installed successfully on Quest serial `2G0YC5ZG9609PY`, preserving
  application data. APK v2 signature verifies and the installed `base.apk`
  matches SHA-256 `d432e5c33fa83230d6422eb454a009527cf60ddb3132ced4d171d4cf1af06b86`.
  Android `zh` APK hash is
  `692eee40164e169a22ae9a5ee1ddeb2892d2c0aa0733deea594182e3974bf356`.
  The launch attempt was intercepted by Horizon's
  `LaunchCheckControllerRequiredDialogActivity`; no new P8 game process had
  started at final inspection. Existing stderr describes the old P7.4 run and
  is explicitly not treated as new-build runtime evidence. Wake the controllers
  and launch the app to perform the pending visual checks.

Mandatory worn-headset check: open **UI**, tilt the build panel, close and
reopen; verify its tilt persists. Select a bulldozer, hover enabled/disabled
build buttons, place/cancel a structure, produce and move units, issue an
attack. Increase table size and map coverage, move the head around the edges,
and inspect shadows, selection rings, dust, smoke and explosions without
black flashes, popping terrain or loss of interaction. Check sustained
performance on a busy map; HQ eye capture remains at 1536-wide, not increased
again in this step. Do not declare P8 visually accepted from build logs alone.

## 25. P9: Tabletop body, unit control and preferred startup (2026-09-13)

### Implemented scope

- [x] A low framed board body beneath the actual stereo battlefield. Sampled
  terrain heights close the four cut sides; mitered trim and a closed underside
  follow table size, rotation and game coverage. No synthetic buildings, units
  or replacement terrain are introduced. Samples are spaced at approximately
  five game units; per-triangle seam accuracy remains a visual acceptance gate.
- [x] Terrain-following contrast rings for local infantry and selected/hovered
  objects; head-facing health bars for selected/hovered objects, with native
  health values and game depth occlusion. Names appear in the hover card near
  the build window. Obscured/dead objects are excluded. Decoration is bounded
  to 192 marked objects per frame; this is not an army-selection limit.
- [x] Four UI tabs: **Windows**, **Units**, **Groups**, **View**.
  Unit controls use existing selection/meta/context messages, not synthetic
  mouse drags or modified simulation rules. World-targeted palette actions
  enable the eligible stereo view; camera locks and modal screens block them.
- [x] Version-4 layout persists `startStereo`, health/ring/frame visibility,
  window poses and coverage, migrating v1/v2/v3 atomically. The saved view is
  applied once when an eligible offline match becomes interactive. It does
  not override a manual switch every frame. Normal rendering stays available.

### Unit controls: practical workflow

1. Open **UI → Units → Area: two corners**. Point at the ground and
   release the right trigger for the first corner; point at the opposite
   corner and release again. This is **two clicks, not a held drag**. A terrain-
   following rectangle previews the selection. The resulting army immediately
   returns to context orders. An empty selection area preserves the old army.
2. **Selection +/-** toggles individual army units. **Add area** adds another
   rectangle. These additive modes remain active until **Context command**
   or cancellation. Buildings retain native single-selection semantics: use
   **Select unit** for a building, not additive army selection.
3. In context mode, trigger-release on ground moves the selection; on an enemy
   it issues the native context attack. The explicit palette also provides
   **Move**, **Attack move**, **Force attack**, **Guard position**, **STOP** and
   **Scatter**. Right-grip cancellation exits a pending
   mode/rectangle/waypoint mode before ordinary cancellation or deselection.
4. **Waypoints on / off** enables native queued movement. Place successive
   destinations with context/move mode; toggle off, cancel or stop to end it.
   This does not claim arbitrary attacks, abilities or guard orders are queued.
5. **UI → Groups** provides ten numbered slots: choose a slot, **Save
   selection**, then later **Select group**, **Add to group** or **View group**.
   Membership and commands use the engine's hotkey
   squads. These are match groups, not global persisted armies. Shortcuts also
   select idle/next workers, next unit, hero, aircraft, matching types or all
   eligible mobile units across the map. Native unit caps remain enforced.
6. Construction, production queues, rally points, repair/garrison context and
   special powers retain the original command bar/engine rules. An armed
   ability or construction preview still commits through its established UI
   translator; it takes precedence over a palette world mode.

Selection UI flags and simulation selection messages are updated together.
Duplicates and capped-out units are not appended as newly selected objects.
Dead, contained, off-map, hidden or uncontrollable objects are excluded from
the manual army selection. The native `CanSelectDrawable` gains an optional
screen-UI occlusion bypass only for the already-routed spatial selection;
all existing ordinary callers retain the old default. The detached build panel
must not mask remote tabletop units through its old screen rectangle.

### Preferred view and appearance

Use **UI → View → Tabletop now**, arrange the table/windows and zoom, then
choose **Set as startup view**. The next eligible offline match starts in
stereo. **Normal view now** switches only the present view; **Restore normal
startup** restores normal startup. Health bars, unit rings and
board body can be individually disabled here. Existing physical size (up to
4 m), coverage (up to 3000 game units), free window tilt and fallback remain.
This is a user-selected default, not a silently enabled global change.

### Reproducible verification and deployment

- Native `cmake --build build/android-vulkan --target z_generals -j 6` succeeds.
  Both `zh` and `xr` debug APKs package successfully; inherited compile/Gradle
  warnings remain. The Quest APK has a valid v2 signature and ARM64 `libmain.so`.
- **3258 UBSan host checks** pass across the 13 XR suites. New coverage includes
  tactics rectangle/cap/reset policy, every command/group menu action, blank
  menu handling, v4 preference round trips/rejection and board geometry/budget.
- **2278 checks pass on Quest ARM64** (placement, menu routing, tactics, board).
  The separately compiled production terrain `Cast_Ray`/WWMath fixture also
  passes all seven collision cases on the same Quest.
- **65 actual Adreno GLES checks** pass using production shader text: existing
  stereo, clip, particle and shadow regressions plus opaque frame coverage,
  clipped feedback, permitted frame extension and front/back depth occlusion.
  Fixture geometry does not prove actual in-game orders or complete rendering.
- P9 XR APK SHA-256:
  `4fc86b3510c331875beb48aca4370c428e3ee22f484ac024d8dbea329853460d`.
  P9 Android APK SHA-256:
  `e6deca366d7b64d294a5a381379973b799aa773873c5c95d1dfe94927e157c93`.
  Quest serial `2G0YC5ZG9609PY` installation succeeds with app data preserved;
  installed `base.apk` has the exact XR hash above.
- Accepted P8 APK and pre-P9 v3 layout are retained in
  `/private/tmp/generals-p9.iCCmuI/accepted-p8.apk` and
  `/private/tmp/generals-p9.iCCmuI/accepted-p8-layout.cfg`. These are temporary
  local rollback files, not a portable release. Restore both for a P8 rollback:
  P8 cannot parse v4. Game assets and saves were not cleared.
- The launch request reaches `LaunchCheckControllerRequiredDialogActivity`;
  no P9 process is running at verification. Existing P8 stderr is not new-build
  evidence. Wake controllers and launch to perform the following device gates.

### Required worn-headset acceptance

- [ ] Select three units with two corners, move them together, attack, stop,
  add/remove one unit and repeat beyond the old flat viewport boundaries.
- [ ] Save and recall two distinct groups; add one to another; center the view.
  Select an idle worker and place/cancel a building through the original UI.
- [ ] Queue three movement destinations; verify traversal and cancellation.
  Test attack-move, forced attack and guard against actual native behavior.
- [ ] Verify no commands leak through menu tabs/background or tracking loss.
  Test construction, production, special abilities and single building control.
- [ ] Inspect desert infantry, sloping/raised terrain cut sides, hovering health
  bars and board underside at small/large sizes; check clipping and flicker.
- [ ] Save tabletop startup, restart, verify window tilt/coverage/view; return
  to normal startup and verify the ordinary fallback.
- [ ] Measure sustained busy-map performance and real Canvas menu readability.
  This build is an implementation checkpoint, not full campaign/multiplayer or
  worn-headset acceptance. Online stereo remains outside the existing scope.

## 26. P10 ready for morning headset validation (2026-09-13)

### Implemented and locally verified

- [x] Direct trigger-drag selection in stereo context mode. A short click keeps
  the existing native context action. Holding and moving the ground ray by
  2 cm in physical table space previews a rectangle; releasing selects units
  without issuing a movement order. Crossing the threshold on the release
  sample itself still counts as a drag. Brief sub-threshold motion stays a click.
- [x] Left grip captured at trigger-down means additive drag or toggle selection
  on a short click. Releasing the modifier early does not change the gesture.
  Incompatible building/army selection is replaced rather than silently refused;
  native multi-building restrictions and unit caps still hold. Additive direct
  gestures return to context mode, so the next click can issue an order.
- [x] Armed construction, abilities, explicit targeted orders and queued
  movement bypass automatic box selection. Tracking/terrain loss, camera motion,
  UI crossing or cancellation discard pending drag intent and preview. Only a
  valid fresh trigger gesture commits; no raw mouse drag is synthesized.
- [x] Persistent **Commands** console with 16 directly reachable actions and ten
  group buttons. Native orders, selection squads and original build/ability UI
  remain authoritative. Console clicks/background capture only the ray over
  the console, not the rest of the game. Presses originating elsewhere cannot
  activate it on release; B still reaches pause while pointing at the console.
- [x] A directly toggles console visibility in eligible live play. A retains
  arrangement in the shell/demo and exits existing arrangement. In a live game,
  **UI → Windows → Grab / Arrange** remains the explicit layout entry point.
  The separate console is docked to the build panel: its offset follows that
  panel's free position/tilt, with a fixed 0.72 m width. It is not a separately
  grabbable fourth room anchor in this checkpoint. An adjacent **COMMANDS**
  button also toggles it without opening UI settings.
- [x] v5 layout persists console visibility and migrates older layouts, keeping
  all existing poses/tilt/zoom/startup preferences. Default visibility is on.
  One-shot group save/add/center modes reset on use, hiding, settings entry or
  match changes, preventing a later accidental group overwrite.

### Morning test sequence

1. Connect Quest and identify its serial with `adb devices -l`. Before updating,
   back up its current `files/xr-layout-v2.cfg` with `run-as`; the device was not
   available to take that backup during P10. Preserve game assets and saves.
2. Install `build/quest-p10/Generals-Quest-P10.apk` with `adb -s SERIAL install -r`.
   Verify installed `base.apk` hash. Wake the controllers and start
   `com.generalsx.zerohour.xr/com.generalsx.zerohour.XrHelloActivity`.
3. Enter an offline match/stereo mode. Without opening UI, hold the right
   trigger on the ground, draw a rectangle over three units, release. The
   rectangle and console should report selection, not movement. Click a ground
   destination; the selected units should move together.
4. Left grip + drag adds units. Left grip + click toggles an individual unit;
   test with a building selected first. Small normal-click motion must not
   accidentally select a box. Test a drag with a brief UI crossing or lost
   tracking and confirm it cannot issue an order on release.
5. Use the visible **STOP**, **Attack move**, **Guard position** and worker
   buttons directly. Select **Save**, then group **1**; change selection and
   click **1** to recall. Test **Add** and **Center** likewise.
   The group modifier is one-shot; ordinary numbered clicks recall immediately.
6. Test three queued movement points, original construction placement/cancel
   and an armed special ability: those gestures must not become selection boxes.
7. Press A to hide/show the console; B to pause while pointing at it. Adjust the
   build window's tilt and verify the docked console remains usable. Restart
   and check console visibility, startup view and old window layout are retained.

### Build/test evidence and artifacts

- Native ARM64 `z_generals` build succeeds; both Android `zh` and Quest `xr`
  APKs package and pass v2 signature verification. Existing compiler/Gradle
  warnings remain, not new build failures.
- **3399 UBSan host checks pass** across 14 suites, including 196 production
  menu/console routing checks and 26 checks compiling the actual
  `XrGameBoot_SpatialTrigger` function verbatim against engine spies. The latter
  verifies that box/drop, modifier-click, armed placement/order, queue and
  cancellation select the intended existing command route, not actual unit AI.
- Six focused fixtures also compile for Android ARM64 (input, interaction,
  placement, menu routing, tactics, production trigger bridge). They were
  **not executed on Quest** in P10. No new GPU or Canvas visual pass is claimed.
- Quest APK: `build/quest-p10/Generals-Quest-P10.apk`, SHA-256
  `2fdd81ff1943bf015f2fba5d62f6a866f3777302816013c1bcdef40bc481d23c`.
- Android APK: `build/quest-p10/Generals-Android-P10.apk`, SHA-256
  `38a00e797f269764f7d9f0302d8cc78eaa687f25dec24b6335b2afad9a6d4aea`.
- Prior P9 APK is preserved as `build/quest-p10/rollback-P9.apk`, SHA-256
  `4fc86b3510c331875beb48aca4370c428e3ee22f484ac024d8dbea329853460d`.
  Restore its pre-update v4 layout too if rolling back: P9 cannot read v5.
  The future device backup must be taken before installing P10; do not overwrite
  current user positions with an older P8 layout merely because one exists.
- Artifacts are local generated build outputs, not a release/source archive.
  Final verification logs are also copied into `build/quest-p10/`.
- [x] Installation and APK identity verified on 2026-09-14, serial
  `2G0YC5ZG9609PY`; the installed hash matches the P10 hash above.
  Actual pre-update layout is v3 and is preserved as
  `build/quest-p10/pre-p10-layout-20260914.cfg` (320 bytes). Use this current
  backup for rollback instead of assuming the previous app wrote v4.
- [ ] Runtime startup: Horizon controller-required dialog blocks launch;
  no P10 process was running at inspection. Wake controllers and launch.
- [ ] Worn-headset drag comfort, direct-command correctness and readability.
- [ ] Busy-map performance and all inherited graphics/gameplay acceptance gates.

## 27. P10.1 — relaxed picking, real map pan, handedness

### Confirmed source findings and implementation

- [x] `W3DView::pickDrawable` unconditionally refused hits behind the legacy
  opaque screen GUI, although detached XR panels already owned ray priority.
  Bypass that *screen* gate only for the active spatial pointer token. Ordinary
  mouse/touch and detached-panel first refusal remain unchanged. The spatial
  model ray is terrain-clipped, matching the visible laser's hill occlusion.
- [x] `TouchInput::tap` also used the default screen-UI selection gate, unlike
  explicit XR selection modes. Add an explicit optional flag, false by default;
  only the spatial context route opts out. Native selectability, local control,
  selection groups, simulation messages and ordinary Android rules remain.
- [x] A model hit is now valid without terrain behind it at the board edge.
  Ground is still required for terrain orders and box selection. No raw
  `Drawable*` is retained across frames: a short click re-picks using its captured
  press ray/pixel, then restores current hover state. No synthetic order at press.
- [x] Replace the grazing-terrain 2 cm classification with a 2 cm displacement
  on a plane facing the initial controller ray. Depth is bounded to 0.25–1.25 m;
  all three room-space axes count. This avoids amplifying small pitch jitter
  into a false rectangle. The actual rectangle corners still use native world
  terrain. Drag cancellation, targeted orders and armed placement retain priority.
- [x] Move support-stick stereo pan before nonmodal console pointer capture.
  Keep modal workspace settings, focus loss, rearming, arrangement, demo mode
  and native camera locks blocking navigation. Rearming precedes console capture,
  so aiming at the console cannot prevent neutral controls from rearming.
- [x] The old `scrollBy` delta was converted on a z=-1 view plane and produced
  near-invisible map movement. Map pan now uses the rendered board's world basis
  and span (half a visible board width per second on one full cardinal stick),
  dispatched through `userLookAt`, preserving native camera control/constraints.
  This moves the map within the table, not the physical table in the room.
- [x] Bind both physical Touch controllers once and map them to dominant/support
  roles in a pure helper. **UI → View → Left-handed: ON/OFF** switches roles,
  closes the menu, cancels grabs/pointer intent and requires buttons/sticks neutral.
  Edge-triggered shortcuts also check their held state before rearming.
- [x] Layout v6 stores handedness; v1–v5 migrate to right-handed without changing
  saved poses, tilt, zoom, startup view or console visibility. Footer hints reflect
  the selected hands. Physical left Menu remains Back; the Meta system button is
  not remapped. No changes to network rules, unit AI, assets or graphics quality.

### Controller mapping

| Role | Right-handed (default) | Left-handed |
|---|---|---|
| Aim, select, trigger-drag | Right aim/trigger | Left aim/trigger |
| Add/toggle selection | Left grip at trigger press | Right grip at trigger press |
| Cancel/context secondary | Right grip | Left grip |
| Map pan | Left stick | Right stick |
| Rotate / zoom | Right stick | Left stick |
| Camera tilt modifier | Left trigger | Right trigger |
| Console toggle | A | X |
| Back / pause | B or left Menu | Y or left Menu |
| Recenter | X | A |
| View / explicit leveling | Y | B |
| Camera preset | Right stick click | Left stick click |
| Stereo toggle chord | Left trigger + Y | Right trigger + B |
| Layout grabbing/scaling | Either/both physical grips | Either/both physical grips |

Existing modifier chords follow these roles too. Right-handed behavior remains
the default. The console itself stays docked to the freely tilted build window.

### Verification and artifacts

- Native `z_generals` ARM64 build and both Android `zh` / Quest `xr` APKs pass;
  both APKs verify with v2 signatures. Existing compiler/Gradle warnings remain.
- **3683 UBSan host checks across 16 suites** pass. New coverage includes 260
  hand-role/grazing-ray/pan/layout checks, 12 checks compiling production
  `W3DView::pickDrawable` verbatim with spies, and extended actual trigger/menu/
  interaction routing tests. Spies verify dispatch, not actual gameplay or visuals.
- **2735 ARM64 checks on Quest 3 `2G0YC5ZG9609PY`** pass across eight focused
  suites: input, interaction, placement, menu routing, tactics, comfort,
  production trigger bridge and production pick bridge. Device test binaries
  are isolated under `/data/local/tmp/generals-p101-p1S4wG`.
- Quest APK: `build/quest-p101/Generals-Quest-P10.1.apk`, SHA-256
  `6682741fb7f9fca7adb64ea7ee03fc72feb9919a525ed4ff4530d825312e3088`.
- Android APK: `build/quest-p101/Generals-Android-P10.1.apk`, SHA-256
  `e7bdc2fa5dd627ae96ddd1e8e1680da88f93908fdece4542a58fa7b6ea09d195`.
- P10 rollback: `build/quest-p101/rollback-P10.apk`, SHA-256
  `2fdd81ff1943bf015f2fba5d62f6a866f3777302816013c1bcdef40bc481d23c`.
  The actual current v5 layout (356 bytes) was captured before installing as
  `build/quest-p101/pre-p101-layout-20260914.cfg`. Restore this alongside P10
  if rolling back after P10.1 writes v6; P10 cannot read v6. Preserve game data.
- Build logs are retained in `build/quest-p101/`; these are development artifacts,
  not a published release or source snapshot.
- [x] Installed with `adb -s 2G0YC5ZG9609PY install -r` on 2026-09-14;
  installed `base.apk` SHA-256 exactly matches the Quest P10.1 hash above.
- [ ] P10.1 runtime startup: the new launch at 07:56 local time is intercepted
  by Horizon's `LaunchCheckControllerRequiredDialogActivity`; `pidof` finds no
  game process. Wake both controllers, then launch. No P10.1 OpenXR session,
  runtime binding or worn-headset acceptance is claimed. Startup log retained.

### Worn-headset acceptance (still open)

1. In an offline stereo match, keep elbows relaxed. Point at near and far own
   units/buildings; short-click them at shallow angles and near table edges.
   The visible target at press should be selected, not a phantom empty box.
2. Drag deliberately over several units; release selects the rectangle. Add
   with support grip, issue movement, stop, build and target an ability. Cross
   a panel or lose tracking mid-drag: no order should escape on release.
3. Pan cardinally and diagonally with the support stick while aiming at ground,
   empty air, the build panel, and the Commands console. Confirm visible map
   movement, sensible speed/direction after rotation and normal map-edge limits.
   A modal UI settings panel and pause/camera locks must prevent pan.
4. Toggle left-handed mode, release both controllers, then point/select/drag
   with the left trigger; pan with the right stick. Check additive right grip,
   X console, Y cancel/pause, right-trigger tilt and both-hand layout grabs.
   Switching with held inputs must not trigger a new click or command.
5. Restart: handedness and existing window poses/tilt/zoom must survive. Switch
   back to right-handed and repeat. Both Android touch and ordinary combined
   Quest menus must retain their old GUI selection behavior.

No worn-headset or exhaustive order/performance acceptance is inferred from
the build, test-fixture execution or successful package installation.

## 28. P10.2 — photo defaults, high-quality eyes, full native UI

### Implemented scope

- [x] Derive a rounded default from the photo and the user's actual saved v6
  layout: 1.65 m level table centered 0.54 m below / 0.55 m ahead of the launch
  head pose; 1.80 m build bar centered 0.38 m below / 1.08 m ahead, pitched
  24 degrees toward the player. Console remains docked on the left at 0.72 m
  width, with a 45-degree inward yaw, slightly raised and brought forward.
  No real-world surface detection or exact room anchor is inferred from the photo.
- [x] New installs and a **one-time v1–v6 upgrade** apply these board/build
  poses, show the command console and select tabletop startup. Existing shell
  pose, map zoom, handedness, camera favorite and appearance choices remain.
  After saving v7, later user poses and normal-start choices are not reset.
  **UI → View → Photo arrangement** restores the arrangement on demand.
  Intro/shell remain conventional; stereo startup applies to supported offline
  skirmish/campaign matches, not unsupported network/replay stereo.
- [x] v7 adds persisted **Resolution: High / Balanced** under **UI → View**.
  High uses a 1920-pixel eye-width budget (previously 1536), keeping the runtime
  aspect and existing 2048 maximum dimension. Typical Quest aspect yields 25%
  more pixels per axis / about 56% more area. Balanced is the exact old extent.
  Native game/UI canvas remains 1280×720; original assets are not replaced or
  upscaled. No promise of new texture detail, frame rate or native panel resolution.
- [x] Native quit menu is **not necessarily modal**; the old modal-only policy
  missed it. Detect actual quit visibility, modal stack and visible options
  layout without creating windows. Quit's save/load submenu inherits its active
  quit state. Their UI uses the **entire 1280×720 source canvas**, expanding
  upward from the compact bar's existing bottom edge; after close, it contracts.
  The upper transparent piece is omitted during expansion, avoiding duplicate
  content/zero-height geometry. Render and pointer share these exact transforms.
  The stereo board remains behind split-compatible dialogs; shell/load/movie
  transitions retain the existing conventional full-frame fallback.
- [x] Dialog geometry transitions require neutral controls. Dialogs block
  world picking, commands, camera motion, stick shortcuts and console actions;
  native pointer/back continue to reach the real menu. No native dialog widget,
  pause rule, save format or game command protocol is reimplemented.
- [x] Console's second status line explicitly asks for a target after selecting
  Guard, Move, Attack Move or Force Attack. One-shot group operations still have
  priority in that line. **Select unit(s) → Guard position → click a terrain
  position** dispatches the existing native guard-position order. STOP and
  Scatter act immediately; guard is not an immediate stop command.

### Validation and handoff

- Native `z_generals` ARM64 build and both APK flavors pass; both v2 signatures
  verify. Existing compiler and Gradle deprecation warnings remain.
- **4091 UBSan checks across 17 host suites pass**, including 400 new tests
  compiling the actual dialog predicate and display/crop functions with spies.
  They cover full-canvas corner hit mapping at different tilts/crops, bottom-edge
  continuity, migration only once, v7 corruption rejection and quality limits.
- **3356 checks across ten ARM64 fixtures pass on Quest 3 `2G0YC5ZG9609PY`**.
  **73 actual GLES checks pass on Adreno 740**, including simultaneous two-eye
  color/depth/stencil allocation and far-corner readback at High → Balanced →
  High, plus inherited stereo alpha, clipping, shadow and depth regressions.
  These are controlled fixtures, not real native quit/options screenshots or
  sustained live-match GPU profiling.
- Quest APK: `build/quest-p102/Generals-Quest-P10.2.apk`, SHA-256
  `22ad13e5f31dd29c06d3fb16c86e1fb932a32d00d426c6ca10ed7cebc62d7400`.
- Android APK: `build/quest-p102/Generals-Android-P10.2.apk`, SHA-256
  `1b2a494be84ffe9a77693d6e816962c291854f149c0c34a1b60a3223bc9bcb86`.
- P10.1 rollback: `build/quest-p102/rollback-P10.1.apk`, SHA-256
  `6682741fb7f9fca7adb64ea7ee03fc72feb9919a525ed4ff4530d825312e3088`.
  Restore `build/quest-p102/pre-p102-layout-20260914.cfg` (363-byte v6 captured
  from this device before update) alongside it: P10.1 cannot parse v7. No app
  data was cleared, no assets changed, and no release/commit/push was made.
- Installed successfully on Quest 3 `2G0YC5ZG9609PY`; SHA-256 read from the
  installed `base.apk` exactly matches the Quest artifact above. Horizon OS
  intercepts the launch with `LaunchCheckControllerRequiredDialogActivity`;
  no game PID is present. Wake the Touch controllers to continue. This is an
  installation verification, not a successful game-start or worn-headset test.
  Captured evidence is retained in `build/quest-p102/startup.log` and
  `build/quest-p102/activities.log`; no controller requirement was bypassed.

### Required physical checks

1. Start an offline match. Confirm automatic stereo, centered flat table,
   tilted build bar above its rear edge and readable inward-facing console.
   Move/tilt a panel, restart, and ensure v7 does not reset the new adjustment.
2. Open the **native game-menu button** in the build panel; inspect all corners
   of quit, options, save/load and confirmation dialogs. Click controls near
   their top/bottom edges. Close them: compact bar and board layout must return.
   No camera movement or world command may leak through a dialog.
3. Compare **Resolution: High / Balanced** on infantry and map edges, then
   test a busy scene for sustained smoothness. Use Balanced if High stutters.
   A successful allocation/readback is not evidence of sufficient GPU frame time.
4. Select an army unit → Guard position → click terrain. Confirm the hint and
   actual response; repeat Attack Move, then STOP. Recheck additive drag,
   handedness and building/ability targeting for input regressions.

## 29. P10.3 — upright command console, native communicator, groups and languages

### Implemented

- [x] The console follows the build window's docking position, but strips its
  pitch and roll before the 45-degree inward yaw. Its local up vector is world
  up; tilting the build window no longer leans the console. Table/build poses,
  physical sizes, rendering quality and user camera preferences are unchanged.
- [x] `ControlBarSystem` routes the original Communicator button to
  `ToggleDiplomacy(FALSE)`. This is a nonmodal `Diplomacy.wnd`, not the old
  `PopupCommunicator` class. `IsDiplomacyVisible()` exposes its actual window
  visibility throughout animation; XR expands its entire UI canvas and blocks
  world input just like quit/options. A second direct **Communicator** button
  on the command console opens the same original window. Its contents remain
  native: briefing/mission information or multiplayer player/chat controls;
  no online sign-in, messages or network permissions are added. Offline
  skirmishes need not have mission briefing text. Stereo remains offline-only.
- [x] Group buttons show `number · living members`, including zero. Empty
  recall explains how to save and preserves the current selection. Saving an
  empty selection is rejected. A selected save/add/center operation is marked
  with `>` and an explicit next-number prompt; result/error text stays visible.
- [x] **Save selection → number** creates/replaces membership. **Number alone**
  recalls. **Add to selection → number** adds an existing group's members to
  the current selection; it does NOT save new members into that group. To
  extend: select new units → Add to selection → number → Save selection → same
  number. This reuses native meta/network messages, not a parallel group store.
  Groups are selection shortcuts, not fixed formations. UI 1–10 maps to the
  engine's zero-based slots 0–9 (the last corresponds to the keyboard's 0).
- [x] Center view directly uses the native user camera operation for all ten
  slots. The shared legacy `onMetaViewTeam` wrongly excludes slot zero with
  `group >= 1`; the bounded XR camera path avoids changing the shared translator.
- [x] Direct **Help** opens three pages in the same command console: group
  creation/extension, targeted/immediate orders, selection/controllers and
  communicator. No workspace-settings submenu is needed. Help text cannot
  trigger invisible command buttons; rendering and ray geometry share the
  footer/close hit regions.
- [x] A gettext-style `XrStrings.h` catalog translates custom panels, tabs,
  commands, status, errors and help between German/English. Android Canvas
  receives localized payloads, including the labels previously hard-coded in
  Java. **UI → View → Language** changes XR immediately; v8 layout persists it.
  v7 → v8 never repeats P10.2's pose reset. Unknown/native asset labels are not
  guessed or translated by substring replacement. Catalog extensions need a
  supported language value, parser bounds, entries/help and coverage tests.
- [x] If `data/<language>/generals.str` or `.csf` exists in the engine VFS,
  language selection atomically stages the existing per-package
  `game_language.cfg`. XR boot now honors `GENERALSX_TEXT_LANGUAGE`, as the
  Android boot already does. Original string tables switch only on restart;
  no live cached-window rebuild or automatic restart of a match occurs. Missing
  packs do not overwrite the last valid native preference. Error/missing-pack
  status is shown. Audio, movies, artwork and SKU language are unchanged.

### Original command coverage audit (source inspection, not exhaustive gameplay acceptance)

| Category | Quest access / boundary |
|---|---|
| Selection, additive selection, drag box | Direct trigger/support-grip; two-corner modes also remain in workspace UI |
| Move, attack/context, attack move, force attack, guard position, stop, scatter | Direct context or console; targeted modes require the next world click |
| Waypoint movement | Console toggle; not proof of every original path-building/queued-ability gesture |
| Ten groups: replace, recall, add to selection, center | Direct console; original membership/messages retained |
| Next unit/worker, idle worker, hero, aircraft, matching type, all units | Console; all-units excludes structures, matching type is across-map rather than a separate screen-only shortcut |
| Construction, production, upgrades, sell, evacuate, unit abilities and generals powers | Original context-sensitive build bar/power UI and existing targeting path; per-faction ability/placement/queue coverage still needs real-game testing |
| Capture, repair, enter/garrison, transport interactions, rally points | Existing native context evaluation or original command buttons; not separately reimplemented by the console |
| Communicator / diplomacy, quit/options/save/load | Native windows, now full canvas; text entry/online chat and every dialog still require headset acceptance |
| Fixed formation (`MSG_META_CREATE_FORMATION`) | Native implementation exists in `CommandXlat`; no dedicated Quest action yet |
| Force move (`MSG_META_BEGIN/END_FORCEMOVE`) | Native mode exists; ordinary Move is not equivalent; no dedicated Quest action yet |
| Guard object / pursuit variants | Native command paths exist; explicit console mode currently issues guard POSITION only; original exposed GUI variants are separate |
| Camera bookmarks 1–8, camera tracking, replay controls | No equivalent complete Quest shortcut set; saved tabletop camera/pose and navigation are separate features |
| Beacon/chat shortcuts, cheer, observer/online controls | Not a complete controller-only mapping; live stereo networking/replay intentionally remains gated |
| Generic `MSG_META_DEPLOY` / `MSG_META_FOLLOW` | Already unimplemented TODOs in the inspected native translator; do not count enum names as working retail commands or add inert buttons |

Evidence: `XrGameBoot.cpp` tactical/spatial bridge; `SelectionXlat.cpp`
`onMetaCreateTeam/onMetaSelectTeam/onMetaAddTeam/onMetaViewTeam`;
`CommandXlat.cpp` meta cases; `MessageStream.h`; `ControlBarCallback.cpp`
Communicator dispatch; `Diplomacy.cpp`; `GameText.cpp` text-only loader.
This is an access-path inventory, not a claim that every original unit command
has been played through on Quest.

### Validation and deployment

- Native ARM64 build and both Android/Quest APK packages pass.
- **4931 UBSan host checks across 19 suites pass**. New production bridge
  checks exercise group slots 0–9, empty/no-selection/locked state, center view,
  missing language packs, atomic marker writes and catalog coverage. Production
  panel generation is compiled verbatim to check all localized Canvas payloads.
- **4133 checks across 11 ARM64 fixtures pass on Quest 3 `2G0YC5ZG9609PY`**:
  world 214, placement 106, interaction 80, input 27, menu routing 256, comfort
  260, tactics 2028, workspace 418, console bridge 704, trigger 28, pick 12.
- **26 panels rendered with actual Android Canvas**, including all six help
  pages in both languages. Help measures 459–558 px high in an 820 px region
  at its normal 26 px font. Inspected German/English console, group help and
  view settings images. The initial standalone `app_process` attempt lacked
  Android typeface initialization and failed; it is NOT counted as passing.
  A permission-free isolated Activity fixture passed. The temporary
  `com.generalsx.xr.panelqa` app was uninstalled after preserving its images.
  These are flat Canvas fixtures, not worn-headset screenshots or stereo tests.
- Device data inspection: `EnglishZH.big`, English audio/speech archives and
  `Data/English` are present; no `Data/German` directory or German-language
  archive is present at the inspected game-data root. Full German original
  game text therefore needs user-supplied language data. No assets downloaded,
  translated in place, replaced or cleared. The existing Android Setup language
  data workflow remains available; launcher locale is an independent setting.
- Quest APK: `build/quest-p103/Generals-Quest-P10.3.apk`, SHA-256
  `b0c073cdef29a0ae800b808e07f5fdee38626ddcb57fbf60a1ee067964667844`.
  Installed successfully; hash read from installed `base.apk` matches exactly.
- Android APK: `build/quest-p103/Generals-Android-P10.3.apk`, SHA-256
  `fe3ee79fc0168eff25d1d129363f972bdd93f76b1defdbfca97266d1a295daea`.
  Both APK v2 signatures verify. Android flavor built, not installed on a phone.
- Startup is intercepted by Horizon OS's
  `LaunchCheckControllerRequiredDialogActivity`; no game PID is present. Wake
  Touch controllers for acceptance. No game launch success or physical test is
  claimed. See `build/quest-p103/startup.log` and `activities.log`.
- Rollback APK: `build/quest-p103/rollback-P10.2.apk`, SHA-256
  `22ad13e5f31dd29c06d3fb16c86e1fb932a32d00d426c6ca10ed7cebc62d7400`.
  Restore the captured 332-byte `pre-p103-layout-v7.cfg` alongside it after v8
  has been saved. No game data cleared and no commit/push/release made.

### Required headset checks

1. Confirm console is upright while the build window stays tilted; test after
   custom rotation/tilt, recenter and app restart. No manual layout reset needed.
2. Open Communicator from both the native button and the new console button;
   read/click its full area and close it. Confirm compact HUD returns and no
   order or camera motion leaks through its animation/visibility transition.
3. Select several units → Save selection → 1. Verify member count, recall after
   another selection, add-to-selection then re-save, and center groups 1 and 10.
   Review Help without accidentally issuing commands.
4. Switch language in UI → View: check immediate XR labels/help/status, restart
   persistence, and preservation of all window poses. Verify native text on a
   restart only when that pack is present; missing-pack status must be explicit.
5. Recheck build/ability targeting, ordinary selection, left-handed controls and
   P10.2 high-quality performance. No shader or resolution budget changes in P10.3.

## 30. P11 — direct tactics, map views and explicit multiplayer follow-up

### Scope and implementation (2026-09-14)

The user authorized the useful next controller-access slice and explicitly
requested that multiplayer not be forgotten. This section supersedes the
P10.3 coverage audit for the commands listed below; it does not claim that all
original commands or online operation have been implemented/tested.

- [x] **Tactics + / −**, directly beside Help in the persistent command console.
  Expands downward from 768×1024 to 768×1280. Existing controls retain their
  physical size and top anchor, gravity alignment and inward heading. There is
  no settings submenu. Foldout is transient, not a new saved layout format.
- [x] **Create / release formation** sends native `MSG_META_CREATE_FORMATION`,
  whose translator emits `MSG_CREATE_FORMATION`. It retains current relative
  offsets, not an invented line/wedge pattern. Show native selected formation
  state. New formation requires at least two valid own AI-controlled mobile
  units; a remaining single formation member can release it. Mixed formation
  IDs are disabled with a reason: native `AIGroup::getMinMaxAndCenter` currently
  uses the first member's formation state and does not correctly compare all
  subsequent IDs. Do not silently change that shared simulation rule here.
- [x] **Force move → ground target** uses the native context evaluator with
  force-move enabled and target object suppressed; its existing translator
  emits `MSG_DO_FORCEMOVETO`. Not equivalent to ordinary Move. Save/restore all
  waypoint/force-attack/force-move/attack-move flags around the call. Arming this
  mode exits queued movement. Enabling Waypoints explicitly returns to Move.
- [x] **Guard position** now accepts ground OR another living, visible allied
  object. Native `MSG_DO_GUARD_POSITION` / `MSG_DO_GUARD_OBJECT` with normal
  guard mode, including native voice response. Enemy, selected/self, shrouded,
  contained, off-map and dead targets are rejected, not silently converted to
  ground orders. Mobile guarded objects can be escorted by native AI behavior.
- [x] **Guard: no pursuit → ground target** emits native guard-position with
  `GUARDMODE_GUARD_WITHOUT_PURSUIT`. No new AI/simulation/network message type.
- [x] **Save view → A–D**, then **A–D alone** to recall. Four session-local XR
  slots use the same native `ViewLocation`, `getLocation` and `userSetLocation`
  as the original camera bookmarks. Filled/empty indicators and explicit
  save/recall/empty feedback. Empty recall cannot jump the camera. Script locks
  and modal UI gate save/recall. Reset when leaving eligible offline play or
  detecting simulation-frame rollback (restart/loading an earlier frame).
  No disk persistence, unit-group changes, physical table/window pose changes,
  or physical size changes. XR coverage zoom is not stored in a map bookmark.
- [x] Hover explanations for the new tactics, guard, map views and group slots.
  Invalid tactical selection is greyed out with a reason and cannot activate.
  Target modes tell the player what to click next; invalid guard targets retain
  target mode and report the error. Cancel clears targeting without issuing an
  order; focus/tracking loss also clears target/drag/queue state and pending
  console save operations, preserving the selected army.
- [x] New labels, statuses, reasons and a fourth direct Help page in DE/EN.
  Existing game asset text remains governed by P10.3 language-pack rules.
- [x] Original Android 2D controls, simulation rules, layout v8, saved poses,
  game assets, stereo resolution and rendering pipeline remain unchanged.

### Offline Skirmish versus multiplayer — retained commitment

**Offline Skirmish against AI is included now and is the user's primary test
mode.** `GAME_SKIRMISH` and `GAME_SINGLE_PLAYER` remain eligible for stereo
tabletop. This is separate from a human LAN/internet match, even if that match
also has AI opponents. `GAME_LAN`, `GAME_INTERNET` and `GAME_REPLAY` retain the
current stereo gate; this change does not enable or certify them.

Explicit future milestone **QTR-MP: controller-complete human multiplayer**:

- [ ] Audit native networking/relay availability and controller-only join,
  lobby, faction/team/AI slot selection, ready/start, failure and reconnect.
- [ ] Chat/team chat with Quest text input, focus ownership, send/cancel and
  explicit recipient; no game commands while typing. Communicator/diplomacy
  player controls must have complete visible panels and controller access.
- [ ] Place/cancel map beacons and select recipients where supported; separate
  their targeting mode from movement, guard, construction and special powers.
- [ ] Observer/replay camera and playback controls only in modes that permit
  them. No observer actions for active players; no bypassing fog/ownership.
- [ ] Verify native selection/order messages against mouse/Android clients,
  lockstep/determinism, pause/load/quit transitions, disconnects and repeated
  human matches. XR presentation must not change shared rules or wire formats.
- [ ] Only then evaluate removing the network/replay stereo gate and perform
  actual worn-headset multiplayer acceptance. Offline build/tests are not proof.

Other deferred useful access: camera tracking, waypoint/route visualization,
flying-only guard variants, remaining mode-specific shortcuts. Generic native
Deploy/Follow TODOs do not become inert buttons. No online messages, accounts,
server changes or network-game actions are performed by P11.

### Validation and deployment

- Native ARM64 build and both Android/Quest APK packages passed; both APKs
  verify with v2 signing. Existing legacy C++ warnings remain in build logs.
- **6148 UBSan host checks across 20 suites passed**: math 111, placement 106,
  interaction 81, camera 43, layers 64, presentation 348, diorama 30, world 214,
  input 27, menu 75, menu routing 273, tactics 2030, board 64, comfort 260,
  trigger 28, pick 12, workspace 418, console 824, panel text 117, tactical
  bridge 1023. Menu routing recompiled/rechecked after the final disabled-button
  activation guard. `git diff --check` passed for tracked changes.
- **5296 checks across 12 ARM64 fixtures passed on Quest 3 `2G0YC5ZG9609PY`**:
  world 214, placement 106, interaction 81, input 27, menu routing 273,
  comfort 260, tactics 2030, workspace 418, console 824, trigger 28, pick 12,
  tactical bridge 1023. Isolated binaries remain under
  `/data/local/tmp/generals-p11-0pcfwl`; no game assets accessed by fixtures.
- **38 actual Android Canvas panels passed**, with the final production text
  payloads. Includes DE/EN foldout, greyed-out commands, hover reasons, save
  state and all eight help pages. Help fits at normal 26 px font: 492–690 px
  within its 820 px region. Visually inspected expanded DE/EN, disabled DE and
  tactics help. The permission-free Canvas QA app was then uninstalled; PNGs
  and reproducible fixture sources are preserved in `build/quest-p11/`.
- Installed `Generals-Quest-P11.apk` with existing data retained. Installed
  `base.apk` SHA-256 exactly matches **caeed1aa288572c0b7f51ea46ddfae4906be8a18d28614b602466e018928d642**.
- `Generals-Android-P11.apk` SHA-256:
  **1ec8dc90077073e7403e91e92ed6a0a4a84afc054cf2ab3bda31e47783ed69cf**.
  Packaged/verified, not installed on a separate 2D Android device.
- Preserved actual installed P10.3 APK as `rollback-P10.3.apk`, SHA-256
  **b0c073cdef29a0ae800b808e07f5fdee38626ddcb57fbf60a1ee067964667844**.
  The actual saved layout was still **v7, 332 bytes**, not v8: preserved as
  `pre-p11-layout-v7.cfg`. Existing v7→v8 migration preserves these poses.
- Normal startup request is intercepted by Horizon OS's
  `LaunchCheckControllerRequiredDialogActivity`. Wake the Touch controllers
  before testing. Installed APK and isolated Canvas/ARM64 results are not a
  successful game startup, unit behavior or worn-headset acceptance claim.
- Evidence: `build/quest-p11/{host,device,canvas,native-release,package,signing,
  installed-hash,startup,activities}.log`, Canvas PNGs, fixture sources and
  generated bridge includes. No asset edits, game-data deletion, commit, push,
  remote issue or release. Multiplayer commitment also stored in the explicitly
  requested project memory note dated 2026-09-14.

Regression source: `scripts/qa/xr-tactical-bridge-test.{sh,cpp}` compiles actual
production movement/guard/formation/bookmark/cancel/action functions with spies,
not rewritten approximations. Existing gesture/selection/layout tests remain.

### Required worn-headset acceptance — open

1. Start an offline AI Skirmish. Expand/collapse Tactics; verify stable old
   button positions, reachable lower controls, inward/upright orientation and
   no clicks through the console. Try both language and handedness settings.
2. Select two tanks/infantry in a visible arrangement → Create formation → Move.
   Observe original formation behavior → Release formation. Compare groups:
   selecting group 1 alone must not create a formation.
3. Force move over a visible object onto terrain; ensure movement rather than
   implicit attack/repair/enter. Recheck ordinary Move, attack move, force
   attack and queued movement afterward. No mode/modifier must leak.
4. Guard ground; then guard a different own moving unit and verify escort.
   Reject an enemy/self/shrouded target. Guard no-pursuit near enemies and
   compare with normal guard; native unit-specific behavior remains authoritative.
5. Save distant views A and B, pan elsewhere, recall each. Empty C explains
   itself. Table/windows and XR scale stay fixed. Restart/leave match and check
   stale slots are cleared. Recheck modal windows and scripted camera locks.
6. Arm each target command and lose tracking/focus before releasing trigger.
   Restore tracking, release all controls, verify no accidental order. Recheck
   drag selection, construction, upgrades and abilities in each faction.

## 31. P11.1 — arrangement exit, science canvas and campaign presentation (2026-09-14)

### Scope and causes

1. **Arrangement:** `updateXrMenu` and command-console capture could consume
   Back/A before the arrangement exit. Closing the workspace did not itself
   leave arrangement. The UI entry becomes **Done** while arranging;
   it exits directly. Back/A is handled before panel capture, close routes
   cancel grabs/click latches, save placement and require neutral controls.
2. **Science tree:** purchase science is a visible ControlBar child, not a
   modal window or a different command-bar context. Add a read-only actual
   visibility query and include it in the existing full-canvas UI policy.
   Keep compact building controls afterward, stable bottom edge, full-canvas
   ray coordinates and exclusive dialog input. No permanent crop enlargement.
3. **Campaign:** `SinglePlayerLoadScreen::init` (also Challenge) renders an
   entire movie inside one synchronous engine frame, explicitly feeding video
   audio. The user's previous stderr records USA campaign/easy, video buffer
   allocation and ongoing native presentation but no advancing XR frame log.
   Audio continues while the compositor receives no new projection frames.

### Implementation boundaries

- Device W3D display invokes a scoped XR-thread callback after publishing each
  complete loading image. Repeated cinematic camera draw loops also yield.
- The callback ends the already-open outer XR frame exactly once, then performs
  wait/begin, event handling, current eye location, conventional full game-panel
  drawing, passthrough-under-projection composition and end for each yield.
  No recursive `executeSingleFrame`, simulation update, world command or layout
  persistence occurs in the callback. The normal loop skips its stale frame
  after the loader returns and reacquires poses before resuming tabletop.
- A non-renderable frame has no layers; invalid eye poses submit no projection.
  Stopping sessions do not begin another frame. Render failure still attempts
  to end the owned frame, requests movie abort and leaves the outer loop.
  Clear GL caches after XR drawing so the next native draw rebinds its state.
- Hide inert workspace controls during blocking video. Release then press the
  trigger or Back to skip; held launch input cannot skip. Release Back on scope
  exit. Native movie-abort semantics remain authoritative.
- Fullscreen `InGameUI::videoBuffer()` joins Display movies, letterbox, shell
  and loading gates. Cameo/portrait videos retain the normal command-bar role.
  Split eligibility is rechecked after the native frame before presentation.
- Preserve the user's stereo preference and window poses. No v8 migration,
  additional graphics cost in ordinary gameplay, asset changes or game-data
  deletion. QTR-MP in section 30 remains mandatory; human multiplayer/replay
  stereo is not enabled by this work.
- Loading presentation is serviced at native movie/progress draw opportunities,
  not a new asynchronous loader. A long decode/asset operation between those
  opportunities may still hitch. Do not claim loading is uniformly headset
  refresh-rate smooth or that every campaign mission is accepted.

### Validation and artifacts

- Native Android `z_generals` build and both `zh xr` Gradle APKs pass. Quest APK
  v2 signature verifies. `git diff --check` passes.
- **6834 host checks / 22 suites**, with UBSan, pass. Includes 517 loading-frame
  ownership/skip checks, 107 tests compiling the actual XR presenter against
  compositor spies, 442 actual dialog/movie-gate and geometry checks, 91
  interaction-routing and 285 actual menu-action/ray-routing checks.
- **1442 focused ARM64 checks / five suites** pass on Quest serial
  `2G0YC5ZG9609PY`: interaction 91, menu routing 285, loading policy 517,
  production presenter 107, workspace/movie policy 442. These isolated
  executables do not launch or alter the game. Prior P11 Canvas results are
  historical; P11.1's changed Done/title payloads pass host text tests, not a
  newly executed Android Canvas fixture.
- Quest: `build/quest-p111/Generals-Quest-P11.1.apk`, SHA-256
  `1dc6e85e75bd2331dd6f1ddfedfa199c48e113b08f07c5d6d1d43c0c8437e09f`.
- Android sibling: `build/quest-p111/Generals-Android-P11.1.apk`, SHA-256
  `66a093c019d12753a6767618d5c305b5ca61465ac16ff074cdc1854cfcb6518c`.
- P11 rollback: `build/quest-p111/rollback-P11.apk`, SHA-256
  `caeed1aa288572c0b7f51ea46ddfae4906be8a18d28614b602466e018928d642`.
  Preserve the 358-byte actual pre-update layout as `pre-p111-layout.cfg`.
- Installation succeeds, and installed `/data/app/.../base.apk` SHA-256 matches
  the Quest artifact. The manifest-confirmed entry is `XrHelloActivity`.
  An initial attempt naming nonexistent `XrActivity` did not launch anything;
  the corrected ordinary launch reaches Horizon OS's controller-required
  dialog. At the latest check no game PID is running. User has been asked to
  wake both controllers and confirm the OS dialog; no OS gate is bypassed.
- Evidence is retained under `build/quest-p111/` (native/package/host logs,
  original campaign stderr, pre-update layout, test sources and generated
  bridge fixtures). No new game-runtime campaign pass is claimed yet.

### Required worn-headset acceptance — open

1. Arrange table and build panel with sticks/grabs, point at UI/console, exit
   using Done and Back/A. Release sticks, verify pan/rotation/zoom resume and
   no accidental selection/order occurs. Repeat with left-handed roles.
2. In an offline AI Skirmish, open three-star purchase science, inspect every
   corner, buy an available science and close. Verify compact crop returns;
   repeat quit/options/communicator and check no click-through to the world.
3. Start a campaign, let the full movie run while moving the head. Verify
   upright video and responsive room presentation; then automatic tabletop
   gameplay with original camera-script locks respected. Confirm selection,
   objectives and a save/reload. Repeat release-then-skip and focus loss.
4. Repeat in each faction, next-mission transition and Generals Challenge.
   Observe scripted cinematics and fullscreen/cameo videos separately.
   Capture loading XR frame logs; native audio alone is not a successful XR
   presentation. Run the Android sibling separately before claiming Android
   campaign device acceptance.

## 32. P11.1 Locale — system-derived first-run language (2026-09-14)

The user requests: German system -> German; every other system -> English on
the first start. Previously XR defaulted to German regardless of system locale,
while original-game text defaulted to English in Setup.

- Shared Java `InitialLanguage` maps German regional/script tags to XR German
  and everything else, including unknown/empty values, to English.
  `LocaleHelper.systemLanguage()` reads the OS Resources configuration, not
  `Locale.getDefault()` altered by the independent launcher language override.
- Pass the initial language as a JNI argument to the XR entry and call
  `XrLayout::initializeLanguage` only for an unrestored layout. Existing v8
  choices and historical layouts keep their previous language. No new layout
  format and no reset of the user's current German selection.
- Setup first adopts existing native text markers. A new default chooses German
  only when the OS is German and the user's original German CSF/STR is found;
  otherwise it requests English explicitly, avoiding archive-language guessing.
  Retain explicit saved language choices. The default menu entry is clarified
  in DE/EN; other launcher languages get the English fallback for these new
  labels and are otherwise unchanged.
- Direct XR entry also seeds an absent initial native marker, atomically, when
  there is no saved text preference. This covers old direct-to-GameData paths
  that never opened Setup. The existing marker is never overwritten by seeding.
- Original text still requires the corresponding user-supplied language data.
  Mods may replace tables filed under the English token. This is not an audio,
  localized movie or SKU-artwork switch; do not promise translated assets.
- 6842 UBSan C++ checks / 22 suites and **51 Java policy checks** pass for this
  final source: **6893 host checks / 23 suites**. Java tests cover de-DE, de-AT,
  de-CH, uppercase, underscore/script tags, non-German/invalid/null tags, and
  missing German text. Workspace tests now total 450, including preservation
  of both saved choices against either system default. Native plus both APK
  builds pass. Earlier P11.1 focused ARM64 binaries (1442 checks) still pass;
  they are not a hardware test of fresh-install locale seeding.
- Quest artifact: `build/quest-p111/Generals-Quest-P11.1-Locale.apk`, SHA-256
  `c2a27888c8c6c88854392ecab71b4e43dcdd2bdbb144d983eba95add9f818667`.
- Android sibling: `build/quest-p111/Generals-Android-P11.1-Locale.apk`, SHA-256
  `7ae7f2223aacb77b96a0c01290180e056bc6751e6b438e243ba672cf237078d9`.
- Quest v2 signature verifies, install succeeds, and installed base APK hash
  matches. Device locale reads `de-DE`. `cmp` confirms the actual pre-update
  and post-update layout files are byte-identical. No running game was
  interrupted; normal launch still awaits Horizon OS's controller-required
  dialog at the final inspection. Do not clear app data to test first start.
- Evidence: `host-locale-final.log`, `native-locale.log`, `package-locale.log`,
  `installed-locale-hash.log`, `startup-locale.log`, `device-p111.log` and layout
  backups under `build/quest-p111/`. A real clean-profile first-start test with
  DE and non-DE systems, plus all section 31 physical gates, remains open.

## 33. P12: campaign shadow comparison and CPU/GPU attribution (2026-09-14)

### Request and evidence

The user confirms campaigns work after P11.1, then reports poorer performance
and appearance than offline Skirmish. This closes the reported basic campaign
launch blocker, not full campaign completion, performance or cinematic acceptance.
They authorize the proposed dedicated measurement and shadow A/B step.

Read-only P11.1 Locale recordings (USA campaign Easy / earlier Skirmish) show:

| Renderer diagnostic | Campaign | Skirmish |
| --- | ---: | ---: |
| Sampled present-counter median | 16.7 fps, 88 samples | 56.7 fps, 175 samples |
| Mean source model draws/frame | 298.4 | 172.4 |
| Mean source terrain draws/frame | 115.0 | 126.0 |
| Mean source shadow draws/frame | 451.7 | 211.9 |

Different scenes and user-adjusted views; not a controlled A/B or measured
headset/compositor FPS. Both initially use 1920x2011 eyes. The later campaign
switch to 1536x1609 does not establish a resolution-only cause. Steady play
does not repeatedly invoke the new loading presenter, and UI-submit is roughly
0.4-0.57 ms. Both recordings have capped OpenAL exceptions, so audio is not
proven to be a campaign-specific bottleneck. Source evidence points first to
XR's forced volume shadows and original-plus-two-eye submission with frequent
FBO switches, not new game-logic work to disable speculatively.

### Implemented, reversible first optimization

Under **UI -> View** there are two new direct controls:

- **Shadows A: Original**: current P11.1 rendering. Clicking toggles
  **Shadows B: Light**, and vice versa.
  B disables volumetric shadows in the XR frame, keeping existing decal shadows,
  textures, resolution, board coverage, native world and gameplay unchanged.
  Volume-only templates lose their shadow in B; no replacement contact shadows
  or improved visual fidelity are claimed. The production manager reads this
  flag at its volume-render gate. Keep resources resident for instant A/B.
- **Timing: ON/OFF**: opt-in CPU/GPU measurement; current
  engine CPU/GPU averages appear in the View panel. Approximately every two
  seconds a `[xr] P12 perf` line records detailed values in the existing log.

Original stays the default pending visual/performance acceptance. Both new
choices are **session-only**; restarting restores Original and measurement OFF.
No layout migration or persistence changes, no hidden lowering of eye quality,
no game-data edits, no campaign script/AI/network-message changes. Original
shadow flags are restored after the frame, including exceptions. The ordinary
Android game, normal panel fallback and existing offline multiplayer gate stay.
Native adaptive LOD is unchanged: live A/B can also affect its subsequent detail
decisions, so use A/B/A and record the view/scene instead of promising identical
simulation snapshots. Do not disable native simulation or LOD for this test.

### Timing contract and scope

- `XrPerformance.h`: session state, 30 eligible-frame warm-up after changes,
  distinct settings epochs, no mixing of GPU results from old profiles.
- `XrGpuTimer.h`: actual production eight-query ring, host-testable API dispatch.
  Results are read only after availability; a full ring drops a measurement
  rather than waiting. Query-counter support is checked. Missing support means
  unavailable, not zero. Disjoint before/during readout discards affected
  measurements; zero or >=1s GPU values are rejected. No steady readback,
  `glFinish`, deferred geometry snapshots or extra scene/simulation traversal.
- `XrGpuTimerGL.h`: optional extension discovery and context-local proc lookup,
  independent of d3d8gles interposed gl wrappers. Verified against the primary
  [Khronos EXT_disjoint_timer_query specification](https://registry.khronos.org/OpenGL/extensions/EXT/EXT_disjoint_timer_query.txt).
- Engine GPU query spans `XrGameBoot_Frame`, not the compositor. CPU values are
  **wall elapsed**, including driver waits, not CPU core utilization. GL elapsed
  can include submission gaps; it is not GPU busy percentage. CPU/GPU overlap:
  never add them as a frame budget. `eyeCPU` measures host eye composition,
  `xrWait` is runtime wait, `frame` includes those phases and `xrEndFrame`.
- Only focused, interactive stereo play without arrangement, native full-panel
  dialogs or locked cinematic camera is sampled. Loading callbacks end/discard
  active queries before presenting nested XR frames. Mode/resolution/coverage/
  board-size changes clear measurements. Camera movement is not auto-frozen or
  rejected; hold the same view manually for comparisons. Native periodic map
  mapping logs remain available alongside the new settings key.
- Per-window log fields: `scene`, `shadows`, `eye`, `coverage`, `board`, `n`,
  `engineCPU`, `eyeCPU`, `xrWait`, `frame`, `maxFrame`, `GPUengine`, `gpuN`,
  `gpuSupported`, cumulative `disjoint`/`dropped`. `GPUengine=-1` means no valid
  GPU samples. CPU/GPU counts differ because queries arrive asynchronously.

### Verification and artifacts

- Native ARM64 `z_generals` and both `zh`/`xr` packages build successfully.
- 7501 UBSan C++ checks across 24 suites plus 51 Java language cases = **7552**
  host checks. Included: actual menu routes/payloads, original frame shadow
  scope (449), optional timer ring/warm-up policy (161), campaign presenter,
  science/quit expansion, locale precedence and all earlier tactical fixtures.
- Quest ARM64 timer policy: 161 checks. Real Adreno 740 GLES query fixture:
  91 checks, 79 valid elapsed samples, mean 0.0021 ms for its trivial clears.
  That number validates query plumbing only, **not game performance**.
  No new real Canvas rendering or worn-headset acceptance claimed in P12.
- Quest artifact: `build/quest-p12/Generals-Quest-P12-Performance.apk`, SHA-256
  `8a4659d2938ad3189ab667fec6884e3d62b64014c61b6f0f6b5311aa45868d37`.
- Android sibling: `build/quest-p12/Generals-Android-P12-Performance.apk`, SHA-256
  `810dd99aaacae3248b1ba9e9e8b93cf22fe001be05a84a96e638258a475d2326`.
- Quest v2 signing verifies. Installed by `adb install -r` with no running game;
  installed base.apk SHA-256 matches. The actual 360-byte v8 layout was backed
  up and remains byte-identical. P11.1 Locale rollback APK preserved with hash
  `c2a27888c8c6c88854392ecab71b4e43dcdd2bdbb144d983eba95add9f818667`.
- Build/test logs and layout/rollback files are in `build/quest-p12/`.
  Normal start waits at Horizon's controller-required dialog; no P12 game PID
  at final installation inspection. Wake Touch controllers normally; never
  bypass the OS launch check. No game data, saves or user settings cleared.

### Required in-headset A/B/A and next decision

1. Start/load the same USA campaign scene, leave resolution/table coverage
   fixed, wait until the introductory video/scripted camera is finished.
2. UI -> View: turn Timing ON, leave Original A. Stay in the same location and
   look at the same board for about 20 seconds after warm-up. Keep workspace
   visibility consistent across runs. Do not issue new unit orders during it.
3. Toggle Light B for 20 seconds, then Original A for 20 seconds again.
   Note smoothness and missing/acceptable shadows; capture the existing XR log.
4. Repeat on one representative offline Skirmish, then verify a campaign
   video -> tabletop transition and return to normal display with A/B.
5. If B materially improves CPU/GL elapsed and frame cadence, design a bounded
   cheaper shadow replacement/budget that retains miniature contact cues; only
   then choose a new default with the user. If cost remains, investigate world
   pass/FBO scheduling using the new timing baseline. Original + two-eye draws
   cannot be deferred blindly: dynamic D3D buffers and state mutate between
   draws. Preserve the established dynamic-buffer and MRT/fallback contracts.

This step delivers the selectable optimization and measurement infrastructure.
The campaign is **not declared performance-fixed**, and no FPS uplift, new
default, multiplayer certification or full renderer refactor is claimed.

## 34. P12.1: recover campaign view selection (2026-09-14)

### Evidence and bounded correction

The user reports the campaign remains flat, cannot switch to Tabletop, and
looks low resolution. Fresh P12 log: USA Easy selected at 7574/7578; interactive
mode at 7619; loading returns at 7666; favorite camera applies at 7669. No
world/UI transition occurs for the campaign. Subsequent Skirmish switches at
9843 and uses 1536x1609 eyes. These logs do not record the individual blocking
flags, so they cannot establish the sole cause of the persistent campaign block.

Two concrete source defects are corrected:

1. Workspace and LT+Y require `splitVisible` to request stereo, and workspace
   does not clear `uprightGame`, which itself disables split generation. This
   circular dependency traps manual flat view. New `XrViewMode.h` separates
   intent from readiness, releases the upright override and prevents a later
   startup default from overwriting explicit choices.
2. Native `View::userSetAngle/userSetPitch/userZoom` calls
   `stopDoingScriptedCamera` after the short native control lock expires.
   The old automatic preset ignores cinematic state and runs before any split
   capture. New read-only predicates guard XR camera methods against load,
   intro, movie, letterbox and active scripted camera paths. The automatic
   favorite additionally waits for completed world/UI capture. Pending camera
   preferences remain pending until safe; no campaign script is edited/skipped.

Requested Tabletop can now be queued while a cinematic is still using the
ordinary panel. Actual stereo waits for safe gameplay, honoring the existing
offline gate. Video/load presentation and expanded science/quit UI remain.
View shows the actual flat/stereo extent and reason (movie, camera sequence,
loading, manual window, shell or capture pending); P12.1 logs reason, requested
mode, upright override, split readiness and quality every 120 host frames.

### Clarity and preserved choices

Saved quality is **Balanced**, and the unsplit fallback is **1280x720**. High
already provides **1920x2011 per eye** on this Quest versus Balanced 1536x1609.
The corrective build does not silently reset that preference, resize the
native GUI, invent high-resolution assets or increase the current GPU budget.
Choose High under View after stereo is restored and verify the displayed
extent. Shadow A/B and the P12 timing controls remain available.

### Acceptance

Start a **new campaign mission** and let its intro finish, avoiding a save made
after a potentially cancelled cinematic. Expect video panel -> Tabletop game.
Test UI -> View -> Now Tabletop from manually upright gameplay, and LT+Y.
If it remains flat, report the reason now shown on View; the log records it.
Source/fixture success is not confirmation of real campaign recovery, clarity
or improved frame rate. Those worn-headset gates remain open until tested.

### Build, deployment and retained evidence

- Host: 7,627 checks across 24 C++ suites and the Java locale suite, all pass.
- Quest ARM64: production menu routing 295 and workspace/cinematic guards 487,
  all 782 checks pass. These are fixtures, not a live campaign playthrough.
- Native `z_generals` and Android `zh`/`xr` debug packaging succeed. XR APK
  signature verifies; `adb install -r` succeeds on Quest `2G0YC5ZG9609PY`.
- Installed APK SHA-256 matches the local XR artifact. The saved 360-byte layout
  is byte-identical before/after installation. Normal launch reaches Horizon's
  Controller Required dialog, with no game PID; no bypass was attempted.
- Artifacts and logs: `build/quest-p121/`. P12 rollback remains at
  `build/quest-p12/Generals-Quest-P12-Performance.apk`.

SHA-256:

```text
e558263fec8134b5fd5b00f0348b778c2826fabbd42fe4dc42c405981cc5d08f  Generals-Quest-P12.1-Recovery.apk
714d9e34d20f99429c6dc7a0cd5bb1353cecdab9ca0677a0f6cf86204f1aaccc  Generals-Android-P12.1-Recovery.apk
```

## 35. Performance-first direction after user testing (2026-09-14)

The user has already compared Light shadows with both eye-quality choices:
approximately 36 FPS at High, approaching 50 FPS at Balanced. These are
user-observed counters, not newly captured compositor measurements or a
controlled benchmark. Do not ask for the same comparison again or treat the
average as proof that frame pacing and demanding battles are accepted.

Use **Light shadows + Balanced resolution as the working performance baseline**.
This is a planning decision, not a shipped default/persistence change: the
current shadow switch still resets to Original on restart.

The user explicitly declines replacement/contact shadows for now. Defer them
to a possible later visual-quality phase; they are not part of the next
implementation. Retain existing inexpensive decal shadows, but do not add
substitutes for objects whose volume shadows are disabled.

Next priority: consistent frame times and responsive selection, orders and
map navigation in both campaign and Skirmish, including demanding combat and
sustained sessions. Profile remaining CPU/GPU cost at this fixed baseline
before choosing a bounded renderer optimization. Preserve original missions,
simulation timing, unit counts, AI and multiplayer determinism. Do not trade
gameplay correctness for a higher displayed FPS, promise a specific uplift,
or raise resolution/add effects ahead of fluid playability.

## 36. P12.2: narrow stereo state restoration (2026-09-14)

### Evidence and bounded change

The current P12.1 Quest log confirms stereo gameplay and both quality extents.
Its later stereo windows report about 46-48% fixed-state cache hits; UI submit
is generally below 1 ms and shader compilation is absent in those windows.
There are no opt-in `[xr] P12 perf` samples, so shadow profile and CPU/GPU
attribution are not established for those windows. The user's Light/Balanced
comparison remains the performance baseline, not a new controlled capture.

Source inspection identifies guaranteed redundant work: each eligible source
draw executes its ordinary submission and both eye submissions, then invalidates
and reapplies the complete fixed state. The eye loop changes only stencil
enable, color mask, blend factors, viewport and depth range (plus its shader
uniforms and target bindings, already restored separately).

P12.2 restores precisely that changed subset via `XRStereoRestore.h`, keeping
the existing fixed-state key valid. Unchanged depth function/write/enable,
cull, polygon bias and stencil parameters are no longer reissued at this
boundary. Equal-sized eye viewports and depth ranges are set once before the
eye loop rather than once per eye. Original submission, both eyes, dynamic
buffer consumption order, offscreen passes, geometry, resolution, native UI,
simulation, scripts and selected shadow profile are unchanged. No replacement
shadows, deferred draw queues, readbacks, GL waits or new GL entry points.

The ordinary Android path uses no narrow restore when stereo is inactive.
Clear/target/context cache invalidation stays unchanged. Any future eye-loop
state change must extend this restore contract and the regression test.

Bounded `[d3d8gles] P12.2 narrow-state` counters record restore count and its
3-5 GL calls per eligible source draw. Do not interpret a higher cache-hit
percentage as a frame-rate gain: removing a cache check also changes its
denominator. The installed View status and startup stamp explicitly say P12.2.

### Validation

- Native `z_generals` build and both Android `zh`/`xr` packages succeed.
- 7,627 checks across the existing 24 C++ host suites and Java locale suite pass.
- Quest Adreno 740: 10,250 new production state/draw checks pass across 1,024
  scenarios. The script extracts actual `drawCommon`, `applyFixedState`, state
  key and converters; its reference substitutes the previous full restoration.
  Both eye targets, ordinary target and following ordinary draw are pixel-identical;
  queried GL state matches. Eligible dispatch count is identical, with 612 full
  state applications avoided. Cases cover masks (including zero), blending,
  stencil, depth, bias, partial viewport/depth range, indexed/non-indexed draws,
  world/shadow/UI categories and disabled-stereo/offscreen paths. Shader/resource
  setup is a fixture, not the full game's scene or a speed benchmark.
- Existing real GLES production shader/coverage/depth/eye-allocation suite:
  73 checks pass. These do not replace actual campaign visual acceptance.
- XR signature v2 verifies, `adb install -r` succeeds on Quest `2G0YC5ZG9609PY`.
  The 360-byte user layout is byte-identical before/after installation.
  Installed APK SHA-256 matches the artifact. Normal startup is intercepted by
  Horizon's Controller Required dialog; no game PID is present, no bypass used.
- Evidence and APKs are retained under `build/quest-p122/`. P12.1 rollback is
  `build/quest-p121/Generals-Quest-P12.1-Recovery.apk`.

```text
e46290731c357750ee8b769384c48970ae8c767680e8dbe0069b637b2e4ebf53  Generals-Quest-P12.2-State.apk
336857afec1b0f45c33ee1196610d3d5d668196762207a4790b4e28aed0c0670  Generals-Android-P12.2-State.apk
```

### Next worn-headset gate

Keep Light shadows + Balanced, same mission/location and comparable coverage.
No need to repeat the user's High-versus-Balanced test. Enable View -> Timing
for a short quiet comparison, then test camera movement, unit orders and combat;
repeat in Skirmish and over a sustained session. Check native menus, cinematic
transitions, translucent effects and original-shadow fallback. A live FPS gain
or resolved frame pacing is not claimed from eliminated calls or fixture success.
Shadow choice is still session-only; select Light after restart.

## 37. P13: fix audio stalls and inactive timing; gate the stereo atlas

### Reassessment after the P12.2 headset test

P12.2 is **not performance-accepted**. The user reports substantially worse
Light/Balanced play and only a small possible improvement with Timing OFF.
The latest retained log includes 13–28 renderer FPS in that profile and repeated
`alDeleteBuffers` exception backtraces during audio cache eviction, reached via
`startNextLoop`/`notifyOfAudioCompletion` inside `GameEngine::update`. The logger
caps stack traces at 64; reaching that cap does not mean the error has recovered.
P12.1 also contains these errors, so they are not attributed to P12.2's restore
change. Different camera coverage and mission states prevent a controlled
before/after FPS claim. CPU intervals include waiting and are not utilization.

### Implemented without further quality reductions

1. **Audio cache lifetime:** successful fresh loads now acquire one lease,
   matching cache hits. Closing an already-zero count cannot underflow. Before
   loading the next loop/attack/sound/decay portion, stop and detach AL_BUFFER,
   clear the playing handle, then return its lease. A stopped source still owns
   its AL buffer, which made immediate eviction invalid. Stream playback and
   cache size/accounting policy are unchanged. Shared sound-device fix applies
   to both Android flavors; other platforms have not been device-certified here.
2. **Timing really OFF:** no GPU status/result polling when disabled; an empty
   ring also avoids the two disjoint queries. In-flight query objects stay owned
   until polling resumes or context teardown; old epochs cannot contaminate new
   measurements. No glFinish, blocking readback or per-frame allocation added.
3. **Optional same-resolution stereo atlas:** View -> Stereo: Compact/Reference
   switches session-only. Reference remains the default. Compact uses one
   side-by-side color/depth-stencil target, one eye-target binding per source
   draw, and isolated viewports/scissors. Both eye draws still happen immediately
   after the ordinary draw, preserving mutable-buffer ordering. No single-pass
   geometry/multiview claim. Balanced remains 1536x1609 per eye on this Quest;
   physical atlas is 3072x1609, the same total pixels as two separate targets.
   High, original/light shadows, board feedback, alpha coverage probes and
   compositing are supported. Texel-center clamping prevents cross-eye filtering.
   Unsupported/incomplete atlas allocation falls back to separate targets;
   actual View status reports P13 Atlas or Ref. A fallback is not reallocated
   each frame solely because the Compact preference is still requested.

No default resolution/shadow reduction, replacement shadows, simulation, input,
mission scripts, game assets, saves, layout migrations or network changes.
Audio correctness and disabled-timing fixes apply in both stereo modes.

### Validation and the benchmark decision

- Native `z_generals` and both `zh`/`xr` APKs build successfully.
- 8,360 host checks pass: prior XR suites, extended timer/menu/localization
  coverage and 614 production audio cache/lease tests (307 fixture decodes).
- Quest real OpenAL Soft 1.24.3: 1,802 silent loopback checks. Reproduce
  AL_INVALID_OPERATION deleting a stopped-but-bound buffer, then verify the
  actual production helper releases it safely over 300 cycles.
- Quest Adreno 740: 16,395 production draw/state checks over 1,024 scenarios;
  both separate eye images equal atlas subregions, ordinary and following
  draws match, GL state and dispatch match. Including actual XR compositing
  shaders gives 24,591 checks, including deliberately out-of-region UVs at
  the eye seam. Existing world/shadow/depth/allocation suite passes 73 checks.
- A small 600-source-draw target-switch benchmark at Balanced has order/clock
  variability. The first run averaged roughly 5% less elapsed time for Compact;
  a repeat was effectively tied. This is **not a reliable speedup**, and not
  campaign FPS. Thus Compact stays opt-in until measured in the same actual
  mission. Avoid replacing a user-accepted default based on eliminated calls.

### Acceptance still required

Start with Light + Balanced, Timing OFF and default Reference, same campaign
location. Check responsiveness, pans/orders/combat, no audio errors or dropouts,
cinematic-to-tabletop transitions and native menus. Then optionally compare
Compact at unchanged coverage; allow allocation/visibility probe warm-up first.
Only short timed runs after that establish engine/GPU/frame-time differences;
keep measurement-off subjective acceptance separate. No sustained FPS target
has yet been met or certified by this candidate. If performance remains poor,
profile the residual engine/audio/driver work before another visible quality cut.

Retain the P12.1 APK as rollback. Multiplayer coverage remains a later separate
gate; these presentation/audio changes do not alter simulation or wire formats.

### Deployment evidence

Both APK signatures verify (v2). `adb install -r` succeeded on Quest
`2G0YC5ZG9609PY`; the installed base APK hash matches the XR artifact. The
373-byte current user layout is byte-identical before/after. Normal launch via
`com.generalsx.zerohour.xr/com.generalsx.zerohour.XrHelloActivity` is currently
blocked by Horizon's Controller Required dialog; no bypass was used and no
new campaign runtime sample is available yet. Artifacts/logs are under
`build/quest-p13/`. Device regression tests are not worn-headset acceptance.

```text
f4c0acb30616ca883eb5039d5a30f20fa21f5e783bd03c26c4f9fbdd30ef7354  Generals-Quest-P13-Performance.apk
bbf9d00048eeaf4f3b0285673cd2e3a0a6c29128863142f88c1b1632181bcd31  Generals-Android-P13-Performance.apk
```

## 38. P14: omit the unused planar world copy after stereo validation

### User acceptance and scope

The user reports P13 is substantially better and asks for improvements without
further graphical sacrifices. P13 is the working rollback baseline, not proof
of a sustained target FPS. A fresh read-only log shows no prior OpenAL
buffer-deletion errors in that sample and several user-driven stereo/quality
switches, ending at High/Compact. Do not attribute changes in that recording
to a single optimization or use it as a controlled A/B benchmark.

The user authorizes the proposed copy-elision step, followed by auditing the
additional ordinary world draw. No new shadow, texture, resolution, simulation,
input, audio, multiplayer or game-asset changes are included in P14.

### Implementation and fallback contract

- `beginXRUI(elideWorldCopy)` only omits `glBlitFramebuffer` when
  `m_xrStereoReady` is already true **in the current native frame**. That flag
  comes from the completed stereo draw/coverage gate, after both eyes have
  been validated. `beginXRFrame` resets readiness: a previous good frame alone
  never authorizes skipping the new copy.
- Retain world/UI allocation and ordinary world draws. Retain the full composed
  color attachment and transparent UI MRT attachment, including normal blending.
  Only one 1280x720 RGBA8 world image copy plus its two framebuffer bindings are
  omitted. This removes a 3.52 MiB image-copy payload per eligible frame; actual
  memory-bus traffic depends on the driver and is not inferred from that size.
- Record planar snapshot validity separately. Its texture getter returns zero
  after an omitted copy, so no consumer can mistake stale contents for a fresh
  image. If stereo becomes unavailable later in the frame, the host falls back
  to the current composed game image. Next non-stereo frame copies normally and
  can restore the detached planar view. Never replay engine/simulation work.
- Flat/native Android and unready/empty-eye cases keep the old copying path.
  Split-ineligible movies and opaque shell screens keep full presentation.
  Dialogs that retain stereo continue to receive the complete detached UI.
- View -> **World copy: Auto / Always** is an
  independent, session-only comparison. Auto is the candidate default; Always
  restores P13 copying. Neither changes saved layout or quality. Existing
  Compact/Reference and Original/Light choices remain independent and unchanged.
  The measurement key now includes copy mode; disabled timing stays inactive.
- `[d3d8gles] P14 planar snapshot copied=... skipped=...` reports actual outcomes
  every 120 successful UI boundaries. Presentation/startup identify P14. Mode
  preference alone is not claimed as evidence that the blit was eliminated.

### Validation

- Native `z_generals`, both `zh`/`xr` APKs and v2 signature verification pass.
- 8,437 host checks pass: 8,360 P13 baseline plus 66 capture/fallback cases,
  3 comparison-toggle cases and 8 catalog checks. Includes audio and disabled
  timing regressions, gameplay input, dialogs, loading and language policies.
- Quest Adreno 740: **3,285** new production UI-boundary/frame/getter checks.
  Exercise 128 scenarios in both modes with different world colors and partial
  translucent UI. Composed and isolated UI pixels match; copied worlds are
  fresh, skipped images are unavailable, late stereo loss selects a valid
  presentation, and subsequent planar frames recover. Ineligible/duplicate
  calls do not blit. Benchmarks assert zero versus 120 blits for 120 frames.
- Existing Quest stereo tests pass: 16,395 draw/state checks, 24,591 cumulative
  including actual compositing shaders, plus 12 benchmark checks. Existing
  world/shadow/depth/allocation test passes 73 checks.
- Isolated boundary benchmark at 1280x720 (ms/frame, paired order alternated):

  | Round | Always | Auto |
  |---|---:|---:|
  | 0 | 0.265 | 0.182 |
  | 1 | 0.214 | 0.171 |
  | 2 | 0.205 | 0.129 |
  | 3 | 0.178 | 0.129 |

  About 0.04–0.08 ms/frame less work in this small fixture; **not** a campaign
  frame-time saving or percentage FPS gain. The optimization is intentionally
  bounded and should not be described as the major remaining performance win.

### Audit of the additional ordinary world draw

`drawCommon` still submits the ordinary draw and then each eligible eye. P14
deliberately preserves that draw: UI MRT composes over it and a movie/dialog or
stereo failure discovered later in the native frame can require its completed
image. Suppressing all ordinary draws now would invalidate the safety guarantee
just tested. Render-to-texture passes are also not interchangeable with these
backbuffer draws and must remain outside any future suppression rule.

A later candidate must establish a same-frame recovery strategy and classify
dependencies before suppressing eligible backbuffer world draws. Do not call
`drawViews`/scene callbacks a second time to repair a failed frame. True per-eye
multiview is another candidate to reduce submission overhead while preserving
the ordinary fallback, but needs its own shader/depth/stencil and timer support
validation. These larger renderer changes are **not** implemented in P14.

### Worn-headset gate

Keep one campaign/location, same shadow profile, resolution, board coverage
and stereo mode. Test Auto versus Always without simultaneously changing
Compact/Reference. Verify unit orders, drag selection, hover/build panel,
science tree, game menu/Communicator, normal-view switching and campaign
video-to-tabletop transitions. Include startup and sustained combat. Begin with
Timing OFF; short warmed-up measurements can follow. Physical acceptance and
a measured game FPS benefit remain open, regardless of passing fixtures.

Preserve `build/quest-p13/Generals-Quest-P13-Performance.apk` for rollback;
P14 logs, APKs and layout evidence are retained under `build/quest-p14/`.

### Deployment

`adb install -r` succeeds on Quest `2G0YC5ZG9609PY`. Installed APK hash matches
the XR artifact; both APK signatures verify. The current 358-byte layout is
byte-identical before/after. Normal launch reaches Horizon's Controller Required
dialog, with no game PID. No bypass used; fresh in-game P14 counter capture and
physical acceptance require active controllers.

```text
52e3b2a2f18913ab3ede9dc3ff3547111168a2fda977dfb1e7591675fc264e54  Generals-Quest-P14-WorldCopy.apk
d58fc53acfe5fb464563891f8744b0b9ef4189bf75eda60a613453f8b67317c2  Generals-Android-P14-WorldCopy.apk
```

## 39. P15: agreed defaults, tabletop-only view controls, optional true multiview (2026-09-14)

**User request:** implement these three changes together. P14 user feedback is
positive, but is not a controlled frame-time measurement. Keep its APKs as
rollback. This section supersedes earlier manual normal-view acceptance steps.

### Defaults and view ownership

- Fresh XR layout v9: **Balanced** resolution. A valid v1-v8 layout migrates
  once to Balanced and tabletop startup. The existing pre-v7 photo migration
  remains as before; v7/v8 to v9 preserves every pose, size, snap preference,
  zoom, language, handedness and command-window visibility. A later deliberate
  High choice in v9 survives restart; quality is not reset each launch.
- Session defaults: **Light** shadows (original projected shadows retained,
  volume shadows disabled), **World copy Auto**, **Reference** stereo,
  measurements off. No substitute shadows and no lower eye resolution introduced
  for multiview. Existing manual quality/shadow/copy experiments remain available.
- Supported offline gameplay (Skirmish and campaign) requests tabletop
  automatically, including return from a cinematic. Remove the four legacy
  view/startup actions and both controller ways of selecting planar gameplay.
  Window menu's former tabletop toggle now opens Graphics / View.
- Menus, videos, loading and technical capture fallback retain their normal
  upright full-image presentation. Explicit window/table placement, tilt, scale,
  arrangement exit, camera control and handedness are not removed.
  Multiplayer eligibility is still the previously documented separate gate.

### Renderer implementation and comparison controls

UI -> View -> **Stereo** cycles **Reference -> Compact -> Multiview (Test) ->
Reference**, independently from quality, shadows and copy policy. It resets
measurement epochs and is session-only. Actual presentation status reports P15
Multiview/Atlas/Ref, not merely the requested setting.

Multiview uses the XR host's current system-EGL resolver, requires the exact
GL_OVR_multiview2 extension and at least two views, and allocates a two-layer
RGBA8 color texture plus a two-layer DEPTH24_STENCIL8 texture. Both attachments
have the same view count. A separate generated shader variant uses
layout(num_views=2) and gl_ViewID_OVR to choose the eye matrix. Every eligible
source world draw immediately submits both eye layers with one draw call;
there is no deferred mutable-buffer replay or second scene traversal.
The ordinary composed world draw remains available for same-frame fallback.

Single-layer FBO views share those array images for coverage probes,
decorations and per-draw reference recovery. The compositor samples the correct
sampler2DArray layer directly, without an intermediate full-eye copy. Its 2D
sampler and array sampler use different texture units, including ordinary UI
draws, and preserve alpha coverage and display-to-linear conversion.

The cache distinguishes ordinary and multiview programs explicitly and has a
bounded 512-entry budget. A failed multiview shader uses ordinary per-eye
drawing into the array for the remaining current frame, latches failure, and
returns to ordinary targets on the next frame. Unsupported extension or
incomplete allocation falls back to the selected reference/atlas path without
retrying allocation every frame. Actual texture type follows allocation, not
the requested mode. The existing two-eye coverage gate remains authoritative.

GPU elapsed/timestamp queries are **disabled while multiview is requested**:
the [OVR specification](https://registry.khronos.org/OpenGL/extensions/OVR/OVR_multiview.txt)
defines their values as undefined if a multiview draw framebuffer occurs during
the query. CPU timing and frame/FPS measurements remain available, with a
localized explanation for unavailable GPU time. See also
[OVR_multiview2](https://registry.khronos.org/OpenGL/extensions/OVR/OVR_multiview2.txt).
No GPU timing claim is made for this path.

### Validation and reproducibility

- Native z_generals build, both zh/xr APK builds, both v2 signatures: pass.
- **8,492 host checks** pass, including v8 custom-layout migration,
  deliberate v9 High persistence, removed controller toggles, cinematic
  tabletop restoration, three-way renderer routing, language, audio,
  disabled timing, dialogs and loading ownership.
- Quest 3, Adreno 740: exact OVR_multiview2 capability confirmed.
- Extended production draw/fixed-state suite: **28,689** checks over 1,024
  scenarios, **45,077 cumulative** with real compositor tests; the existing
  bounded benchmark adds 12 checks. Reference, narrow restore, atlas, multiview
  and injected multiview-shader failure produce identical ordinary/eye pixels
  and restored GL state. Test shader/resource fixtures are not game assets or
  proof that every generated game-material variant has been exercised.
- New **427** production allocator/lifetime/getter and shared-shader checks:
  repeated Balanced/High allocations, reuse, capability/failure fallback,
  both transparent layers, projection, clipping, particles, cutout, coverage
  and teardown. Production code is extracted by
  scripts/qa/xr-multiview-device-test.sh; run its NDK-built binary on an EGL
  device. Shader-contract geometry is synthetic, not headset game acceptance.
- Existing copy/MRT/fallback suite: **3,285** checks; existing world/shadow/
  allocation suite: **73** checks. Both pass.
- Existing microbenchmarks do not measure true-multiview game performance.
  No campaign FPS improvement is claimed. The multiview companion-program
  switch currently reapplies uniform blocks; profiling and per-program uniform
  caching are a likely follow-up if this limits the saved draw submissions.
  Keep Reference default until an actual in-game A/B establishes a benefit.

### Deployment and remaining headset gate

Artifacts and raw logs are in build/quest-p15/. Installed with adb install -r
on Quest 2G0YC5ZG9609PY; installed base.apk SHA-256 matches the XR artifact.
The 358-byte layout is identical immediately before/after installation.
Migration runs on the next successful game startup and is covered by host tests.
Normal launch currently reaches Horizon's Controller Required dialog, no game
PID; no bypass. In-game migration and physical playability are not yet verified.

```text
aab623a4aea571ca7727c0b64678f2406d74ee85df5db5d230381088a235d6d7  Generals-Quest-P15-Multiview.apk
2277a8b5422249aa0f22651dba9847d805b486105f14e8ac10c07027535553c4  Generals-Android-P15-Multiview.apk
```

Next test: use both active controllers, launch normally, check Balanced/Light/
Auto and preserved placement. In the same campaign location with identical
coverage/camera, compare Reference versus Multiview, first Timing OFF, then a
short warmed-up FPS/CPU measurement. Include sustained combat, drag selection,
orders, build hover, powers tree, game menu and Communicator. Verify both eye
images and edges, shadows/effects and video-to-tabletop return. First-seen
material variants can compile during a session; do not count a shader-warmup
hitch as steady-state FPS. High remains optional for a later quality comparison.

## 40. P16: preferred multiview and program-resident uniform keys (2026-09-14)

### Decision and implementation

The user's worn-headset feedback for P15: Multiview looks equivalent, has no
obvious speed regression, and is reasonably playable. Subsequent read-only
logs confirmed actual Multiview rather than just the requested setting.
There is still no controlled in-game A/B establishing an FPS gain. At the
user's request, Multiview now becomes preferred; this supersedes section 39's
Reference-default decision without removing its fallback or comparison path.

- Performance and world-frame defaults request Multiview. The selector still
  cycles Reference, Compact, Multiview; capability, allocation and shader
  failures retain existing same-frame/per-eye and next-frame fallback.
  Presentation status reports P16 and the actual renderer. Source-draw logs
  no longer incorrectly label Multiview as two eye draws.
- No new persisted-layout version or migration. Balanced on fresh starts,
  Light shadows and Auto world copy remain unchanged. A deliberate v9 High
  setting, language, handedness and user window poses remain intact.
- Each XR ProgramInfo owns independent texture-matrix, miscellaneous,
  material and lighting keys. Switching between ordinary and Multiview
  companion programs no longer discards resident values. Keys have the same
  equality/value semantics as before; generated shader math is unchanged.
- External GL ownership handoffs lazily invalidate every program's keys via
  an epoch. Program creation starts with empty keys, owned by ProgramInfo
  inside the existing bounded program cache. No per-draw allocations or GL
  state queries are added.
- Object/world matrices still upload each draw. Shared view/projection UBO
  caching is unchanged. Absent material uniforms do not suppress lighting.
  Non-XR keeps the old global cache policy. Dynamic vertex/index submission,
  D3D zero values, stencil conversions, coverage and alpha remain unchanged.
- GPU elapsed queries stay disabled for requested Multiview. CPU/FPS timing
  remains available; no new continuous profiler overhead is added.

### Validation

Native z_generals and both zh/xr APK packages build successfully. Host suite:
**8,492 checks**, including updated default/three-way routing expectations.
Real Quest 3 / Adreno 740 EGL tests:

- **8,667** new checks from extracted production uniform keys, ProgramInfo,
  applyUniforms and invalidateCachedGLState. Legacy and XR policies produce
  identical reflected GPU values and ordinary/both-eye pixels across
  changing object/camera/texture matrices, fog, zero states, materials and
  lights. Includes actual Multiview FBOs, optimized-out material uniforms
  with active lighting, and recovery from explicit foreign uniform changes.
- The dynamic fixture records **24,282 -> 11,598 glUniform calls**, 52.2%
  fewer. A four-round alternating-policy benchmark with static materials
  records 18.3-18.7 ms cached versus 21.4-22.8 ms legacy for 1,800 submissions.
  This tiny 32x32 synthetic workload is not campaign FPS, compositor latency
  or an estimate of real-game percentage improvement.
- Existing stereo/fixed-state suite: **28,689**, **45,077 cumulative** with
  compositor tests, plus 12 benchmark checks. Includes injected Multiview
  shader failure and restoration against Reference/Compact.
- Existing Multiview allocation/shared-shader/coverage: **427**.
  Copy/MRT/fallback: **3,285**. World/shadow/allocation: **73**.

Reproduce the new device suite with the Android NDK compiler:

```sh
CXX=<NDK-aarch64-linux-android29-clang++> \
  bash scripts/qa/xr-uniform-cache-test.sh <fresh-output-directory>
adb -s <quest-serial> push <fresh-output-directory>/uniform-cache-test /data/local/tmp/generals-uniform-test
adb -s <quest-serial> shell /data/local/tmp/generals-uniform-test
```

Use scripts/README.md for the other extraction suites. Build/deployment
evidence and rollback packages are retained under build/quest-p16/ and
build/quest-p15/. Device fixtures do not prove every real material variant,
sustained battle performance or worn-headset comfort.

### Next physical gate and remaining optimization candidates

Start a campaign normally with active controllers. Verify P16 Multiview,
unchanged custom placement/quality, video-to-tabletop return, animated units,
shadows, particles and transparent edges. Compare the same warmed-up scene
and camera to P15 with identical settings; test sustained combat and orders.
First use Timing OFF, then briefly enable CPU/FPS measurements. Keep user
playability feedback distinct from a controlled FPS claim.

The ordinary composed-world draw is deliberately still present. Removing
it safely (while retaining UI, cinematics, coverage and same-frame fallback)
and earlier geometry visibility rejection remain separate later changes.
Neither is implemented here, and neither should be combined with an
unmeasured quality downgrade.

### Deployment result

P16 installed with adb install -r on Quest 2G0YC5ZG9609PY. The installed
base.apk SHA-256 matches the XR artifact. Both APK v2 signatures verify.
The current **362-byte v9 layout is byte-identical before/after install**,
including the deliberately saved quality and placement. Normal startup
reaches Horizon's Controller Required dialog; no game PID, no dialog bypass.
P16 campaign performance and actual in-game default still require the
active-controller physical gate above. P15 remains available for rollback.

```text
657993d81285c565020e09b2d683af868c2ed71c67a0ffc910827ab42211e437  Generals-Quest-P16-UniformCache.apk
2e65214dc1b2f16d35750349a289992f8fd92e7212f1c885212e41ea7804f83d  Generals-Android-P16-UniformCache.apk
```

## 41. P16.1: build-panel spacing, height diagnosis and next performance candidate (2026-09-14)

### Requested placement change: implemented

Move the detached build panel 0.10 m away from the player along the saved
launch heading. Default depth changes -1.08 -> -1.18 m. Height, tilt, width,
board and composed-screen pose are unchanged. The photo preset uses this
same position. Current custom layouts receive only the depth delta once.

Layout version 10 accepts versions 1-10. v7-v9 get the one-time delta; v1-v6
get the new photo preset directly without a double shift. P15's quality
migration still applies only to pre-v9. Saved v9 High/Balanced, view intent,
zoom, language, handedness, visibility, decoration settings and snap flags
remain intact. Subsequent user moves are preserved across restart. An
already extreme custom pose is left alone if the delta would violate the
existing 5 m validation bound.

Validation: native z_generals and both zh/xr packages build; **8,537 host
checks pass**, including migration/load/save/one-shot/custom-tilt and the
existing input, tactical, cinematic, localization and audio regressions.
The actual device layout also passes a separate production-header migration
probe: build depth -1.080 -> -1.180 m, every other field unchanged.
There are no GLES/shader/math changes; no new GPU performance claim.

### Tabletop height: investigated, not changed

The user believes the build panel does not move with the table, but is not
certain. Two concrete mechanisms exist:

1. GX_XR_BeginStereoWorld uses current center terrain Z in world-to-board
   mapping. Panning over a different elevation changes the rendered world's
   vertical offset without changing the physical surface pose.
2. xrBuildBoard derives its underside from the lowest sampled visible edge.
   Panning/zooming changes that minimum. The production-header geometry
   probe keeps the table pose fixed but moves its underside from -0.0594 m
   to -0.1419 m to -0.2244 m as normalized edge height changes 0/-0.05/-0.10
   at 1.65 m board width.

The read-only P16 runtime sample has only one layout-anchor event, a stable
sampled gameplay panel position, and center terrain Z changing
15.6 -> 49.6 -> 15.6. This is consistent with geometry-relative movement,
but samples are sparse and there is no timestamped headset reproduction.
Reference-space changes are another path: updateInteraction re-anchors to
current head height. Do not claim that source inspection rules tracking out.

Proposed separate fix: make the physical plinth/base independent of visible
terrain minima, and use an explicit stable map/session elevation datum for
world mapping. Preserve real terrain relief and shared rendering/picking
transforms. Validate map switches, campaign videos, cliffs/valleys, extreme
coverage/zoom and low/high maps against vertical clipping before enabling.
Do not mask the symptom with smoothing, flatten terrain, or change game
camera/simulation rules. Confirm whether any windows move during a repro.

### Next performance step: proposal, not implemented

P16 gameplay logs confirm actual Multiview and 96-98% uniform cache hits in
sampled play. The cache now avoids most repeated values; its hit percentage
is not frame rate, and no identical-scene old/new comparison is available.

The larger candidate is the ordinary composed-world draw: drawCommon still
submits it before every eligible stereo draw. P14 removes only the later
planar copy, not this rendering work. Investigate a separately toggleable
stereo-only rendering path without reducing eye resolution or visual detail.
Its acceptance contract must preserve native menus, video/loading, MRT UI,
coverage probes, generated-shader failures and safe same-frame recovery.
Do not merely remove submit(), rely on previous-frame readiness, or run a
second game/scene traversal; mutable dynamic buffers must remain valid.
Only ship default-on after equivalent output and actual same-scene A/B.

After that, conservative early rejection of objects/terrain outside the
table volume is another candidate, with explicit shadow/particle/oversized
object margins and unchanged simulation. Fragment clipping alone saves no
submission or vertex work. Neither candidate is implemented by P16.1.

### Deployment and physical gate

P16.1 installed with data preserved on Quest 2G0YC5ZG9609PY after confirming
no running game. Both APK v2 signatures verify; installed base.apk hash
matches the XR artifact. The actual 360-byte v9 layout is unchanged before/
after install and after the normal launch request. Horizon's Controller
Required dialog prevents a game PID; no bypass. Migration is therefore
validated by host tests and the actual-layout probe, but has not yet run in
the headset. Activate both controllers and check the extra 10 cm clearance,
unchanged tilt and the native expanded menus.

Artifacts, logs, expected v10 layout and pre-update layout are in
build/quest-p16-1/. P16 APKs remain under build/quest-p16/.
For rollback after successful migration, restore the saved v9 layout along
with P16: P16 cannot parse v10. Do not silently discard the user's layout.

```text
37f334fc46d30a716f6c319fac714b82be9e5368914a1a12670feb4376db047f  Generals-Quest-P16.1-PanelSpacing.apk
93ea9bdb485753d379dd1d51fd0d18e2d874021de3968661b529643abcba5c7b  Generals-Android-P16.1-PanelSpacing.apk
```

## 42. P17 - omit redundant ordinary-world draws (headset-test candidate)

User authorization: implement the removal discussed after P16.1. This is
not removal of native flat panels, campaign videos, loading or the Android
game. Those remain required. No tabletop-height, panel-pose, layout-format,
eye-resolution, shadow/detail or simulation change is bundled here.

### Implementation and current-frame gates

Auto now controls omission of eligible ordinary world submissions as well
as the older planar-copy optimization. Require current stereo eligibility,
coverage certified for the current allocation, allocated UI MRT resources,
healthy Multiview and no latched recovery failure. Keep ordinary draws for
Reference/Atlas, initial allocation/coverage/UI setup, explicit PPM capture,
RTT and non-world/UI content. Existing program/uniform/state setup and
restoration remain; only the redundant draw submission is skipped. Both
eyes still consume each dynamic draw immediately, without re-traversing the
scene or replaying mutable vertex/index buffers.

Track incomplete composed pixels per actual native Present. Both planar
and composed getters reject incomplete output. Empty Presents cannot mark
old partial contents complete; only a full backbuffer color clear supplies
a fresh base, then any later omission invalidates it again. Partial viewport
or depth-only clears do not recover color. Native world/movie Begin_Render
already clears the full image. The UI attachment remains an independent
MRT output; forced copying cannot expose a partial snapshot.

### Explicit fallback limitation and revised acceptance

Generated Multiview shader failure still falls back to per-layer Reference
shaders on the same draw, retaining both eye images. Further ordinary draws
are not omitted after the failure; allocation can revert next frame.

If a late failure also removes valid stereo/UI after earlier ordinary draws
were omitted, exact same-frame ordinary reconstruction is unavailable. Do
not pretend a clear color plus UI is a complete fallback. Show a localized
recovery card, suppress input, and latch full ordinary rendering until a
fresh complete frame is published. This latch is reset by leaving Auto or
leaving/re-entering eligible split rendering. Nested movie presentation
also checks completeness and removes a previous recovery card as soon as
its full movie frame is available.

This qualifies section 41's proposed same-frame recovery contract: safe
presentation is guaranteed by refusing known incomplete content, not by
reconstructing the original planar scene in that frame. An exact fallback
would need a separate immutable command/buffer replay design. The recovery
card may cause a brief interruption; device fault and campaign transitions
remain acceptance gates. Repeated recovery is a failure, not acceptable
normal play. Do not add buffer replays, glFinish or per-frame coverage reads.

The test build follows the existing Auto default so the user can exercise
the approved optimization; this is not release/default-performance
acceptance. The section 41 controlled scene A/B gate remains open.

### Controls and observability

- View settings: Extra world: Auto / Always.
- Auto: conditionally omit the extra ordinary world; Always: full reserve
  for direct comparison. Neither setting enables selectable flat gameplay.
- Presentation status shows Extra world off/active from the published number
  of skipped draws, rather than echoing the requested toggle.
- P17 ordinary-world logs count actual saved submissions per 120 native
  Presents. No GPU timer queries or per-draw log output were added.

### Automated validation

8,775 host checks pass, including bilingual recovery text, all policy gate
combinations, view recovery, nested movie recovery, layout and input/game
bridges. Native engine and both APK flavors compile/package successfully.

On Quest 3 (2G0YC5ZG9609PY, Adreno 740), extracted production GLES paths pass:

- 46,905 stereo state checks across 1,024 scenarios, 63,293 cumulative with
  the actual compositor. Normal and injected Multiview-shader-failure paths
  retain identical eye pixels; measured GL call counts decrease by exactly
  the omitted ordinary submissions. Following state/dispatch remain valid.
- 3,313 production copy/MRT/clear/publication checks, 3,321 including copy
  benchmark checks. Incomplete/empty/partial/depth-only frames stay hidden;
  a complete new native frame recovers. Forced-copy requests stay safe.
- 427 Multiview allocation/coverage checks, 8,667 uniform-cache checks and
  73 world-shader/eye-allocation checks.

Shader/resource fixtures are synthetic, production methods are extracted.
The existing copy/atlas/cache benchmarks do not establish a P17 campaign
FPS gain. There is no measured same-scene P16.1/P17 speedup claim.

### Physical acceptance and rollback

Check actual Multiview and Extra world off while playing. Compare Auto/Always
in the same paused/calm camera view, after warmup, then exercise movement,
selection, building, particles and shadows. Run campaign intro through to
tabletop, loading, pause/options, general-powers tree and Communicator.
There must be no persistent recovery card, missing world/UI or new flashing.
Confirm the P16.1 build-panel clearance and unchanged table placement.

Artifacts and validation evidence: build/quest-p17/. Keep P16.1 APKs for
rollback (same v10 layout support); do not overwrite or reset user layout.
The older P16 APK still requires the corresponding saved pre-v10 layout.
Height-bobbing remains the separate section 41 follow-up, not fixed by P17.

Deployment: both APK v2 signatures verify, installed Quest hash matches.
Installation preserves the actual 360-byte v9 layout. Normal launch is
blocked by Horizon's Controller Required dialog, with no bypass; P16.1's
pending v10 migration has not run. No real P17 gameplay/savings/FPS evidence
is claimed yet. Activate both controllers for the physical gate above.

```text
11bb1a94cd0313873f465df89933cf78a106b1907b7fa14c046c0b57a03cc975  Generals-Quest-P17-WorldElision.apk
f8c4e21500e5ffc262a8e97961686dadcd8f3dfa49ea45be2968decc030c0080  Generals-Android-P17-WorldElision.apk
```

## 43. P18 - stable tabletop height, plinth and shared picking transform

User explicitly authorizes the three proposed steps, after calling P17 the
major performance unlock. P17 is accepted qualitatively for playability;
sampled runtime confirms High/Multiview, Extra world off and around 590-1,230
omitted draws per native frame without recovery in those intervals. This
supersedes section 42's last-launch-only observation, but is not a measured
same-scene FPS gain. P17 remains the rollback for this separate visual step.

### Implemented

1. Fixed physical plinth: top -0.018, underside -0.036 in board widths,
   independent of the lowest visible edge. Soil sides retain sampled relief;
   only the redundant edge-minimum scan is removed. Physical resizing still
   scales the whole table intentionally. No saved surface pose is changed.
2. Stable world height: use TerrainLogic's map-wide loaded minimum Z, not
   the terrain under the center pixel. The native loader already computes
   this range over the whole map, independent of active mission boundary.
   New/reloaded maps provide fresh values even with reused object pointers;
   cinematics do not create a new first-view datum. No per-frame terrain
   scan, smoothing, game-rule or shared terrain/camera mutation.
3. Coherent mapping/picking: world center XY follows the native look-at
   point; yaw follows native angle. Span is derived from user-desired camera
   height, horizontal FOV, fixed W3D default-pitch distance and XR coverage,
   rather than three terrain hits. This also removes accidental world-scale
   changes while panning across slopes or native camera collision correction.
   Existing inverse rays, build/ability aim, model picks, selection rectangle
   and overlays consume that same published mapping. Native command rules
   and touch/non-XR paths are unchanged.

The span formula is 1.6 * desiredHeight * tan(horizontalFOV/2) /
sin(ViewDefaultPitchRadians) * coverage, clamped to the existing 200-3000
world-unit range. It retains the 80% central-width framing convention;
500 height / 50-degree FOV gives about 612.8 world units at coverage 1.
Native pitch no longer changes table scale through terrain intersections.
The stereo perspective still comes from real eye poses and the freely
rotatable/tiltable physical tabletop, not from the flat camera projection.

### Terrain envelope and compatibility

A fixed datum can put high terrain above the old normalized +0.5 ceiling
when zoomed close. CPU sphere culling, bounded picking, stereo/MV clipping
and feedback now share a ceiling derived from the native byte-height limit
(255 * 10/16 = 159.375 world units), the existing matrix and +0.5 headroom.
The engine adapter statically asserts this scale. No new per-draw uniform
uploads, no vertical exaggeration/flattening, no second scene traversal.
Horizontal clipping and the lower -0.15 guard remain. Cut sides are bounded
to the fixed base and the same upper envelope; frame geometry still bypasses
feedback clipping to close its underside.

This is conservative across retail heightmaps, not a claim to support every
modded height representation or aircraft altitude. Native terrain edits that
go below the loaded map minimum can still extend beneath its chosen datum;
no gameplay deformation is rewritten to hide that. Test craters/seismic
effects during acceptance. Genuine tracking/recenter movement is separate
from this terrain-relative fix and is not suppressed.

### Validation and physical gate

- Native build and both zh/xr package builds pass; no changes to layout v10,
  user High/Balanced, light/original shadows, window tilt or P17 omission.
- 8,775 existing host assertions pass. 23 checks compile the actual mapping
  adapter against spies without terrain-pick/camera-write methods, covering
  scroll, fresh-map extents, zoom, cinematic gating and invalid inputs.
- 272,818 parametrized height/plinth/render-ray checks pass on host UBSan
  and Quest ARM64: low/high maps, yaw, board tilt/width, pan and 200/619/3000
  spans. Check fixed trim vertices, true relief, model/ground target ray
  intersection, roundtrips and high-terrain culling. These are numerical
  contracts, not a replacement for real engine collision/placement tests.
- Quest GLES: 1,339 allocation/MV shader checks (including terrain above the
  former ceiling and above-envelope rejection), 77 world/feedback checks,
  46,905 state checks / 63,293 cumulative compositor checks, 3,321 copy/MRT
  publication checks. No P18 FPS improvement is inferred from microbenchmarks.

In a campaign and Skirmish, pan from low ground across hills while leaving
the table pose/zoom unchanged: the base must stay put and equal elevations
must remain at equal room height. Check extreme zoom, physical size/yaw/tilt,
map edges, cliffs, water, bridges, craters, aircraft and campaign boundary
expansion. Select nearby/distant units, drag a rectangle and place/rotate a
building: ray marker, preview and committed location must agree. Play a
campaign movie and return to the table; load a different/save map. No new
flashing, missing relief, persistent recovery or P17 performance regression.

Build and evidence are in build/quest-p18/. Roll back with the retained P17
APK using data-preserving install; no P18 settings migration needs reversal.
This completes implementation, not the user's worn-headset acceptance.

### Deployment evidence

Installed P18 on Quest 2G0YC5ZG9609PY with data preserved, after verifying no
running game. Both APK v2 signatures pass and the installed base.apk hash
matches. The actual 363-byte v10 layout is byte-identical before/after.
Normal launch is intercepted by Horizon's Controller Required dialog; no
game PID or bypass. Actual P18 gameplay/height/placement acceptance is open.

```text
edf76b3e8f122eafa3a78b03282bba3565c3b96c74c8b475f19d3736ab206a80  Generals-Quest-P18-StableTable.apk
4a05ce304ea65ad2cd168da0a51e5d2eb99cc4bc3ed65c67caf193ec06c95c75  Generals-Android-P18-StableTable.apk
```

## 44. P18.1 - building orientation, visible editing target and controller guide

User authorizes these two interaction improvements plus an in-game controller
reference. Implement before the proposed P19 real-surface placement. P19 is
not part of this build; scene-plane discovery, permissions, explicit preview
and confirmation still need their own implementation and device gate.

### Construction input

- While a rotatable building preview is pending, hold the supporting hand's
  Grip and move the pointing hand's stick horizontally. Continuous rotation,
  deadzone 0.25, maximum 90 degrees/second, bounded frame delta. Thus default
  right-handed input is left Grip + right stick; left-handed input mirrors it.
- Only the native W3D placement ghost is rotated. Its existing orientation
  feeds native preview/path validation and PlaceEventTranslator's normal
  construction/special-power message. No new order, protocol or simulation
  rule, no synthetic drag, no cached ghost pointer and no touch-path change.
- Map pan/yaw/zoom and stick-click camera cycling do not fire in that context.
  After releasing Grip, center the pointing stick before camera control resumes.
  Freeze the angle on trigger press, while held and on release, because the
  native placement click is processed later in the frame.
- Pointing Trigger still places; pointing Grip still cancels. Outside building
  placement, supporting Grip remains additive selection. Arrangement still
  owns one-/two-hand grabs. UI, dialog, focus, tracking and neutral rearm gates
  remain authoritative. Rotation requires a current spatial ray, not UI hover.
- A bilingual contextual card shows the combination, confirm/cancel buttons
  and an approximate angle (readout rounded to 5 degrees; rotation is continuous).
  Releasing Grip retains the actual angle. No rotation of finished buildings;
  line-construction templates retain native behavior and are excluded here.

### Visible editing selection

- UI -> Windows selected table/build-window button has a persistent orange
  stroke and check mark, independent of cyan hover. Header names the editing
  target. Corresponding physical surface gets the same display-encoded orange.
- During Grab / arrange, the outline follows the active arrangement slot,
  including stick-click target changes. Closing/exiting removes it. Other
  workspace tabs, help, movies and recovery do not acquire an editing target.
- Use the actual displayed crop/pose for the build panel, including expanded
  native dialogs. Table outline follows the fixed P18 plinth's outer rim.
  Thin 4 mm overlay, transparent center; one quad per eye only while editing.
  No game texture read, second world rendering, terrain scan or layout migration.

### In-game controller guide

Every normal workspace tab has a top-right ? button. UI -> ? opens four
localized pages directly, also in the shell: play/selection, building rotation,
window arrangement, and groups/orders/extra camera shortcuts. Text substitutes
the actual physical hands and A/B/X/Y buttons for the current handedness.
Next cycles pages; Back to windows returns; X/Back closes. Input is captured
by the guide, never dispatched as an order through its surface. Existing
command-console help remains available beside the army controls.

### Validation and acceptance

Production routing/text and extracted native-preview adapter tests cover both
hands, held/releasing trigger, Grip release, neutral stick rearm, camera/modal
gates, missing/replaced ghosts, line templates, invalid deltas, and outline
pose/size across tilted/cropped surfaces. Existing placement, picking, height,
commands, group/language, workspace, loading and shadow-scope regressions pass.

Quest GLES uses the actual compositor shader: opaque orange perimeter,
untouched transparent center, existing Reference/Multiview/ordinary-world
omission regression cases pass. Actual Android Canvas renders 68 payloads,
including all 16 language/hand/controller-page variants at the normal 26 px
help font without clipping (maximum 789/820 px). Persistent button selection
survives hovering the other target. Selected-window and longest-guide images
were inspected. These fixtures do not establish worn-headset readability,
actual controller bindings or a complete engine construction cycle.

Build/install evidence and rollback information: build/quest-p18-1/validation.md.
P18 APK retained; saved v10 arrangement, graphics choices and game data are not
migrated or reset. No new FPS claim and no multiplayer eligibility expansion.

Headset gate: place one rotated building with each faction in Skirmish and
campaign. Check footprint/exit orientation, valid/invalid sites, insufficient
funds, cancellation, repeat placement and both hands. Grip release must not
rotate/zoom the map until the stick is neutral. Trigger press/release must
retain preview orientation. Then verify table/build-window outlines after
size/tilt/crop changes, target switch, Done/Back, guide navigation and normal
map control afterward. Human multiplayer/replays remain the separate QTR-MP
agenda item; real-surface placement remains P19.

Deployment: P18.1 installed on Quest 2G0YC5ZG9609PY, installed APK SHA-256
cae6c1c1e2ec9f2843efe8fd7a5d7ce2ad03f85441162304cf1f5e89d5ec3a8f
verified. Both APK v2 signatures pass. Actual 361-byte layout unchanged.
Normal launch waits for active controllers; no game PID or bypass. Isolated
Canvas QA app removed after evidence retrieval; physical acceptance stays open.

## 45. P19 - real-surface placement and complete build hover information

User decision (14 September 2026): **P18.1 is tested and accepted**. Build P19;
prices must be accessible when hovering producible units/buildings. Audit other
missing information. Create the user's GitHub repository **after P19 passes
their worn-headset test**, not merely after compilation or installation.

### Scope and interaction

UI -> Windows -> Place on surface opens an optional native OpenXR scene workflow.
Load room data requests USE_SCENE permission through Android on explicit use.
The XR manifest also declares USE_ANCHOR_API: the native spatial-entity
extensions require that capability even though room-data consent is separate.
Denial, missing extensions, query failure and an empty scene do not disable
manual arrangement or gameplay. Capture room explicitly opens system room
capture; afterward reload room data. No background room scanning or network
upload is added.

Choose Tables or Floor, optionally enable Fit to surface, then Start preview.
Aim at a recognized bounded supporting face. An orange outline previews the
board; the pointing trigger confirms only after neutral input. Either Grip or
Back cancels. Invalid/too-small hits cannot place; aim near the center or opt
into fitting. Fit only reduces the existing width, never enlarges it, and
retains the 0.45 m minimum. Default is to preserve width. Pointing roles mirror
left-handed mode. Gameplay commands, map navigation and grabs are blocked
during preview; exit requires neutral controls before normal gameplay resumes.

The plinth underside (-0.036 board widths) rests above the chosen plane with
2 mm clearance, not the terrain center. Slight scene-plane slope is compensated
upward so a horizontal plinth does not penetrate it. Windows move rigidly with
the board translation/yaw, keeping their existing pitch and scale; the command
window continues to derive its upright pose from the build window.

Tables use the upper supporting face of the oriented scene volume, not a
hardcoded local axis. Floors use scene polygons where available, otherwise the
runtime's bounded rectangle. Walls/ceilings, untracked poses and distant or
out-of-bounds hits are rejected. Fit checks the current footprint including
the plinth lip. Room bounds are estimates, not collision/safety guarantees:
they do not establish free space above the surface or detect all obstacles.
Changing size or viewport shape afterward requires a new fit if desired.

Confirmation copies the pose once; there is no continuous scene snapping.
Saved layout remains the existing launch-relative v10 arrangement. P19 does
**not** create persistent spatial anchors or automatically bind to the same
physical furniture across restarts/recenter. That remains a separate follow-up.

### Hover audit and delivered information

| Information | P19 behavior |
|---|---|
| Unit/building price | Original player-adjusted cost, including discounts; free production explicitly shows zero |
| Prerequisites and unaffordable purchases | Original requirements and insufficient-money descriptions |
| Production restrictions | Queue full, no parking and player/unit/building limits from original tooltip evaluation |
| Upgrades | Price, purchased/conflicting status and missing Generals promotion |
| Generals ability tree | Native command resolution includes expanded science widgets; science-point costs, not fabricated dollar prices |
| Energy, money, experience | Original static tooltip descriptions; power includes production/consumption values |
| Other native widgets | Existing localized static tooltip text where provided by the game |
| Long descriptions | Larger legible card; line-aligned pages every 10 seconds rather than dropping the tail |

ControlBar::describeCommand factors the existing command tooltip calculation
into a read-only method shared with the original popup. XR never creates a
hidden popup or sends a purchase command to obtain a description. Native
tooltip picking preserves modal/Z-order ownership and includes disabled and
non-input widgets; gadget data is validated against the command list.

Price/queue changes do not reset the initial 300 ms hover delay. Evaluation is
throttled while stationary, and texture uploads occur on content/page changes.
DE/EN XR guide and scene controls are included. Retail tooltip text continues
to follow the installed game language; absent language assets are not invented.

Audit boundary: health remains available through existing health bars and
unit feedback; production/cooldown progress remains on original queue icons
and radial indicators. Numeric remaining-time readouts, pinning a detailed unit
inspector and human-multiplayer player/chat information are not added by P19.
No hidden enemy/shroud data is exposed. QTR-MP (human LAN/internet multiplayer,
replay XR, chat/diplomacy/network-specific controls) remains explicitly planned;
offline AI Skirmish is not multiplayer validation.

### Implementation and validation

New XrSceneSurface / XrScene / XrSceneUI split geometry, asynchronous ownership
and modal input. Required scene extensions are optional as a group; capture
is separately optional. Queries are on demand, bounded to 256 results/5 s,
matched by request ID, and own/destroy returned spaces on refresh/shutdown.
Late results never reopen placement after cancellation. No scene locate loop
runs during ordinary gameplay.

Host coverage: original cost/status calculation, modal placement/rearm/failure
paths, rotated volume faces, floor bounds, plinth clearance, fitting and
companion transforms. Existing construction/selection/controller, layout,
loading/campaign-policy and performance regressions remain green. Real Quest
GLES retains 46,905 state assertions / 63,295 cumulative compositor assertions.
Actual Canvas verifies scene controls, 16 DE/EN/hand-specific guide variants and
multi-page cost/requirements cards; inspected screenshots are synthetic test
data, not a claim of native gameplay acceptance. Final build/install hashes,
logs and device gates belong in build/quest-p19/validation.md.

**Worn-headset acceptance still required:**

- Load/allow room data; select table and floor separately; exercise capture if
  the existing room setup has no relevant surfaces. Also test denied permission.
- Aim near furniture center, verify preview footprint, fit on/off, cancellation
  and confirmation. Check underside clearance and both companion windows.
- Pan/rotate/zoom after confirming/cancelling. Move the head, change selection
  and play both Skirmish and campaign: no involuntary resnapping or input leak.
- Hover representative units, buildings, disabled purchases, upgrades and
  science buttons. Check real costs and requirements, including discounts.
- Confirm guide/card readability, long-text page changes, and saved manual
  arrangement after a restart (not a persistent room anchor).

### Next milestone: GitHub preservation after acceptance

Once the user confirms P19 works, prepare a clean, asset-free source repository
in the user's GitHub account, with build/bootstrap instructions, licenses and
an authoritative ROADMAP.md linking this plan. Exclude retail game data, keys,
local paths/secrets, generated build trees and room data. Preserve upstream
attribution and submodule/dependency instructions; validate a clean clone.
No repository, remote, push, public visibility choice or release is performed
as part of this pre-acceptance build.

Device bring-up correction: the first P19 package declared USE_SCENE only and
reported scene=0/capture=0; the user observed no action on Load room data.
The official Meta SceneModel manifest requires USE_ANCHOR_API as well. The
corrected installed package reports scene=1/capture=1 and XR init complete.
User room permission and a successful physical placement are still separate
gates. No permission was force-granted.

API references (checked 14 September 2026):

- [Meta SceneModel manifest](https://github.com/meta-quest/Meta-OpenXR-SDK/blob/main/Samples/XrSamples/XrSceneModel/Projects/Android/AndroidManifest.xml): both USE_SCENE and USE_ANCHOR_API.

- [Meta spatial data permission](https://developers.meta.com/horizon/documentation/native/native-spatial-data-perm/): optional USE_SCENE consent and refusal fallback.
- [Khronos query dependencies](https://registry.khronos.org/OpenXR/specs/1.0/man/html/XR_FB_spatial_entity_query.html): enable spatial_entity_storage along with spatial_entity/query/scene.
- [Khronos local-storage filtering](https://registry.khronos.org/OpenXR/specs/1.0/man/html/XrSpaceStorageLocationFilterInfoFB.html): chain the local-storage filter to the semantic-component filter.
- [Khronos 2D surface bounds](https://registry.khronos.org/OpenXR/specs/1.0/man/html/xrGetSpaceBoundingBox2DFB.html) and [3D bounds](https://registry.khronos.org/OpenXR/specs/1.0/man/html/XrRect3DfFB.html): use space-local geometry with located poses.

## 46. P19.1 - optional play-space setup and stable room coordinates

User approval (14 September 2026): implement all three follow-ups from the
P19 physical test: missing-table feedback, posture/reference stability and
a clear optional workflow available before a match.

### Delivered behavior

- Normal startup does not request room permission, scan or bind furniture.
  Existing free layout preferences remain intact; no v10 migration or reset.
- UI -> Windows -> Set up play space opens a dedicated sequential DE/EN wizard.
  Choose a free-standing board, real surface, or manually defined height.
- Real surface selection loads room data on explicit entry and displays separate
  table/floor counts. Select the type to preview. Reload/capture/manual fallback
  and Fit are available at this step; instructions are not disguised as buttons.
- Permission refusal and missing tables retain manual/free alternatives. Capture
  completion automatically reloads after focus returns. Long system capture
  and permission do not consume the startup-only session-READY timeout.
- The preview works in the main menu without a running simulation or game world.
  Movies, loading and live cinematic/modal locks still prohibit entering it.
- Detected horizontal surfaces have cyan outlines. The board footprint remains
  visible orange when valid and red when too small/out of bounds. These are
  geometric estimates, not obstacle-clearance or safety guarantees.
- Use 2D tabletop geometry before the volume fallback. Clutter in a stored
  room scan can still require OS recapture; the app does not recognize live
  objects or upload room geometry.
- Fit defaults on, shrinking only when necessary; it may be disabled.
  Pointing-hand stick up/down changes preview size, left/right rotates.
  Handedness already maps the controller roles. Trigger confirms; either Grip
  or Back cancels with no layout modification.
- Manual mode records the controller's aim-origin height on a fresh trigger
  press, then intersects the ray with that horizontal plane. Release and press
  again to confirm the board. No permission or room model is needed.
- Confirmation aligns the plinth underside and moves companion windows while
  retaining their existing pitch/scale. Settings changes remain reversible.
- Successful placement is explicitly session-local. No persistent spatial
  anchor or automatic real-furniture binding across app restarts is claimed.

### Reference-space correction

Previously LOCAL reference change discarded poseInPreviousSpace/poseValid and
called the initial head-relative layout placement again. That could change board
height with current posture. Queue changes by changeTime and apply inverse
poseInPreviousSpace to surfaces, layout anchor and setup panel before rays and
rendering. Normal head movement and valid origin changes do not reset the layout.
Multiple changes are ordered. Preview/input captures are cancelled and rearmed.

If the runtime supplies no valid old/new relation, hide the stale workspace,
cancel commands and show a head-readable recovery wizard. Only an explicit free
board or new placement recovers it. This does not promise perfect spatial-anchor
drift correction: continuously updated/persistent anchors remain separate work.

### Validation and acceptance

Production host tests cover known-origin compensation, invalid-origin rejection,
plane-before-volume selection, detection versus fit, wizard actions in shell
and game, two-press manual height, size/yaw cancellation, input rearm, permission
failure and menu hit geometry. Run existing host regressions, Android native
build, both APK flavors, Canvas layout checks and the Quest compositor fixture.
Build evidence belongs in build/quest-p19-1/validation.md.

Physical gate: test loaded floor/table and manual height, preview fit and rotation,
Back/Grip at every step, permission denial/capture return, setup before Skirmish,
campaign playback transitions, left-handed controls, sit/stand and system recenter.
No hardware acceptance is inferred from host tests or installation.
GitHub source preservation waits for user acceptance. QTR-MP human multiplayer,
replay XR and networking-specific controls remain planned; offline AI Skirmish
remains the supported primary test path and is not proof of network compatibility.

Build outcome: native and both APK flavors pass with JDK 17 / Gradle 8.7.
Host scene/wizard/reference checks pass (1,280); actual Quest Canvas passes
88 panels and compositor passes 63,299 cumulative assertions. Quest P19.1 is
installed with matching hash and unchanged 370-byte layout. Exact artifacts,
reproduction and still-open physical acceptance: build/quest-p19-1/validation.md.

## 47. Public identity - Generals: Zero Hour XR

User accepts P19.1 and requests consistent product identity before source
preservation. Public name is **Generals: Zero Hour XR**. After explaining that
a new application ID creates a separate Android app/data domain, the user
explicitly chooses to retain **com.generalsx.zerohour.xr**.

Update XR launcher/application labels, setup headers, log-share subjects,
background-permission copy, OpenXR application and localized action-set names,
workspace heading, README and APK export. XR locale overlays cover every
existing localization; the standard Android zh flavor is unchanged.
Keep Java/JNI symbols, storage paths, original retail logos/artwork, upstream
engine identity, attribution and license notices. No gameplay/renderer change,
data migration, permission reset, app uninstall or GitHub upload is required.

The build exports build/apk/Generals-Zero-Hour-XR.apk while retaining the normal
Gradle outputs. Validate both flavors, merged APK labels for all locales,
OpenXR name, same package/signature, native/host checks and preserved device
layout. Physical launcher readability is separate from resource verification.

Validation outcome (2026-09-14): native build and both APK flavors pass;
235 bilingual panel assertions, 1,280 scene/ownership assertions and branding
checks pass (89 compiled app labels, 13 source locale overlays). Both APKs
verify with APK Signature Scheme v2. Quest update installation succeeds under
the retained package ID, with the 342-byte layout unchanged. Installed APK
SHA-256 matches the exported product APK and cold activity launch returns OK.
No new worn-headset gameplay/launcher acceptance is inferred from these checks.
Detailed evidence: build/quest-branding/validation.md. No GitHub upload yet.

The private source snapshot is now published at
`github.com/Cesarus85/Generals-Zero-Hour-XR` on `main` (commit
`0e7cc9e1cd986779b161c8839feb845a3b028ba0`). The Quest preview release
`xr-preview-2026-09-14` carries `Generals-Zero-Hour-XR.apk` and its SHA-256
sidecar; both remain private until an explicit visibility change is requested.

## 48. Next XR implementation queue - placement, window presentation and desktop input

The user requests these items for the next implementation cycles after the
GitHub source snapshot. They are intentionally separate from the accepted P19.1
room workflow and from the deferred multiplayer milestone.

### P20 - safe fresh placement when no play surface is defined

When the player has no confirmed play surface for the current session, present
a fresh tabletop board and the build window directly in front of the player at
a comfortable default distance and height. Do not silently restore the last
session's world-relative pose in this state: a saved pose from home can be
unsafe or unusable while travelling. Mark this as a new-session fallback, not
as a deletion of the saved arrangement. Once the player confirms or manually
adjusts the fresh layout, normal per-session save/restore may resume.

Acceptance: save an intentionally oversized/off-axis arrangement, terminate the
XR process, and relaunch in the same or another room. Board, build window and
Commands window must use the compact front-of-player preset while language,
graphics, handedness and map-coverage preferences remain unchanged. A surface
or manual/free arrangement confirmed in the new process must survive shell to
Skirmish/campaign transitions. Denied scene permission and tracking loss must
fall back safely without crashing or issuing orders.

Implementation note (2026-09-15): because this port intentionally owns no
persistent room anchor or stable room identity, it cannot safely prove that a
saved pose belongs to the current room after process restart. The implemented
safe contract therefore restores non-spatial preferences but resets all spatial
geometry to the photo-inspired free-standing preset for every XR process. An
explicit surface or manual/free arrangement remains active within that process.
The user accepted this initial arrangement but reported later board/build
separation and a campaign board appearing off-axis and low. P20.2 (2026-09-15)
adds one-shot alignment when each match first permits gameplay, unless the
player explicitly chose/adjusted a placement in this process. Plain X in play
now rigidly recenters the whole workspace, not just the board; selected-surface
reset remains in arrangement mode. Fully tracked head/controller poses are
required for placement and input. Lost tracking cancels input/grabs without
writing inferred poses. Nested campaign video presentation now consumes LOCAL
reference-space changes immediately through the same exactly-once path as the
outer game loop. Unknown reference changes recover the upright movie surface
and require tabletop placement confirmation. There is no head-following or
automatic relocation of an explicitly chosen real surface. Retest Skirmish,
campaign, repeated seated/standing X resets and tracking/recenter recovery in
the worn headset; no claim of hardware tracking stability in a dark room.

P20.3 refinement (2026-09-15): user testing confirms session-local manual placement
and fresh-process defaults, but exposes deferred match-entry alignment as a source
of unwanted glance-dependent relocation. Supersede P20.2's match-entry alignment:
initialize once per process; no placement writes on match/camera readiness. Add
**Align everything in front of me** directly to the default UI/Windows page.
Preserve sizes, relative transforms and tilt; use head yaw only. The button and
gameplay X share a leave/cancel confirmation for confirmed surface/manual-height
placement. Refresh the menu to its default page when opening it. Retain all
tracking/reference-space protections. Physical acceptance is required before
merging the P21/P20 follow-up branch.

Navigation follow-up (2026-09-15): supporting stick click returns the camera to
the original command-center target, falling back to the most expensive owned
building. Reuse `MSG_META_VIEW_COMMAND_CENTER`; no selection, orders, tabletop
poses or network protocol changes. Gate menus, camera locks, construction,
editing and tracking/focus transitions; a click consumes that frame's pan/zoom.
Both handedness modes and native radar Grip/Trigger semantics are documented
in the in-game guide. The user accepted the final headset result, including the
base-return shortcut, on 2026-09-15; retain these paths in future regression tests.

### P20.1 - reduce the tabletop underbody thickness

Reduce the visible grey/dark lower plinth to no more than half its current
thickness as a near-term tabletop appearance correction. P18 currently keeps
the terrain/soil transition at -0.018 board widths and the underside at -0.036,
so the lower trim itself is 0.018 board widths thick. Keep the transition and
terrain registration unchanged, but target a lower-trim thickness of at most
0.009 board widths. The new underside must be a single shared constant/contract
for mesh geometry, real-surface clearance, editing outline and tests.

Acceptance: compare the same board pose and scale before/after in the headset;
the underbody must read as a thin tabletop edge rather than a deep block. It
must remain closed from below with no cracks, z-fighting or black flutter. Board
placement on detected/manual planes, selection/picking, terrain relief, window
poses and saved layouts must not move. Update the board, height and scene tests
for the new underside. This task is separate from P21's window redesign.

### P21 - UI and Commands window presentation pass

Rework the spatial `UI` and `Commands` windows as a coherent, polished pair of
companion surfaces for the tabletop. This is an interaction and information-
architecture pass as well as a visual one: both windows must be faster to scan,
more comfortable to reach and clearly part of the same Generals-inspired XR
interface. Use a restrained dark command-surface treatment, consistent panel
depth, typography, spacing and accent colours rather than a generic flat list
of equally weighted buttons.

The `UI` window groups its controls by player intent: view and tabletop,
windows, play-surface placement, graphics/performance, language and help.
Frequently used adjustments remain directly reachable and the currently
selected manipulation target is unmistakable in both the button and the
corresponding spatial object. The `Commands` window leads with selection
context and immediately valid unit orders, separates control groups from
tactical orders, and presents short contextual guidance without crowding the
primary actions. Command availability, armed/targeting state and disabled
actions must be visually distinct.

Panel dimensions and layout adapt to the visible content. Expanded pages,
abilities, help and localized labels must neither be clipped nor leave large
unused grey areas. Preserve comfortable viewing angles, grab/tilt behaviour,
left-handed use, controller hit targets and the established default arrangement
around the board. English and German receive equal layout validation, including
longer German labels. Do not alter original command semantics, simulation state
or multiplayer-facing messages as part of this presentation work.

Acceptance: in-headset comparisons show both windows in the default tabletop
arrangement without overlap with the board or build window; headings, groups,
states and help text are readable at the normal seated distance; all buttons,
hover states, selected targets, disabled states and command-arming transitions
remain unambiguous and free of click-through. Exercise every page in English
and German, both handedness modes, manual window movement/tilt and a representative
multi-unit command flow. The pass must not introduce a measurable sustained
frame-rate regression in the existing balanced/light-shadow/multiview default.
The detailed delegation contract is
`docs/WORKDIR/planning/PLAN-024_QUEST_UI_COMMAND_WINDOWS.md`.

Acceptance record (2026-09-15): the redesigned windows and their default
arrangement were accepted in the headset. P20/P20.3 then stabilized fresh-process
placement and added explicit whole-workspace alignment; the supporting-stick
base shortcut was accepted in the final candidate. Replacement Android CI and
merge/release packaging remain publication work, not feature-development gates.

### P22 - keyboard and mouse support investigation (secondary)

Audit Quest 3 Bluetooth/USB keyboard and mouse discovery through Android,
SDL3 and OpenXR without making desktop input a prerequisite for tabletop play.
First determine which devices and firmware combinations expose reliable
relative mouse, keyboard and modifier events while the headset is worn. Then
map only low-risk equivalents (camera pan/zoom, menu navigation, text entry)
and preserve the controller path as the primary interaction model. Do not
change the simulation protocol or claim multiplayer compatibility from this
investigation.

Acceptance is a documented device matrix, a diagnostic input trace and an
explicit go/no-go decision for a small mapping slice. Full keyboard/mouse
parity remains below controller ergonomics, campaign polish and multiplayer
compatibility in priority.
