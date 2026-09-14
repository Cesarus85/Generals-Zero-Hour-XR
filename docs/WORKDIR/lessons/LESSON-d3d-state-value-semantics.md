# LESSON: D3D8 state values a translation layer must not second-guess

**Date**: 2026-09-05
**Phase**: Android port, native GLES backend (`Core/Libraries/Source/d3d8gles/`)
**Symptoms**: greyed-out command-bar buttons vanished / went too bright / flickered
between all-colour and all-grey; aircraft shadows rendered as dark vertical slabs
**Fix commits**: `69eb29af7`, `e64ffe126` (fixed-function stages), `06679a211` (stencil)
**Sibling lesson**: `LESSON-d3d-vs-gl-rasterization-conventions.md` — that one is about
*coordinate* conventions; this one is about *state values*.

## TL;DR for the next agent

Three separate GLES-only visual bugs in this port had the same shape: the translation
layer received a legitimate D3D8 state value and **decided it could not have meant it**.

| What D3D sent | What the layer assumed | Result |
|---|---|---|
| `D3DTOP_MULTIPLYADD` | "unknown op → approximate as MODULATE" | disabled buttons rendered black |
| a stage with no texture bound | "no texture → disable the whole stage" | the greyscale pass never ran |
| `D3DRS_STENCILMASK = 0` | "0 → nobody set it → use 0xFFFFFFFF" | shadow volumes never cancelled |
| `D3DRS_STENCILREF = 0x80808080` | `(GLint)` — fine as a signed int | clamped to 0 by GL |

**Rules that fall out of this:**

1. **Zero is a value, not "unset".** `D3DRS_STENCILMASK = 0` means *compare no bits*;
   `D3DRS_STENCILWRITEMASK = 0` means *write no bits*; `D3DRS_COLORWRITEENABLE = 0` means
   *write no channels*. If your device's state array starts zeroed, **seed D3D8's documented
   defaults** instead of pattern-matching zeros at the point of use. Every `x ? x : DEFAULT`
   in a state translator is a latent bug of this kind.
2. **D3D state is unsigned.** `DWORD` → `GLint` is a silent trap. `0x80808080` becomes
   `-2139062144`; GLES 3.0 §4.1.4 clamps `glStencilFunc`'s ref to `[0, 2^s−1]`, so it lands
   on **0** — the opposite end of the range from what was asked for.
3. **Never approximate an unimplemented op silently.** Emit a one-time warning naming the op.
   A missing op rendered as a plausible-looking modulate survived for months.

## Case 1 — the command-bar buttons

`Render2DClass::Render()`'s `IsGrayScale` path (used for buildings you cannot currently
afford or build) desaturates in two texture stages:

- stage 0: `D3DTOP_MULTIPLYADD` biases the icon up by `TFACTOR.a * TFACTOR.a` (+0.25)
- stage 1: `D3DTOP_DOTPRODUCT3` against TFACTOR's luminance weights, which subtracts 0.5 first

**Two independent defects, found one after the other:**

`D3DTOP_MULTIPLYADD` was not implemented and fell through `combinerOp()`'s silent `default:`,
emitting a plain modulate — `tex * 0.5` instead of `tex + 0.25`. That caps stage 0 at or below
the 0.5 the next stage subtracts, so the dot product went negative for every texel and clamped
to 0: **every disabled button rendered solid black**, which against the dark command bar reads
as the button having disappeared. Moving the camera changes what is buildable, which is why the
buttons came and went with camera movement.

Implementing it exposed the second defect. **D3D8 still runs a stage whose op is not
`D3DTOP_DISABLE` when no texture is bound there, as long as the op's arguments do not source
`D3DTA_TEXTURE`.** This backend disabled the whole stage on "no texture". The greyscale stage 1
uses `COLORARG1 = D3DTA_CURRENT` and `COLORARG2 = D3DTA_TFACTOR` — no texture anywhere — and
the 2D path only ever binds stage 0, so the desaturation **never ran** and the icon was left as
the bare stage-0 result: too bright to tell apart. Worse, it was intermittent: whenever a
previous 3D pass happened to leave a texture bound on stage 1, the stage *did* run and every
icon went greyscale at once. That is the reported all-colour/all-grey flicker, and its
frame-global character is the tell — a per-button bug cannot flip every button at once.

