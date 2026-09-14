# Mali GPU crash investigation (issue #9): how we got from "DirectX error" to a playable skirmish match

This documents the path from a bare `SIGSEGV` inside a proprietary Mali
driver to two concrete, verified fixes — for anyone (human or AI) picking
up a similar Mali/mobile-GPU report on this port in the future.

**Device**: Redmi Note 14 Pro, MediaTek mt6789 chipset, Mali-G57 MC2 GPU,
Android 16. **Tester**: mmtrt, GitHub issue #9.

## Starting point: an opaque driver crash

The only evidence available at first was a native crash log:

```
=== NATIVE CRASH === signal=11 (Segmentation fault) fault_addr=0x50 ===
crash PC=0x75639bcdcc is in /vendor/lib64/egl/mt6789/libGLES_mali.so+0x1f26dcc
crash LR=0x75639b424c is in /vendor/lib64/egl/mt6789/libGLES_mali.so+0x1f1e24c
```

Both PC and LR resolve *inside the vendor driver itself*, not our code —
which tells you almost nothing about what our engine/DXVK did wrong, only
that the driver didn't like it. `fault_addr=0x50` (a small, non-zero
offset) is consistent with a null-or-invalid handle being dereferenced at
some struct member offset, but that's a hypothesis, not a diagnosis.

The tester's earlier logs (before this investigation) showed the crash
happening right after the loading screen. By the time this investigation
started, an unrelated fix had already moved the crash point later — past
the main menu, into the shell-map background video — which was itself a
useful signal that progress was being made, just not the fix itself.

## Dead ends worth knowing about

- **Thread-race theory**: log lines from font/text rendering
  (`Render2DSentenceClass::Render`) and video decode (`swscaler`) were
  interleaved in the exported log, which looked like evidence of two
  threads racing. Checked the actual code
  (`FFmpegVideoPlayer.cpp`/`W3DDisplay::draw()`): there is no thread
  spawned anywhere in our video playback path: it's called synchronously,
  once per frame, from the main render loop. The interleaving was just
  ordinary single-threaded call ordering within one frame, not a race.
  Log-line adjacency is weak evidence for threading; a symbolized
  backtrace or an actual thread ID mismatch (see below) is much stronger.
- **`crash.log`/`generals-stderr.log` correlation**: before the
  crash-log-rotation fix (below), `crash.log` was append-only across every
  app launch. A crash record and a stderr log shown together in one
  export could be from two *different* sessions, making it easy to
  misattribute a crash to whatever the stderr log happened to be doing
  when it ended. Fixed by rotating `crash.log` to `crash-prev.log` on
  every launch (same pattern already used for `generals-stderr.log`), so a
  tester's next export is guaranteed to show at most one session's worth
  of crash data next to that same session's engine log.

## The actual diagnostic unlock: Vulkan validation layers

Guessing from log adjacency wasn't working. The real fix was to stop
guessing and ask the Vulkan driver directly what it didn't like about our
API usage.

DXVK already supports `VK_LAYER_KHRONOS_validation` via
`DXVK_DEBUG=validation` (`dxvk_instance.cpp`), and its debug-callback
output already goes to `std::cerr` on non-Windows (`log.cpp`) — which was
already being redirected into `generals-stderr.log`
(`SDL3Main.cpp`). The only missing piece was the validation layer binary
itself and a way to turn it on:

- Bundled `libVkLayer_khronos_validation.so` (from
  `KhronosGroup/Vulkan-ValidationLayers`'s `android-binaries-<ver>.zip`
  release asset) into `jniLibs/arm64-v8a/` — Android's Vulkan loader
  auto-discovers a layer bundled in a *debuggable* app's own native lib
  dir, no extra manifest work needed.
- Gated it behind a marker file, `dxvk_validation.txt`, dropped into the
  game data folder — same opt-in UX as the existing `gx_trace.txt` trace
  flag: no adb, no rebuild, a tester (or the game-folder owner) can just
  create an empty file and reproduce. Off by default because the layer
  adds real per-call overhead.
- See `Patches/dxvk-mali-clip-distance.patch`'s commit message and
  `scripts/build/android/fetch-vulkan-validation-layer.sh` for the exact
  mechanics.

With that in place, the *next* crash report from the same device came
with the driver's own explanation attached.

## Root cause #1: unconditional `ClipDistance`

The validation layer caught this immediately, repeating on every single
frame right up to where the log ends before the crash:

```
err:   VUID-VkShaderModuleCreateInfo-pCode-08740:
err:   vkCreateGraphicsPipelines(): ... SPIR-V Capability ClipDistance was
       declared, but ... VkPhysicalDeviceFeatures::shaderClipDistance ...
       is required.
```

D3D9's fixed-function vertex shader output signature always reserves
clip-plane output slots, whether or not a given shader/game actually uses
them. DXVK's D3D9 backend requested `shaderClipDistance`/
`shaderCullDistance` **unconditionally** at device creation
(`d3d9_device.cpp`), and the DXSO shader compiler declared SPIR-V's
`ClipDistance` capability **unconditionally** on every vertex shader
(`dxso_compiler.cpp`, `emitVsInit()`/`emitVsClipping()`). True on every
desktop GPU and on Adreno (via Turnip) — not true on Mali-G57.

**Fix**: added `DxsoOptions::supportsClipDistance`, sourced from the
actual device feature bit (`devFeatures.core.features.shaderClipDistance`
in `dxso_options.cpp`), and gated both the capability declaration and the
clip-plane output setup on it. `d3d9_device.cpp` now requests
`shaderClipDistance`/`shaderCullDistance` only if the device actually
supports them, instead of always claiming `VK_TRUE`. On hardware without
the feature, D3D9 "user clip planes" (if a map ever uses them) simply
have no effect — a graceful degradation instead of a broken shader
module. See `Patches/dxvk-mali-clip-distance.patch`.

