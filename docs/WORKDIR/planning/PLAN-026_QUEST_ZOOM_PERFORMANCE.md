# PLAN-026: Quest far-zoom performance

**Status:** P26-2 terrain batching implemented, visually accepted on Quest 3
and merged to `main` in PR #34 (`3e895fa`). The measured maximum-zoom frame
time improved; broader map/Campaign regression remains a release gate.

**Current base:** `main` includes PR #27's XR board-mesh churn reduction, PR
#33's Quest draw-breakdown logging and PR #34's accepted P26-2 terrain
batching. The public 1.2.25 release APK is unchanged; 10228 is the installed
measurement build rather than a published release.

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
5. Existing diagnostics already calculate FPS and draws/frame split into
   models, particles, UI, terrain, shadows, skin and other. The release APK
   routes the XR timing report to logcat, but the draw split was emitted only
   through native stderr and was not observable over ADB. The temporary
   `codex/p26-draw-breakdown-logcat` branch mirrors that existing two-second
   split to the `gx-perf-draws` log tag without changing the counters.

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

### Gate 0 result, 2026-09-20

Quest 3 test build `10226 / 1.2.26-xr-board-mesh-test`, Balanced resolution,
Multiview, Light shadows and automatic world copy was measured for 60 seconds
in the same live Campaign scene at each zoom level:

| View | Coverage | Samples | Engine CPU | Frame | Derived rate | Peak frame |
|---|---:|---:|---:|---:|---:|---:|
| Normal tabletop | 1.5508 | 30 | 20.73 ms | 21.71 ms | 46.1 FPS | 45.52 ms |
| Maximum zoom-out | 4.5000 | 27 | 40.96 ms | 42.00 ms | 23.8 FPS | 183.85 ms |

Eye CPU stayed near 0.2 ms, XR wait near 0.05 ms and the eye extent stayed
1536x1609. The immediate 93% frame-time increase is therefore in engine work,
not XR composition or an increased render resolution.

The follow-up `10227 / 1.2.27-p26-drawlog-test` capture used the same scene and
settings. Steady two-second category averages were:

| View | Samples | Models | Sorted | UI | Terrain | Shadows | Other | Total |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| Normal tabletop | 15 | 275.5 | 54.8 | 22.3 | 56.0 | 4.4 | 5.0 | 418.0 |
| Maximum zoom-out | 14 | 404.3 | 91.3 | 22.2 | 311.3 | 8.1 | 5.0 | 842.1 |

Terrain is the dominant absolute increase: +255.3 draws/frame, versus +128.8
models and +36.5 sorted draws. Gate 0 therefore selects P26-2 first. P26-1
remains a measured follow-up because model work also rises, but broad cosmetic
culling must not precede the terrain fix. Dynamic resolution and PR #28 are not
the first P26 action.

## Decided implementation order

### P26-1: safe far-zoom cosmetic visibility budget (measured follow-up)

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

### P26-2: terrain draw/state batching (selected first)

Gate 0 selected this branch first: terrain rises from 56.0 to 311.3 draws/frame
and accounts for the largest part of the measured far-zoom increase.

Batch adjacent static terrain tiles that share render state without changing
coverage, texture selection, fog, shroud or geometry. This is preferred over
reducing terrain detail because it targets CPU/draw overhead without a visual
quality reduction.

Acceptance:

- materially fewer `terrain=` draws and lower far-zoom frame time;
- per-eye image comparison shows no terrain, shroud or border change;
- map edges, large campaign maps and all tabletop zoom steps remain correct.

#### P26-2 implementation and Quest result, 2026-09-20

Android now stores up to ten unchanged 32x32 terrain patches in one static
vertex buffer and addresses them through one expanded 16-bit index buffer.
Ten patches remain below the legacy limits (40,960 vertices and 61,440
indices). Updates and dynamic-light writes retain per-patch offsets; coverage,
geometry, textures, fog, shroud and non-Android rendering remain unchanged.

Release-signed test APK `10228 / 1.2.28-p26-terrain-batch-test`, SHA-256
`78ff8ed5abfc3b1f5b2ac608961dad4289f95fa715c289bf43e3f02a1791fbeb`,
was update-installed on Quest 3. Native ARM64 packaging passed and the
maintainer confirmed the image was correct across the tested zoom range.

The immediate normal/max draw capture on that build measured:

| View | Samples | Models | Sorted | UI | Terrain | Shadows | Other | Total |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| Normal tabletop | 15 | 212.0 | 60.6 | 21.2 | 24.0 | 3.6 | 5.0 | 326.4 |
| Maximum zoom-out | 12 | 387.0 | 55.5 | 21.6 | 45.0 | 8.1 | 5.0 | 522.2 |

Against the diagnostic maximum-zoom capture, terrain fell from 311.3 to 45.0
draws/frame (-85.5%) and total draws fell from 842.1 to 522.2 (-38.0%). A
separate 26-sample maximum-coverage timing capture during continued Campaign
play measured 33.17 ms engine CPU and 34.30 ms/frame, or 29.2 derived FPS.
Compared with the 10226 baseline at the same 4.5000 coverage, that is 19.0%
less engine CPU, 18.3% lower frame time and 22.7% higher derived FPS (23.8 to
29.2). Terrain remained at 60 draws/frame in that later scene; model and sorted
effect variation is now the main remaining source of frame-time swings.

This passes the P26-2 device/visual gate without a detail reduction. It does
not prove every map edge or long-session Campaign path; those stay part of
release regression testing. P26-1 is the next measured candidate, but must
remain a conservative cosmetic-only A/B slice.

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
