# From-Scratch Porting Patterns — What GeneralsX Teaches About Porting Anything

Companion to `PORTING_PLAYBOOK.md` (which documents *our* iOS port that stood on GeneralsX's shoulders). This document answers the harder question: **what do you do when there is no GeneralsX** — when you must be the one porting a legacy Windows/console codebase to a new platform yourself. It distills the GeneralsX project's own methodology (mined from their dev diaries `docs/DEV_BLOG/`, lessons library `docs/WORKDIR/lessons/`, phase plans `docs/WORKDIR/phases/`, and architecture) plus our session's findings into transferable patterns.

GeneralsX's achievement for calibration: ~500k+ LOC, Windows-only, 32-bit, DirectX 8, VC6-era C++ → native 64-bit ARM64 macOS/Linux, over ~5 months, by a tiny team using AI agents heavily. Everything below is what made that tractable.

---

## 1. Strategy selection: translate, shim, or rewrite (per subsystem, not per project)

The single most important architectural insight: **a port is not one decision, it's one decision per subsystem**, chosen by API surface size and determinism risk:

| Strategy | When | GeneralsX instance | Generalizes to |
|---|---|---|---|
| **Translate** (keep the legacy API surface, swap the implementation underneath) | Huge embedded API surface; behavior must be bit-faithful | DirectX 8 kept, implemented by DXVK→Vulkan(→MoltenVK→Metal) | D3D9/11→DXVK; OpenGL→ANGLE/Zink; Glide→nGlide; Win32 sound→sdl_mixer behind the old API |
| **Shim** (reimplement a thin API on the new platform's primitives) | Many small scattered calls, simple semantics | CompatLib: 28 headers re-implementing QueryPerformanceCounter, CreateFile, threads, sockets on POSIX | Any windows.h usage; registry→config file; GetTickCount→clock_gettime |
| **Swap behind an interface** (the codebase already has a device/manager abstraction — add a new backend) | The original architecture has factory seams | SDL3GameEngine beside Win32GameEngine; OpenALAudioManager beside MilesAudioManager | Most engines have Device/Manager/Driver seams — find them before inventing your own |
| **Stub now, implement later** | Non-gameplay-critical (screenshots, file dialogs, web browser, telemetry) | WWStub lib, `#else { /* TODO */ }` whole-function stubs | Anything you can lose without losing the core loop |
| **Rewrite** | Only when the API has no translator and no thin shim is possible | They avoided it entirely (and the one fork that rewrote D3D8→Metal directly got rejected upstream as unreviewable) | Last resort; budget 10× the estimate |

**Rule of thumb from their experience:** prefer *translate* for the renderer (replay/visual fidelity), *swap* for audio/video (managers are natural seams), *shim* for OS plumbing, *stub* for periphery. Their renderer-translation choice was explicitly justified by **determinism**: replays/network lockstep require frame-exact behavior that a rewrite would silently break.

## 2. The universal porting sequence (their phase model, generalized)

GeneralsX's phases (documented in `docs/WORKDIR/phases/`) generalize to any port:

0. **Analysis & references** — before code: map the architecture; find every *reference implementation* in the ecosystem (they studied fighter19's DXVK integration and jmarshall's OpenAL port via deep repo research before writing anything). Even without a complete prior port, *partial* references almost always exist — an SDL backend someone attempted, a similar engine's port, a translation layer's example app. **Adapt, never copy-paste** (their explicit rule — reference code is proof-of-concept, not production).
1. **Make it compile** on the new toolchain with stubs — compiler/ABI fixes (MSVC-isms, `__int64`, `__max`), whole-function stubbing of Windows code. Goal: linking binary, not running game.
2. **Graphics + windowing + input** first among real subsystems — it's the longest pole and everything else is testable only once you can see the game.
3. **Audio**, then **video/cutscenes** — natural manager seams, lower risk.
4. **Polish & hardening** — paths, lifecycle, packaging, CI, upstream sync.
5. **New-architecture pass** (64-bit/ARM64/new OS) — only after the same-architecture port works, so failures isolate.

Their acceptance gates per phase were behavioral, not build-based: "main menu renders," "skirmish loads," "10-minute stability," and ultimately **replay determinism** (see §5).

## 3. The compat-shim pattern, done right

From their lessons (Sessions 34-35, `LESSON-platform-guards-apple-vs-win32.md`):

- **One shim layer, included first.** Stubs scattered inline in source files conflict with third-party headers that define the same symbols (their QueryPerformanceCounter vs DXVK headers collision). Centralize in compat headers included *before* anything else.
- **Wrap whole functions, not lines.** When a function is >50% Windows API calls, `#ifdef` the *entire function* with a platform alternative — line-by-line guards become unmaintainable ("80 lines × 8 guards = nightmare").
- **Never wrap gameplay logic** in platform conditionals — that's how determinism dies invisibly.
- Keep Win32 *signatures* in the shim so game code doesn't change — the port stays diffable against upstream.
- Expect ~dozens of headers, a few hundred lines total: time, files, threads, sockets, types, text encoding, COM stubs. Budget it as a real subsystem.

## 4. The portability bug taxonomy (check these proactively, they cost them sessions)

Every one of these is a *silent* corruption class they hit and documented; grep for them on day one:

| Class | Bug | Detection/Prevention |
|---|---|---|
| **ABI: integer width** | `long` in on-disk struct = 4 bytes Win32, 8 bytes LP64 → binary file parsing breaks silently (their TGA footer) | Fixed-width types in all serialized structs + `static_assert(sizeof(X)==N)` on every one |
| **ABI: wchar_t width** | 2 bytes Windows, 4 bytes elsewhere → Unicode file fields desync (their map cache) | Hard-code the *disk* format (UTF-16LE), convert at I/O boundaries |
| **Exit semantics** | Windows `ExitProcess` skips global C++ dtors; POSIX `return main()` runs them → exit crashes in pool allocators | `_exit()` after explicit cleanup; crash in `__cxa_finalize*` ⇒ this |
| **Case sensitivity** | Asset paths that worked on NTFS fail on case-sensitive FS (their `LESSON_44_MISSING_TEXTURE_DEBUG`) | Case-normalizing VFS layer or lowercase-on-load |
| **Path separators** | `\\` literals in data-driven paths (their INI dirs use `Data\INI\...`) | Normalize in the file-system layer, not at call sites |
| **MSVC builtins** | `__max/__min/__int64/__forceinline` | One compat-types header; grep audit |
| **High-DPI** | window points ≠ drawable pixels: input lands wrong / render in a corner (both they and we hit variants) | Decide pixel-vs-point per API call; verify input scaling end-to-end with a corner tap |
| **Dual package roots** | Two Homebrews (Intel + ARM) → pkg-config silently picks wrong arch | Pin `PKG_CONFIG_PATH`; check `lipo -info` of what you linked |
| **Graphics API conventions** | Engine code contains *deliberate corrections* for the old API's conventions (pixel-center/half-pixel offsets, viewport Y origin, clip-space Y, render-target texture origin, default address mode). Under a **faithful translator** (DXVK) they still do their job; under a **native reimplementation** (our GLES backend) they become real, uncompensated errors — a half-pixel shift, a flipped frame, an edge streak | Enumerate the conventions before writing backend code; grep `0.5`/`bias`/`flip`/`RHW`/`texel` near screen-space code. "Correct on the translator, wrong on the native backend" is the fingerprint. Full case study + debugging method: `docs/WORKDIR/lessons/LESSON-d3d-vs-gl-rasterization-conventions.md` |
| **Resource-update contracts** | The old API's lock/usage flags (`D3DLOCK_DISCARD`, `D3DLOCK_NOOVERWRITE`, the lock offset/size) are a **synchronization contract** the engine relies on for speed. A translation layer that drops them is still *correct* — and stalls the GPU on every dynamic-buffer update | Translate the flags before optimizing anything else; `NOOVERWRITE` → `GL_MAP_UNSYNCHRONIZED_BIT`, `DISCARD` → storage respecification. But verify the "old contents are dead" claim against the *engine*, not the API — here `SortingRendererClass` re-reads them. Case study: `docs/WORKDIR/lessons/LESSON-gles-dynamic-buffer-stalls.md` |
| **State-value semantics** | The old API's state values carry meaning the new one does not share: zero is a legitimate "none" (compare/write no bits, no colour channels), values are unsigned where the new API's parameter is signed, and an unimplemented op has a plausible-looking neighbour. A translator that pattern-matches zeros, casts widths, or approximates silently produces content-dependent visual bugs that look nothing like a translation error | Seed the old API's documented defaults so a zero means the caller asked for zero; keep unsigned all the way to the call; warn once per unimplemented op instead of approximating. Case studies: `docs/WORKDIR/lessons/LESSON-d3d-state-value-semantics.md` |

**When every input measures correct but the picture is wrong**, the bug is *downstream of what
you are measuring* — instrument the final value handed to the GPU, and enumerate every term of
the transform (`out = in*scale + offset`), not just the obvious one. That lesson file documents
the ~15-hypothesis hunt that rule came from.

## 5. Verification: determinism as the master gate

Their deepest practice, directly transferable to any engine with replays/lockstep:

- **Replay determinism testing in CI** — run recorded games headless, assert frame-exact outcomes. This catches "harmless" changes (math, iteration order, even rendering state feeding back into logic) that no unit test or eyeball pass would. If the target has replays, demos, or lockstep networking, make this gate #1 before touching anything.
- **Both platforms built in every session**, even when focused on one ("Windows works, Linux broken is still a failure" — Session 23). Regressions caught same-day are cheap.
- **Artifact verification over exit codes** — our session's repeated lesson, and theirs: check what got *built into* the binary (`strings`/`nm`/`otool`), not whether commands succeeded. Silent dependency fallbacks (their pkg-config arch mixups, our SDL2-WSI fallback) all pass green.
- **Behavioral acceptance criteria per phase** ("menu renders," "10-min stability"), not "it compiles."

## 6. Translation-layer integration craft (the DXVK chapter, generalized)

When you adopt a translation layer (DXVK, MoltenVK, ANGLE, Wine pieces, Rosetta-style shims):

- **Pin it to an immutable commit** of your own fork; integrate via ExternalProject/submodule. You *will* need patches — theirs accumulated 13+ (loader paths, null-descriptor workarounds for MoltenVK, shader spec-constant fixes); ours added 2 more (bundle dlopen paths, pixel-size WSI).
- **Translation layers stack**: D3D8→Vulkan→Metal worked, but each boundary has capability mismatches (MoltenVK lacks `nullDescriptor`; Metal can't disable primitive restart). Budget for reading *both* layers' source when a frame corrupts.
- **Configuration is part of the port**: ship the layer's config (`dxvk.conf`) with tuned options; defaults are tuned for desktop Windows games under Wine, not your target.
- **Learn the layer's internal conventions before patching** — DXVK loads SDL via a function-pointer table, not linking; our direct call broke the build. Read how the layer does X before adding your own X.

## 7. Process patterns (the meta-engineering that made it work)

These are the practices that let a tiny team (humans + AI agents) port 500k LOC without drowning — all directly observable in their repo:

- **Session handoff documents**: every working session ends with blockers (+ evidence), achievements (+ file paths), and an explicit "next session first task." Eliminates context-recovery cost. (Our agent-memory files serve the same role — keep doing it.)
- **A lessons library with stable IDs** (`LESSON-50`, session numbers): every painful debugging session becomes a permanently citable artifact. Monthly consolidation so knowledge doesn't scatter. *This very document exists because they did this.*
- **Source annotation convention** for every change: `// ProjectName @bugfix author date description` — makes a 5-month port diffable, reviewable, and upstream-mergeable. Categorized keywords (@bugfix/@feature/@build/@refactor/@performance).
- **Minimal-diff discipline**: one PR/commit per *category* of fix, platform code never mixed with logic changes. Their upstream rejected another fork's whole-port PR as unreviewable "AI slop" — the same work, sliced into reviewable pieces, would have landed.
- **Explicit upstream policy**: decide per change whether it's upstreamable (core fixes) or downstream-only (platform delivery), and keep the tree structured so syncs are mechanical (platform dirs = keep ours; game logic = keep theirs).
- **Agent-assisted development with guardrails** (their `AGENTS.md` + `.github/agents/*.agent.md`): agents must research reference repos before coding, fix root causes not symptoms, follow the annotation convention, and update phase checklists. Treat agent conventions as part of the codebase.
- **Reproducible build environments** (Docker for Linux targets) so "works on my machine" can't burn a session.
- **Progress metric honesty**: build-target counts and LOC are meaningless; track error *categories* resolved and behavioral milestones.

## 8. If you must be the GeneralsX: a de-risking ladder

Putting it together — the order of operations for porting an arbitrary legacy codebase to a platform nobody's targeted:

1. **Ecosystem sweep first** (always): even without a full prior port, hunt for partial references — abandoned branches, similar-engine ports, the translation layers' own sample integrations. Every subsystem someone else solved is weeks saved. Study before writing (their Phase 0 was *pure analysis*).
2. **Choose per-subsystem strategies** (§1 table) and write them down as an architecture-decision log before coding.
3. **Compile-with-stubs milestone** — everything Windows-only stubbed whole-function; fix ABI taxonomy (§4) proactively.
4. **Graphics translate + windowing swap** → first pixels. This is half the total effort; treat "menu renders" as the project's true halfway point.
5. **Audio/video swaps** behind existing manager seams.
6. **Determinism gate online** as early as replays can run; both-platform builds every session.
7. **New-architecture pass** (64-bit/ARM) as its own phase with the §4 checklist.
8. **Platform-specific delivery last** (packaging, signing, lifecycle, input paradigm — see PORTING_PLAYBOOK.md §5-7 for the iOS instance).
9. **Document as you go** (lessons library, session handoffs, annotations) — on a months-long port, the documentation *is* the velocity.

**Calibration data points:** with full references available (our case), platform delivery of an already-portable engine ≈ days. Without references but with translation layers ≈ GeneralsX: months, tiny team. With neither (true renderer rewrite) ≈ the path everyone abandons.

---

*Sources: GeneralsX repo (`docs/DEV_BLOG/2026-{02..06}-DIARY.md`, `docs/WORKDIR/lessons/*`, `docs/WORKDIR/phases/*`, `AGENTS.md`, `CONTRIBUTING.md`, CompatLib/GameEngineDevice architecture) + our own iOS port session (PORTING_PLAYBOOK.md).*
