# PLAN-026: Quest zoom-out performance PR series

**Status:** proposal only; no code changed; written 2026-09-20 from a
code-verified analysis of `main` at `2a19603`.

**Scope boundary:** this document proposes and stages future pull requests.
It changes no renderer, engine, XR runtime or settings behavior. Sibling
milestones (placement P19, multiplayer, ground-observer campaign gates) stay
out of every staged PR below.

## Symptom and measurement base

The maintainer reports that tabletop performance degrades specifically when
zoomed far out. Every staged PR must be judged on device numbers, not feel.
The required instrumentation already exists and needs no new counter for the
first gate:

- `[d3d8gles] perf: <fps> fps, <n> draws/frame, ...`
  (`Core/Libraries/Source/d3d8gles/src/gles_pipeline.cpp:3360`).
- `[d3d8gles] perf-draws/frame by source: models= sorted(particles)= 2d-ui=
  terrain= shadows= skin= other=`
  (`Core/Libraries/Source/d3d8gles/src/gles_pipeline.cpp:3339`).
- CPU frame/wait/engine/eye means in `XrPerformance`
  (`GeneralsMD/Code/Main/XrPerformance.h`). GPU timer queries stay disabled
  under requested multiview because OVR defines their results as undefined.

**Baseline gate (before any PR):** one logcat capture on the same busy map,
same save, once near-zoomed and once at maximum zoom-out, 60 s each, on the
current Balanced/Multiview defaults. Record fps, draws/frame and the
per-source split for both poses in the first PR body.

## Verified current-state findings

F1. **Terrain coverage grows to the whole map.** Zooming out enlarges the
board span, and `GX_XR_UpdateTerrainCoverage`
(`GeneralsMD/Code/Main/XrGameBoot.cpp:602`) raises the visible terrain area
via `TheTerrainRenderObject->setTerrainDrawSize(cells, cells)` up to the
513x513 clamp. Terrain draws from static vertex-buffer tiles
(`Core/GameEngineDevice/Source/W3DDevice/GameClient/HeightMap.cpp:1365`,
`USAGE_DEFAULT`), but each tile is its own draw with its own shader/texture
state setup (`HeightMapRenderObjClass::Render`, `HeightMap.cpp:1902`), with
cloud and macro-texture passes on top. At full coverage this is the dominant
fixed per-frame draw block.

F2. **No screen-size culling exists.** Visibility is decided only by the
frustum test plus the XR board-coverage sphere hook
(`GeneralsMD/Code/GameEngineDevice/Source/W3DDevice/GameClient/W3DScene.cpp:446,466`
calling `GX_XR_CullSphere`, `XrGameBoot.cpp:591`). At far zoom every object
passes: trees, props, debris and single infantry are drawn regardless of
their on-screen size. Engine LOD preparation is explicitly disabled in
`RTS3DScene` (`W3DScene.cpp:533`, "We're not using LOD yet").

F3. **Fixed per-tier eye resolution, no governor.** `xrStereoExtent`
(`GeneralsMD/Code/Main/XrWorld.h:133`) pins the world eye texture to
1536/1920/2304 target width per tier. Nothing reacts to sustained frame-time
overruns.

F4. **No fixed foveated rendering.** `XR_FB_foveation` is not referenced
anywhere in the tree. PLAN-023 already reserves the UI swapchain as
unfoveated; the world swapchain was left open.

F5. **Particle cap is not zoom-aware.** The global particle cap does not
scale with camera coverage, so far zoom renders the same particle count into
far fewer pixels (overdraw in the sorted pass).

F6. **Shadow cost at far zoom is unmeasured**, not known. The `shadows=`
field of the perf-draws log decides whether shadow work belongs in this
series at all.

## Staged pull requests

Order is A+B first (small, directly aimed at the reported symptom), C next,
D folded into A or shipped alone, E and F only with baseline numbers.

### PR-A: zoom-aware screen-size culling

Extend `GX_XR_CullSphere` (`XrGameBoot.cpp:591`) with a conservative
minimum-radius gate derived from the current coverage span: bounding spheres
below the threshold cull as outside the board volume. The hook only feeds
`RenderObjClass::Set_Visible`, so picking, game logic and mirror passes are
untouched. Threshold must keep combat units visible at every zoom; target
trees, props, debris and small decorations only. Ship behind a session-only
A/B toggle like the shadow A/B in `XrPerformance.h`, default off for the
first device build, then flip default after acceptance.

Acceptance: at maximum zoom-out on the baseline map, `models=` draws/frame
drops measurably with no visible pop-in of units at any zoom step; near-zoom
draw counts unchanged; host regressions plus the stereo/compositor fixtures
pass.

### PR-B: dynamic world-resolution governor

Drive the world eye texture size from the existing `XrPerformance` CPU
frame/wait means: on sustained budget overrun, step the world target width
down from the tier value toward a 0.75 floor; restore with hysteresis after
sustained headroom. Resize only the engine world FBO, never the XR
swapchain; UI and panel surfaces stay at full resolution. Session setting
`Dynamic resolution: Auto/Off`, default Auto on Balanced only.

Acceptance: repeatable zoom-out frame-time dip on the baseline map no longer
drops below the device refresh budget for longer than the governor reaction
window; no resolution oscillation (logged steps only); static near-zoom
image unchanged.

### PR-C: fixed foveated rendering on the world swapchain

Plumb `XR_FB_foveation`/`XR_FB_foveation_configuration` into
`XrHello.cpp` swapchain creation for the world swapchain only, with
capability check and explicit fallback. UI swapchain remains unfoveated per
PLAN-023.

Acceptance: compositor-reported foveation active on Quest 3; world GPU load
drops at identical scene; no visible edge artifacts on UI or board borders.

### PR-D: zoom-scaled particle caps

Scale the global particle budget by current terrain coverage (full cap near
zoom, reduced cap at far coverage). Fold into PR-A if trivially adjacent;
otherwise standalone.

Acceptance: `sorted(particles)=` draws/frame at far zoom drops; battle
readability at near zoom unchanged.

### PR-E: terrain tile draw batching (requires baseline first)

Merge adjacent static terrain tiles into fewer draws at high coverage (e.g.
index-buffer offset batching over groups of tiles, sharing one state setup).
Do not start before the baseline shows `terrain=` dominating draws at far
zoom after A+D have landed.

Acceptance: `terrain=` draws/frame at full coverage reduced by a measurable
factor; identical rendered image (per-eye PPM capture comparison).

### PR-F: shadow update throttle (requires baseline first)

Only if the baseline `shadows=` split justifies it: throttle projected
shadow texture updates at high coverage. Not scoped further until measured.

## Test plan (applies to every staged PR)

1. Baseline and post-change logcat captures per the measurement base above.
2. Host regression suite plus focused `scripts/qa/xr-*` tests per the
   milestone skill procedure; both Android debug flavors build.
3. Physical Quest 3 gate per PR: busy-map zoom sweep with draws/fps notes,
   plus a worn-headset visual check for pop-in (A/D), resolution steps (B)
   and edge artifacts (C). Physical gates are listed explicitly in each PR
   body when not yet performed; nothing is silently omitted.

## Open decisions

- [D1] PR order: A+B first as recommended above, or C pulled forward.
  Recommendation: A+B first; C is independent and can follow immediately.
- [D2] Cull threshold basis: bounding-radius heuristic over coverage span
  (recommended; cheap, robust) versus exact projected pixel size (precise,
  more math in the cull hot path).
- [D3] PR-A default: ship behind session toggle (recommended) versus
  default-on with rollback.
