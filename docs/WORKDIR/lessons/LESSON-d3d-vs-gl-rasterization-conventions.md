# LESSON: D3D↔GL Convention Mismatches — the "everything measures correct but looks wrong" bug class

**Date**: 2026-09-05
**Phase**: Android port, native GLES backend (`Core/Libraries/Source/d3d8gles/`)
**Discovered by**: ~15-hypothesis hunt for a thin see-through strip on the right/bottom screen edges
**Fix commit**: `12c7cbab9` (`render2d.cpp`, `Render2DClass::Update_Bias`)

## TL;DR for the next agent

This codebase keeps its **Direct3D 8 API surface** and swaps the implementation underneath
(DXVK→Vulkan, or our native GLES backend). D3D and OpenGL disagree on several **rasterization
and storage conventions**. Engine code written against D3D contains *deliberate corrections*
for D3D's conventions. On the GLES backend those corrections are **not corrections — they are
bugs**, because GL's convention was already right.

**Before adding any new "alignment" or "offset" logic, check whether the engine already applies
a D3D-era correction upstream of you.** Grep for: `0.5`, `half-pixel`, `bias`, `flip`, `invert`,
`RHW`, `texel`, near any 2D/blit/screen-space code.

## The concrete bug

**Symptom** (reported over many sessions, on two different devices/GPU vendors):
a thin, *see-through* strip along the **right and bottom** screen edges — never left or top.
Through it, the live 3D scene showed behind the UI and behind the fullscreen startup video.
Present at **any** resolution. Present on plain GLES **and** ANGLE. **Never** on Vulkan/DXVK.

**Root cause**: `Render2DClass::Update_Bias()`
(`GeneralsMD/Code/Libraries/Source/WWVegas/WW3D2/render2d.cpp`) shifts the *entire 2D layer* by
**-0.5 pixel** in X and Y, enabled globally by `WW3D::Set_Screen_UV_Bias(TRUE)` in
`W3DDisplay::init()` with the comment *"this makes text look good"*.

That -0.5 is a **D3D-specific texel-to-pixel alignment correction**. D3D's screen-space
convention puts a pixel's sample center half a pixel from where a `0..W` screen-aligned quad's
edge lands, so 2D content needs -0.5 for texels to line up with pixels. **OpenGL's convention
already has pixel centers at the +0.5 offsets**, so on the native GLES backend that
"correction" is a real, uncompensated half-pixel shift of all 2D content up and to the left.

Worked through exactly (`W` = coordinate range width):

```
CoordinateScale.X  = 2/W ,  CoordinateOffset.X = -1
bais_add.X         = -0.5 / (W * 0.5) = -1/W

right edge:  W*(2/W) - 1 - 1/W  =  1 - 1/W   <-- 0.5 px SHORT of +1
left  edge:       0   - 1 - 1/W             <-- 0.5 px PAST -1, clipped away
```

…and symmetrically for Y (`bais_add.Y = -0.5/(H*-0.5) = +1/H`, so the bottom edge lands at
`-1 + 1/H` while the top overshoots and is clipped).

That leaves a **half-covered edge pixel on the right and bottom only**. Alpha-blended, a
50%-covered pixel reads as a thin *see-through* strip showing whatever the 3D pass already
drew underneath — which is exactly the reported artifact, and explains every property of it:

| Reported property | Explained by |
|---|---|
| Transparent/empty, not wrong-colored | 50% fragment coverage + alpha blend |
| Right and bottom only, never left/top | Shift is up-left; the left/top overshoot is clipped |
| Any resolution, doesn't scale | The shift is a constant **in pixels** |
| UI *and* video, never 3D | Both go through `Convert_Vert`; 3D never touches `Render2DClass` |
| GLES/ANGLE only, never Vulkan | DXVK reproduces D3D8's rasterization offset faithfully |

**Fix**: skip the bias on the native GLES backend only
(`d3d8gles_ShouldUseVulkanBackend()` gate). The Vulkan path keeps the original behavior;
non-Android builds are untouched.

## The general class — other instances already in this codebase

This was not the first convention mismatch here, and it will not be the last. Known instances,
all in the same family, all fixed separately:

| Convention | D3D | GL | Where it bit us |
|---|---|---|---|
| **Pixel-center / texel alignment** | needs -0.5 for screen quads | already aligned | *this lesson* — `Update_Bias()` |
| **Viewport Y origin** | `vp.Y` from the **top** | `glViewport` y from the **bottom** | `applyFixedState()` — needs `RTH - vp.Y - vp.Height`; a fullscreen viewport hides the bug (both reduce to 0), so it only showed on a *partial* in-game viewport |
| **Clip-space Y** | `clip.y=+1` is top | `NDC.y=+1` is also top | A blanket `-cpos.y` negation flipped the *whole frame*; correct answer was **no** extra negation |
| **Render-target texture storage origin** | top-origin | bottom-origin | `Pillarbox_End()` blit — a render target (unlike a *loaded* texture, which the loader pre-flips) needs `v` swapped, GLES-only |
| **Default texture address mode** | engine leaves `WRAP` | maps to `GL_REPEAT` | Blit sampled at `u/v≈1` wrapped to the opposite edge → one-sided streak |