## Root cause #2 (related, not Mali-specific): DXVK refcount memory ordering

Separately, `DxvkResourceAllocation::incRef()`/`decRef()` used
`memory_order_acquire` on *both* the increment and the decrement, with no
`memory_order_release` anywhere. On x86 — the platform DXVK is normally
developed and tested on — this is invisible, because every atomic
read-modify-write is already a full memory fence on that architecture. On
ARM's weaker memory model, it can let one thread recycle a pooled
allocation before another thread's writes to it (made just before that
thread's own decrement) are actually visible elsewhere — a live object
gets reused while still logically referenced.

This matches the exact corruption signature reported independently on
*other* Android GPUs too (issues #2, #11):

```
err:   DxvkResourceAllocationPool: corrupted free list head 0x2000000001, abandoning chain
```

— a free-list node that, when popped, holds what looks like a live
allocation's counter bytes instead of a valid `next` pointer.

**Fix**: standard correct pattern (Boost.SmartPtr, libstdc++ `shared_ptr`)
— relaxed increment, `release` on every decrement, and an `acquire` fence
taken only by whichever thread actually reaches zero, right before the
memory is reused. See `Patches/dxvk-resource-refcount-memory-order.patch`.

**Important caveat**: this did *not* eliminate the corruption warning
entirely. mmtrt's post-fix log (an actual skirmish match, no crash) still
showed `corrupted free list head` twice during play — non-fatal this
time, DXVK's own alignment guard in `DxvkResourceAllocationPool::alloc()`
catches it and abandons the poisoned chain rather than crashing. So this
fix closed off at least one real corruption path, but is not proven to be
the *only* one. Treat any future crash carrying this exact log line as
still open, and start from `dxvk_memory.h`'s `DxvkResourceAllocationPool`
rather than assuming it's already handled.

## The build trap that nearly hid both fixes

Both of the above fixes touch `references/fbraz3-dxvk` (a git submodule
built via CMake's `ExternalProject_Add` + meson/ninja, not directly by
the outer project's own build graph). After patching the submodule
source, **the compiled `.so` in every shipped APK for hours kept coming
out identical to a build from a week earlier**, despite every build
script run reporting success and the patch-application log correctly
saying "already applied."

Root cause, found by comparing file timestamps end to end
(`build/android-vulkan/_deps/dxvk-build-android/src/d3d9/libdxvk_d3d9.so`
vs. the patched source's mtime): `ExternalProject_Add`'s `BUILD_COMMAND`
runs exactly once and then trusts a stamp file forever, with *zero*
source-change detection of its own. A second, compounding bug: the
`add_custom_command` that copies the built `.so` out of DXVK's build
directory only `DEPENDS` on the `dxvk_android_build` *target* (an
ordering dependency), not on the `.so` *file*, so even after forcing
DXVK's own ninja to rebuild, Ninja had no reason to think the copy step
needed to re-run.

**Fix** (`cmake/dx8.cmake`): `BUILD_ALWAYS TRUE` on the `ExternalProject_Add`
call (Ninja's own incremental build inside DXVK's build dir is a no-op in
well under a second when nothing actually changed, so this costs
nothing), plus `BUILD_BYPRODUCTS` naming the actual `.so` paths and
extending the downstream `add_custom_command`'s `DEPENDS` to those same
file paths — so Ninja tracks the real file, not just the target.

**Verification method that actually caught this** (worth repeating for
any future DXVK-side fix in this sandbox): don't trust "BUILD SUCCEEDED."
Extract the specific library from the packaged APK and `md5sum` it
against the freshly compiled object on disk:

```bash
python3 -c "
import zipfile, hashlib
z = zipfile.ZipFile('android/app/build/outputs/apk/debug/app-debug.apk')
print(hashlib.md5(z.read('lib/arm64-v8a/libdxvk_d3d9.so')).hexdigest())
"
md5sum build/android-vulkan/_deps/dxvk-build-android/src/d3d9/libdxvk_d3d9.so
```

If those two hashes don't match, the fix you just wrote is not in the
APK you're about to ship, no matter what the build log says.

## Outcome

mmtrt confirmed: main menu loads, a skirmish match starts, buildings
construct, no crash — `crash.log` empty for that session. First
confirmed-playable report on this device.

## Commit reference (branch `claude/dxvk-vulkan11-experiment`)

| Commit | What |
|---|---|
| `62ef329` | Rotate `crash.log` to `crash-prev.log` per launch (stopped conflating sessions) |
| `0a48016` | Bundle Vulkan validation layer, opt-in via `dxvk_validation.txt` |
| `6cfd3b5` | DXVK refcount memory-ordering fix (root cause #2) |
| `c99e830` | Mali `ClipDistance` fix (root cause #1) |
| `9673d5a` | Fix DXVK never actually rebuilding after the first build (the build trap above) — this is the commit that made the previous two *actually* ship |

## If you're debugging a similar report next

1. Get a validation-layer log first, before theorizing. `dxvk_validation.txt`
   in the game data folder, retest, export via Settings → View Logs →
   Share.
2. If the fix touches `references/fbraz3-dxvk`, verify the shipped `.so`'s
   checksum against a fresh compile before telling a tester it's fixed —
   see the build trap section above. This bit us twice today.
3. `DxvkResourceAllocationPool: corrupted free list head` is not fully
   closed out. If it reappears, don't assume the ARM memory-ordering fix
   already covers it.