**Fix**: implement `MULTIPLYADD` and `LERP` plus the `D3DTSS_COLORARG0`/`ALPHAARG0` third
argument they read (never read at all before); collapse a textureless stage **per channel**,
and only for the channel that actually reads the missing texture
(`stageChannelReadsTexture` / `collapseStageWithoutTexture`, applied identically in
`computeProgramKey` and `getProgram` so the cache key and the generated shader cannot describe
different pipelines). `D3DTOP_BLENDTEXTUREALPHA` counts as reading the texture even though no
argument names it.

## Case 2 — the aircraft shadow slabs

`W3DVolumetricShadow`'s fill passes set `STENCILFUNC = GREATEREQUAL`, `STENCILREF = 0x80808080`,
`STENCILMASK = TheW3DShadowManager->getStencilShadowMask()`, which is **0** when the
player-colour occluder feature is off. In D3D the comparison is `(ref & mask) CMP (buf & mask)`,
so with `mask == 0` it is `0 >= 0` — **unconditionally true by design**. The ref only matters in
the alternative occluder mode.

The GL layer turned that into `glStencilFunc(GL_GEQUAL, 0, 0xFF)` — *pass only where the stencil
is still 0* — via the two defects in the table above. Then:

- **INCR pass**: buffer is 0 after the clear → passes → the near half raises the whole volume
  silhouette to 1.
- **DECR pass**: those pixels now hold 1 → `0 >= 1` **fails the stencil test** → `STENCILFAIL`
  is `KEEP` → **nothing is decremented**.
- The far half never cancels the near half. The entire silhouette stays at 1 and the darkening
  quad (`LESSEQUAL`, ref 1) paints all of it.

DXVK is unaffected because D3D applies the mask to both operands and the ref is an unsigned
`DWORD` — the signed-clamp-to-zero is unique to the GL path.

**Fix**: seed D3D8's stencil defaults in the device (`STENCILFUNC=ALWAYS`, both masks
`0xFFFFFFFF`, the three ops `KEEP`) so a zero really means the game asked for zero, then pass
ref and both masks through verbatim, masking the ref into the stencil range **unsigned**.

## Case 3 — geometry invisible below High detail

**Symptom as reported**: "half the textures are missing" after starting the game on Medium or
Low; switching to High fixes it; a *live* switch down to Low is fine.

It was neither textures nor missing. The geometry was drawn every frame, with entirely correct
texture, colour, blend and stage state — and then overpainted, because **the depth test was off
for the whole scene**.

`D3DRS_ZENABLE` defaults to `D3DZB_TRUE` in D3D8 and the engine relies on that: `ShaderClass::Apply`
drives `D3DRS_ZFUNC` and `D3DRS_ZWRITEENABLE` and **never touches `ZENABLE`**, because under a real
runtime the depth test is already on. The device's render-state array started zeroed, so `ZENABLE`
read back as `D3DZB_FALSE`, the pipeline called `glDisable(GL_DEPTH_TEST)`, everything drew in
submission order — and the water, drawn last from `Render_And_Clear_Static_Sort_Lists`, painted
over the terrain in front of it.

The detail-level dependency had nothing to do with textures or with detail as such: `W3DVolumetricShadow`'s
stencil passes do `SetRenderState(D3DRS_ZENABLE, TRUE)`, and shadow volumes are on only at High and
above. That one incidental write repaired the state for the rest of the session. Hence: broken on a
fresh Low/Medium start, fine after a live switch *down* (already repaired), permanently fixed by
visiting High once.

The dump caught it in a single log by watching the value change with the setting:

```
start on Low     -> [gxstate] TERRAIN zEnable=0 ...   (broken)
switch to High   -> [gxstate] TERRAIN zEnable=1 ...   (correct)
back to Low      -> [gxstate] TERRAIN zEnable=1 ...   (stays correct)
```

Same fix as the other two cases: seed D3D8's documented defaults in the device
(`ZENABLE`, `ZWRITEENABLE`, `ZFUNC`, `CULLMODE`, `SRCBLEND`, `DESTBLEND`, `ALPHAFUNC`,
`TEXTUREFACTOR`). Note the consumer side had *guarded* every neighbouring zero
(`zfunc ? zfunc : D3DCMP_LESSEQUAL`, `sb ? sb : D3DBLEND_ONE`) and left `ZENABLE` unguarded —
the point-of-use patching that rule 1 warns against, applied inconsistently, which is how it hid.

## The debugging methodology — the part worth reusing