**Pattern**: every one of these is invisible in the common case and only appears in a specific
configuration (partial viewport, render-to-texture, non-native resolution, bilinear filtering).
Assume more of them exist.

## The debugging methodology — this is the part worth reusing

This bug cost ~15 disproven hypotheses. The reason is instructive.

### What went wrong

Every diagnostic measured the **inputs** to the transform:
viewport size, coordinate range, `Get_Device_Resolution()` vs `Get_Render_Target_Resolution()`,
EGL surface size, an independent `GetWindowSize()`, `GL_VIEWPORT` at `present()` time and at
every `glViewport` call site. **All of them were correct and mutually consistent** — repeatedly,
on real-device logs, including at an exact 100% resolution match.

The bug lived in a term added **downstream of everything being measured**
(`BiasedCoordinateOffset`, applied inside `Convert_Vert`), so no amount of input-checking could
ever have found it.

### The rules that follow

1. **Measure the OUTPUT, not the inputs.** When every input checks out but the picture is
   wrong, instrument the final value handed to the GPU. "All the inputs are right" is evidence
   that the bug is *downstream of the inputs* — that is information, not a dead end.
2. **Read the whole transform chain for extra terms.** `out = in * scale + offset` — we verified
   `scale` (via width/height) for weeks and never once looked at how `offset` was built.
   Enumerate every term; verify each one *separately*.
3. **Symptom geometry is a fingerprint — do the arithmetic on it.**
   "Right and bottom only, never left or top, doesn't scale with resolution" is not vague; it
   uniquely implies *a constant translation in pixels, up and to the left*. That should have
   been the first thing computed, and it points straight at an additive pixel-space term.
4. **A wrong fix with a clean numeric signature is a measurement.** The breakthrough came from a
   *failed* fix that shrank the UI to exactly **70%** in the top-left corner. `1756/2510 = 0.6996`
   — the chosen-resolution / real-backbuffer ratio, exactly. That precise ratio proved
   the content extent and the NDC denominator were being sourced from different places, which
   gave the arithmetic that then exposed the additive term. **When an experiment fails, extract
   its exact number before reverting it.**
5. **"Works on Vulkan, broken on GLES" is a strong prior, not a mystery.** Both backends share
   100% of this project's C++ above the API boundary. So the difference is almost always either
   (a) a convention the D3D-faithful translator preserves and the native backend does not, or
   (b) our own backend-specific code. Enumerate the conventions (table above) *first*.
6. **Prefer one clean A/B per build.** Bundling two changes cost a full test cycle here when a
   regression and a candidate fix shipped together and could not be told apart.
7. **Watch for self-referential checks.** `Pillarbox_Setup()` compared
   `_PresentParameters.BackBufferWidth` against the value it was itself derived from — a check
   that can never fail, and thus proves nothing. If a consistency check has never once fired,
   verify it *can*.

### Cheap instrumentation that would have found this immediately

Track the extreme output value and compare it against the theoretical bound:

```cpp
// in Convert_Vert: does any vertex EVER actually reach |1.0|?
static float maxAbsX = 0.f, maxAbsY = 0.f;
if (fabsf(out.X) > maxAbsX) { maxAbsX = fabsf(out.X); /* log */ }
```

`maxAbsX = 0.9996` instead of `1.000000` is the entire bug, visible in one line.
(Remove such per-vertex probes once done — this one sat in a hot path.)

## Guardrails when touching 2D/screen-space code here

- `Render2DClass` is the **shared** path for every UI widget *and* the video overlay
  (`W3DDisplay::drawVideoBuffer` → `Add_Quad` → `Render2DClass::Render`). A change there hits
  both. 3D goes through `CameraClass::Apply()` instead and is unaffected.
- Two *different* size sources feed 2D and must stay consistent with each other:
  - the **pixel extent** of content — `TheDisplay->getWidth()/getHeight()`
    (used by `drawScaledVideoBuffer`, widget layout);
  - the **pixel→NDC denominator** — `Render2DClass::CoordinateScale`, set via
    `Set_Coordinate_Range()`.
  Changing only one produces a proportional shrink/overflow anchored at the top-left.
  (That is exactly the failed fix in rule 4 above.)
- While the pillarbox path is active, 3D **and** UI both render into the small offscreen
  texture and are stretched together by `Pillarbox_End()` at `End_Scene()` time. Do **not**
  size 2D to the real backbuffer while that target is bound — content gets clipped to the
  texture, then stretched.
- `Set_DX8_Render_State()` writes straight into `DX8Wrapper`'s shared state array. Calling it
  from 2D code, bypassing `Set_Shader()`, desynced the "shader unchanged ⇒ state unchanged"
  cache and broke **3D model rendering** on the next draw. Go through the shader path.

## Related

- `docs/port/PORTING_PATTERNS.md` §4 — portability bug taxonomy (this class is listed there)
- `Core/Libraries/Source/WWVegas/WW3D2/dx8wrapper.cpp` — `Pillarbox_End()` carries long
  comments documenting the Y-flip and address-mode instances of this same class
