# PLAN-026: Quest far-zoom performance

**Status:** measurement gate defined; no P26 runtime code has been accepted.

**Current base:** `main` includes PR #27, which removes XR board-mesh allocation
and alpha-loop churn. The installed `1.2.26-xr-board-mesh-test` build is the
device baseline for the next capture. PR #27 does not address the extra terrain
and model work exposed by a far-zoomed tabletop.

This milestone is deliberately limited to sustained tabletop frame cost when
the player zooms far out. Loading hitches, audio starvation and general POSIX
thread support are separate work and must not be presented as P26 FPS fixes.

## Existing branches and pull requests

- PR #28 (`muse/perf-xr-72hz-foveation`) is an independent Quest GPU/thermal
  experiment. It currently combines a 72 Hz request with HIGH fixed foveated
  rendering. It must not be merged as proof of a far-zoom fix: 72 Hz changes the
  frame budget rather than reducing engine draw work, and both features need
  independent A/B controls and a worn-headset gate.
- PR #29 revives POSIX `ThreadClass`; its own null hypothesis is no FPS change.
- PR #30 restores background texture streaming and targets first-use loading
  hitches. It depends on #29 and carries archive/threading risk.
- PR #31 adds audio-underrun telemetry and intentionally changes no audio or
  rendering behavior.

PRs #29-#31 therefore remain outside P26. They may be reviewed later on their
own acceptance criteria, but they are not prerequisites for the far-zoom work.

## Verified current-state findings

1. `GX_XR_UpdateTerrainCoverage` grows visible terrain coverage up to the
   513x513 clamp as board coverage increases. Terrain uses static VBO tiles,
   but each visible tile still incurs draw and state work.
2. XR visibility uses the normal frustum test plus a board-volume sphere test.
   There is no projected-size budget for distant cosmetic objects, and engine
   LOD preparation remains disabled.
3. World eye resolution is fixed by quality tier. There is no dynamic governor.
4. Particle capacity is not coverage-aware.
5. Existing diagnostics already report FPS and draws/frame split into models,
   particles, UI, terrain, shadows, skin and other. No speculative renderer
   patch is needed before the first measurement.

## Gate 0: reproducible device baseline

Before creating another P26 implementation branch, capture the same busy save
on Quest 3 using the PR #27 test build:

- Balanced resolution, Multiview, Light shadows and automatic/off world copy;
- 60 seconds at the normal useful tabletop zoom;
- 60 seconds at maximum zoom-out without changing map or camera direction;
- record FPS/frame time and the complete `perf-draws/frame by source` split;
- note whether the slowdown appears immediately or only after sustained play.

The comparison, device build hash and settings belong in the first
implementation PR. A subjective improvement alone is not an acceptance result.

## Decided implementation order

### P26-1: safe far-zoom cosmetic visibility budget

Create this branch only if `models=` rises materially at far zoom.

Implement the threshold where `W3DScene` still has access to `Drawable` and
kind-of data, not inside the sphere-only `GX_XR_CullSphere` hook. Only a strict
whitelist of cosmetic objects may be skipped at tiny projected size, initially
`KINDOF_PROP` and `KINDOF_SHRUBBERY`. Never cull selectable objects, infantry,
vehicles, structures, projectiles, force-visible objects, command feedback,
health bars or tactical markers. Rendering may be skipped; simulation, picking,
shroud and lockstep state must remain untouched.

Ship the first device build behind a session-only A/B toggle, default Off. Use
hysteresis between cull and restore thresholds to prevent visible popping.

Acceptance:

- measurable reduction in `models=` and frame time at maximum zoom-out;
- no change to near-zoom counts;
- no missing units, buildings, projectiles, selection feedback or commands;
- Skirmish and Campaign worn-headset sweeps pass at every zoom step.

### P26-2: terrain draw/state batching

Create this branch only if `terrain=` is the dominant far-zoom increase after
P26-1, or if Gate 0 already shows models are not the problem.

Batch adjacent static terrain tiles that share render state without changing
coverage, texture selection, fog, shroud or geometry. This is preferred over
reducing terrain detail because it targets CPU/draw overhead without a visual
quality reduction.

Acceptance:

- materially fewer `terrain=` draws and lower far-zoom frame time;
- per-eye image comparison shows no terrain, shroud or border change;
- map edges, large campaign maps and all tabletop zoom steps remain correct.

### P26-3: Quest GPU headroom controls (amend PR #28)

Do not create a duplicate foveation PR. Amend #28 so refresh rate and fixed
foveation can be enabled and measured independently. Keep the current refresh
behavior and foveation Off by default in the first test build. Apply foveation
only to the world eye swapchains; full-resolution UI and panels must remain
unaffected. Test Medium before HIGH.

This stage is useful only when changing eye resolution or foveation improves
frame time while draw counts remain similar. A 72 Hz request can improve frame
pacing and thermal headroom, but must not be reported as an FPS optimization by
itself.

Acceptance:

- extension and active-state logs verified on Quest 3;
- separate Off/Medium/High and runtime-default/72 Hz comparisons;
- no UI blur, board-edge shimmer, comfort regression or lifecycle failure;
- sustained Skirmish and Campaign session, including Ground View.

## Deferred unless measurements justify them

- Dynamic resolution: only for a demonstrated GPU/fill-rate limit. It requires
  hysteresis, an Auto/Off option and a conservative floor; it is not the first
  response to CPU or draw-call load.
- Zoom-scaled particle caps: only if `sorted(particles)=` dominates. This trades
  battle presentation for performance and is not a default optimization.
- Shadow update throttling: only if `shadows=` dominates and a visual cadence
  can pass the headset gate.
- General threading and texture streaming: track under their own hitch/loading
  milestone, not P26.

## Decision rule

The next implementation branch is selected from Gate 0, not preference:

1. `models=` increase -> P26-1.
2. `terrain=` increase -> P26-2.
3. Stable draw counts but resolution/foveation changes help -> amend and test
   PR #28 as P26-3.
4. Particle or shadow fields dominate -> open only the matching deferred slice.

This keeps every optimization attributable, reversible and testable, while
protecting the currently working controls, visual readability and simulation.