This hunt cost roughly a dozen device round-trips. Most of that was avoidable.

### Instruments have blind spots — state yours before trusting a null result

Two measurements looked conclusive and were not:

- **A draw-call counter showed the two stencil passes perfectly balanced** (`incr=8865
  decr=8865`, every window). But it counts draw *calls*, and a call whose fragments are all
  killed — by culling, or by a stencil test — still counts. It could never have detected the
  actual bug.
- **An occlusion query showed the DECR pass rasterizing samples.** True, and I concluded
  "therefore the stencil is being decremented". That does not follow: fringe pixels and
  depth-failed regions were still at 0 and did survive the test. The instrument answered a
  narrower question than the one I asked of it.

**Before believing a negative result, write down what the instrument physically cannot see.**

### Negative results carry information — mine them

Inverting the cull face changed *nothing*. I recorded that as "hypothesis dead" and moved on.
It was actually the strongest constraint available: a symptom that is bit-for-bit identical
under either culling means **whichever half draws first locks the other out**, which points
straight at a per-fragment test rather than at geometry. The eventual root cause was the only
hypothesis that predicted it.

### An A/B that short-circuits a function must account for the function's tail

The A/B that skipped `renderStencilShadows()` returned early from the top — and so also skipped
the `SetRenderState(D3DRS_STENCILENABLE, FALSE)` at the **end** of that function. The stencil
test leaked into every later draw and the whole frame fell apart, producing dramatic symptoms
that had nothing to do with the bug. **Read what a function restores before skipping its head.**

### Bring in an independent reader when your own hypothesis list is exhausted

After the cull flip failed, every idea I had left was a variation on ones already disproven.
An independent audit of the same code — given the measured facts and told not to re-derive
them — found the root cause in one pass. The value was not extra compute; it was **not having
my accumulated assumptions**.

### An instrument chosen to fit your theory will confirm your theory

Twice in the same hunt a counter was added, went up, and was read as the fingerprint of the bug:

- **Stage-0 texture collapses.** Believed to mark draws rendering black. It turned out to be
  *inversely* correlated: 16936 per window in the CORRECT High run, 180 in the BROKEN Low run.
  It counts untextured draws, i.e. scene complexity. The earlier "healthy 6-549 vs broken 14490"
  reading that launched the theory was menu-versus-in-game, not Low-versus-High in the same scene.
- **The state dump that finally found it** did not report depth on its first version, because its
  fields were chosen to match the theory already held.

**Pick the fields, and the control, before you know what you expect to see.** A counter compared
against a different scene is not a control.

### A change whose whole effect is a value must print that value

Two consecutive builds differed *only* by seeding a render-state default, and neither emitted a
line naming it. A device log came back still showing the old value, and it was impossible to tell
"the fix is not in the build you tested" from "the fix is there and something overwrites it".
That ambiguity cost a full test round — and the answer was the boring one: the tester had the
older APK.

Two things make this impossible to repeat:
- every build already stamps `CI build number:` (from `date +%s`); **read it off the tester's log
  and compare it against the build you meant to ship, before interpreting anything else**;
- print the values themselves. (And place that print *after* every assignment it reports —
  the first version of this very line sat mid-block and reported seven of its eight fields as
  zeros, recreating the ambiguity it existed to remove.)

### What actually worked, throughout

Every disproven hypothesis was killed by a *measurement*, not by an argument, and each one
narrowed the space permanently: framebuffer bit depths, the depth-stencil format, the clear
flags, a GL read-back of the colour mask, the translated state sequence, the engine's own
`SV_DEBUG` volume visualization. By the time the audit ran, the facts it was handed were solid
enough that it could reason to the answer without a device.

The one rule that was broken and cost the most: **twice I dismissed a hypothesis by inference
instead of measuring it** (the stencil clear, "correct for models therefore correct for
volumes"). Both had to be revisited. Inference is for choosing the next measurement, not for
replacing it.

## Related

- `docs/WORKDIR/lessons/LESSON-d3d-vs-gl-rasterization-conventions.md` — the coordinate-
  convention half of this bug family, plus the "measure the output, not the inputs" rule
- `docs/WORKDIR/lessons/LESSON-gles-dynamic-buffer-stalls.md` — the performance counterpart
  (lock/usage flags as a synchronization contract)
- `docs/port/PORTING_PATTERNS.md` §4 — portability bug taxonomy
