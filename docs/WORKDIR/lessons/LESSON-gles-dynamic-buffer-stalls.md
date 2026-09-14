# LESSON: The GLES UI freeze — D3D8 dynamic-buffer lock semantics are a *contract*, not a hint

**Date**: 2026-09-05
**Phase**: Android port, native GLES backend (`Core/Libraries/Source/d3d8gles/`)
**Symptom**: 12–17 fps and hard stutter on OpenGL ES / ANGLE, *worse the more UI was on screen*
**Result**: 55–60 fps; UI submit cost 49–62 ms/frame → **0.30–0.45 ms/frame**
**Fix commits**: `b6b091433` → `0419a6d20` → `610200b69` → `d2f88ab3b`
(diagnostics that found it: `3651ee7da`, `778e93ed4`; unrelated win: `866ef372a`)

## TL;DR for the next agent

D3D8's `Lock(offset, size, D3DLOCK_DISCARD | D3DLOCK_NOOVERWRITE)` is a **synchronization
contract between the app and the driver**, not decoration. This backend ignored all three
parts of it — the `offset`, the `size`, and the flags — and re-uploaded the whole buffer with
`glBufferData`, then with `glBufferSubData`. Both make the GL driver **wait for the GPU**.
The engine calls this path once per 2D batch, so the wait multiplied by the number of UI
draw calls and ate the entire frame.

**If you are writing a D3D→GL/Vulkan/Metal translation layer: translate the lock flags first,
before optimizing anything else.** They are the whole reason the D3D-era engine is fast.

| D3D8 lock flag | What it *promises* | Correct GL translation |
|---|---|---|
| `D3DLOCK_DISCARD` | "I am rewriting from scratch; the old contents are dead" | respecify storage: `glBufferData(target, size, data, GL_DYNAMIC_DRAW)` |
| `D3DLOCK_NOOVERWRITE` | "I am appending; I will not touch bytes the GPU may still read" | `glMapBufferRange(..., GL_MAP_WRITE_BIT \| GL_MAP_UNSYNCHRONIZED_BIT)` |
| neither | "assume the worst" | `glBufferSubData` (implicit sync — correct, and slow, *by design*) |

## The bug, in three layers

The engine keeps one dynamic vertex buffer and one dynamic index buffer and uses them as a
**ring** (`Core/Libraries/Source/WWVegas/WW3D2/dx8vertexbuffer.cpp`): `DEFAULT_VB_SIZE = 5000`
vertices of `dynamic_fvf_type` (`XYZ|NORMAL|TEX2|DIFFUSE`, 44 B/vertex ≈ **220 KB**). It locks
at ring offset 0 with `DISCARD`, then appends with `NOOVERWRITE` until the ring wraps.

### Layer 1 — `Lock()` threw away `offset` and `size`

`WebGLVertexBuffer::Lock` returned `m_bits.data() + offset` and recorded *nothing*. `Unlock()`
could only say "dirty", so `ensureVBUploaded()` re-uploaded **all 220 KB** for a batch that had
written a couple of hundred bytes. Dozens of times a frame.

Fix: record the locked range and the discard flag on `Lock`, merge it into a dirty range on
`Unlock` (`GLBufferState::markRange`, `gles_pipeline.h`). Note D3D8's rule that `size == 0`
means *"from offset to the end of the buffer"* — getting that wrong silently uploads nothing.

### Layer 2 — partial upload was still a full GPU stall

Uploading only the dirty range with `glBufferSubData` made it **worse-looking, not better**:
the cost stayed flat at **~1 ms per `Render2DClass::Render()` call** and tracked the UI draw
count 1:1 (50.6 draws → 49.3 ms; 65.2 draws → 62.5 ms), *independent of how many bytes were
written*.

**Cost that scales with call count and not with data volume is the signature of waiting, not
working.** `glBufferSubData` on a buffer the GPU may still be reading forces the driver to
either block or shadow-copy; that is exactly what `NOOVERWRITE` exists to say is unnecessary.

Fix: `glMapBufferRange` with `GL_MAP_UNSYNCHRONIZED_BIT` — the literal translation of the
promise the engine is already making.

### Layer 3 — the "clever" `DISCARD` optimization caused visual corruption

`DISCARD` was first implemented as **orphaning**: `glBufferData(target, size, nullptr, usage)`
to respecify storage without waiting, then push only the fresh range. That is the textbook
trick, and here it is **wrong**: orphaning leaves every byte *outside* the uploaded range
undefined, and this engine does not treat the ring's older contents as dead —
`SortingRendererClass` draws sorted translucent geometry that still references ranges written
earlier in the same frame. Those bytes survive in the CPU shadow (`m_bits`) but not in the
newly orphaned GL storage. Confirmed on device as **stray grids and long diagonal lines**
across the scene.

Fix (`d2f88ab3b`): on `DISCARD`, call `glBufferData` **with** the data. It respecifies storage
too — so it still does not wait — it just also refills it. This only runs when the ring wraps,
so the ordinary `NOOVERWRITE` appends stay cheap.

> Generalization: **"the old contents are dead" is a claim about the API, not about this
> engine.** Verify what the engine actually re-reads before optimizing on that assumption.

## Measured, on a real Mali device (Redmi Note 8 Pro)

| Metric | Before | After |
|---|---|---|
| fps | 12–17 | **55–60** |
| `2d-submit` (UI draw submission) | 49–62 ms/frame | **0.30–0.45 ms** |
| `uiWidgets` (`TheInGameUI->DRAW()`) | 40–190 ms/frame | **1.3–5.8 ms** |
| `mainScene` | 30–42 ms/frame | **2–12.7 ms** |

`mainScene` fell too, though nothing in the 3D path changed — both passes share the same two
dynamic buffers, so the stall was never really a "UI problem".

## How it was actually found — the part worth reusing

### 1. The user's own A/B was the best instrument in the room

> *"if I remove all the buttons, photos and so on from the lobby, the game does not lag even
> in the lobby, but as soon as buttons with text and pictures appear it starts to freeze —
> worst in the battle setup menu, where there are many buttons. In game, if I do not select a
> builder the game runs fast, but the moment I select one it slows down several times."*

Selecting a unit adds the command-bar buttons. **The cost tracked on-screen UI element count**,
not scene complexity, resolution, or particle count. That single observation invalidated every
3D-side hypothesis before a line of code was read. Take these reports literally and treat them
as measurements — ask for the toggle, not for a description of the feeling.

### 2. Attribute the cost before optimizing it

The instrumentation that made the rest mechanical (all still in the tree, cheap, logged once
per ~2 s in `present()`):

- **Draw calls by subsystem** — `d3d8gles_SetDrawCategory()` / `[d3d8gles] perf-draws/frame by
  source:` splits `models / sorted / 2d / terrain / shadows / skin / other`. Engine passes tag
  themselves with an RAII restore; untagged work lands in `other`. When `other` was the biggest
  bucket, it got subdivided (`778e93ed4`) rather than guessed at.
- **UI time by phase** — `d3d8gles_AddUiTiming()` / `[d3d8gles] perf-ui ms/frame:` splits
  `text-raster` (glyph rasterization) / `text-texture` (atlas building) / `2d-submit`
  (`Render2DClass::Render`).
- **Frame phases** — the pre-existing `[GX-PERF-DISPLAY]` line: `preRTT`, `waterShadowRTT`,
  `mainScene`, `uiWidgets`, `present`.

These three together said: `present` 0.5–2.5 ms (**not GPU-bound**), draws only ~1200–2500/frame
(**not draw-call-bound**), and ~50 ms sitting inside `2d-submit`. That is a two-line answer to
"where is the frame going" that no amount of code reading produces.

### 3. Cost shape identifies the cause before you know the mechanism

- Scales with **data volume** → you are copying too much.
- Scales with **call count**, flat per call → you are **waiting** (sync/stall).
- Scales with **state changes** → cache/redundancy problem.

Here it was flat ~1 ms/call. That number alone named the bug class.

### 4. The obvious optimization was not the bottleneck

Earlier work on this same branch added a VAO cache, split the uniform cache into independently
cached blocks, and added texture/buffer bind-skip caches. They reached **90 %+ hit rates on real
hardware** and moved the frame rate **almost not at all**. Cache hit rate is a metric about the
cache, not about the frame. Measure the frame.

### 5. One A/B per build, and ship the failed experiment's number

Each step here was one change, one APK, one device log. The stray-geometry regression was
caught in the very next build precisely because nothing else shipped with it — bundling the
`DISCARD` orphaning with the `NOOVERWRITE` mapping would have left two suspects and cost a full
test cycle (this project has paid that cost before; see the sibling lesson).

## Adjacent win found by the same instrumentation

`W3DSmudgeManager` (heat distortion behind explosions) copied the **entire backbuffer** into a
texture every frame — ~23 MB/frame of bus traffic — and on GLES it could not work anyway:
`CopyRects` there is a CPU memcpy between shadow buffers and never touches the real GL
framebuffer, so the effect was invisible *and* expensive. Disabled on the native GLES backend
only (`866ef372a`, `W3DSmudge.cpp`), Vulkan/DXVK untouched.

> Pattern: **a feature that silently no-ops on a backend still costs full price.** When
> auditing a translation layer, grep for full-surface `CopyRects`/readback per frame.

## Checklist when adding a GL resource-update path here

1. Does the D3D call carry **lock/usage flags**? Translate them; do not drop them.
2. Does it carry an **offset/size**? Record it; do not widen it to the whole resource.
3. If you skip a wait (`GL_MAP_UNSYNCHRONIZED_BIT`) or discard contents
   (`glBufferData(..., nullptr)`), state **which engine invariant** makes that safe, in a
   comment, at the call site — and check the invariant actually holds
   (`SortingRendererClass` is the counter-example that broke it).
4. New `gl*` entry point ⇒ **four parallel edits** in `gles_dispatch.cpp` (typedef, global
   pointer, forwarding function, `dlsym` resolution) **plus** bump the
   `"resolved %zu entry points"` count. Missing the last one is invisible until someone reads
   the log; missing any of the first three is a link error.
5. Do not add per-byte or per-vertex diagnostics to the hot path without a call cap — a
   `CopyRects` byte-scan probe here cost ~11.6 M iterations per call.

## Related

- `docs/WORKDIR/lessons/LESSON-d3d-vs-gl-rasterization-conventions.md` — the *correctness*
  counterpart: engine code contains deliberate corrections for D3D conventions that become
  bugs under a native GL backend.
- `docs/port/PORTING_PATTERNS.md` §4 — portability bug taxonomy.
- `Core/Libraries/Source/d3d8gles/src/gles_pipeline.cpp` — `ensureVBUploaded` /
  `ensureIBUploaded` carry the full reasoning inline, next to the code.
