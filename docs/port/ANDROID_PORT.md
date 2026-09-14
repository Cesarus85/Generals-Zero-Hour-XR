# Zero Hour on Android — Port Guide

**Status: campaign/skirmish/Challenge run natively, and online multiplayer
works.** GitHub Actions (§3, Option A) builds and links the full engine,
compiles DXVK's d3d8/d3d9 for Android, and packages a signed APK on every
push — first achieved 07/07/2026. Since then this has moved well past initial
bring-up: **GeneralsOnline**, a from-scratch NGMP-based multiplayer backend
(REST + WebSocket) replacing the long-dead GameSpy servers, has been built and
wired into the original `.wnd` UI — account login, the multiplayer Welcome
screen, Custom Match (create/browse/join, live rosters, chat), Quickmatch, My
Persona (stats/rank), and Communicator (friends/social) all work end-to-end
against real players on real devices, including bug reports from outside
testers via this repo's issue tracker. §6 has the running log of what's been
found and fixed; most of it falls into a small number of recurring bug
classes (async-callback use-after-free, an INI-parsing gap from the
Generals/Zero Hour code split, a memory-allocator ownership gap) rather than
one-off mysteries — worth reading before chasing a new crash from scratch.

**Related community work, worth tracking:**
- [tarek369/GeneralsZH-Android](https://github.com/tarek369/GeneralsZH-Android)
  — an independent Android port of the same GeneralsX lineage (same DXVK
  D3D8→Vulkan approach, same directory layout, same `package-android-zh.sh`
  naming). Its [`android.md`](https://github.com/tarek369/GeneralsZH-Android/blob/main/android.md)
  engineering log is where the D3D format-selection bug in §6 was traced from;
  worth re-checking for new findings as their v0.1-android alpha matures.
- [p0ls3r/GenLauncher](https://github.com/p0ls3r/GenLauncher) — a Windows-only
  C# mod-management launcher for Generals/Zero Hour (symlink-based mod
  isolation, not portable to Android's sandboxed storage model as-is). Not
  integrated; a from-scratch Android launcher activity (mod list, GameData
  picker, "verify install" diagnostics) inspired by its UX is a plausible
  future addition, not a port of the C# code itself.

---

## 0. Architecture (what changes vs iOS — mostly: less)

```
Game code (1.6M LOC C++, unmodified game logic, loads retail .big assets)
  │
  ├─ Windowing/input ........ SDL3 (in-tree FetchContent) + SDLActivity Java shell
  ├─ Rendering ............... DirectX 8 calls → DXVK 2.6 d3d8/d3d9 (.so)
  │                            → Vulkan → vendor driver (Adreno/Mali)   ← no MoltenVK!
  ├─ Audio ................... OpenAL (openal-soft; OpenSL/AAudio backends)
  ├─ Video ................... FFmpeg 8.1 (vcpkg, static)
  ├─ Text .................... FreeType + bundled .ttf fonts (same iOS bundled-font
  │                            locator; Android has no fontconfig either)
  └─ App shell ............... android/ Gradle project; game = libmain.so loaded
                               by SDL3's SDLActivity; assets in external files dir
```

Android is the *easier* mobile target on the graphics axis: Vulkan is the
native GPU API, so one whole translation layer (MoltenVK) and its capability
mismatches drop out of the stack. What Android adds instead is the app-model
plumbing: the game must be a shared library in an activity process, storage is
scoped, and the GPU driver landscape is fragmented (see §2).

The touch gesture state machine, app-lifecycle render gate, resolution
injection, and bundled-font lookup from the iOS port apply 1:1 — they are now
compiled under a shared `SAGE_MOBILE_PLATFORM` guard
(iOS `TARGET_OS_IPHONE` **or** `__ANDROID__`) rather than iOS-only.

## 1. What was changed for Android (file manifest)

| File | Purpose |
|---|---|
| `CMakePresets.json` | `android-vulkan` preset: vcpkg + chainloaded NDK toolchain, arm64-v8a, API 28, `SAGE_DXVK_USE_LOCAL_FORK=ON` |
| `cmake/triplets/arm64-android.cmake` | overlay triplet pinning API level for vcpkg-built deps |
| `cmake/meson-arm64-android-cross.ini.in` | DXVK meson cross file (NDK clang, static libc++ into the DXVK libs) |
| `cmake/dx8.cmake` | `elseif(ANDROID)` branch: builds DXVK d3d8/d3d9 from the local fork with meson; same sdl3.pc trick as macOS (silent-SDL2-WSI trap); artifact copy to build root |
| `Patches/dxvk-android.patch` | unversioned `.so` names — APKs can't carry `libdxvk_d3d9.so.0.20600` + symlinks; verified to apply cleanly together with `dxvk-ios.patch` (whose WSI pixel-size fix Android also wants) |
| `cmake/sdl3.cmake` | Android: no system libpng (stb decodes PNG), no TIF/WEBP backends |
| `Core/.../WW3D2/CMakeLists.txt` | `SAGE_USE_FREETYPE` + Freetype link on Android; fontconfig excluded |
| `Core/.../WW3D2/render2dsentence.{h,cpp}` | bundled-font locator now iOS **and** Android |
| `GeneralsMD/Code/Main/CMakeLists.txt` | Android: `z_generals` builds as `libmain.so` (SDLActivity convention) instead of an executable |
| `GeneralsMD/Code/Main/SDL3Main.cpp` | Android env bootstrap: HOME→internal storage, cwd→external files dir (or its `GameData/`), DXVK cache→app cache dir, stderr→pullable log file with rotation, Options.ini seeding; fullscreen/immersive; `SDL_HINT_ANDROID_BLOCK_ON_PAUSE=0`; OpenAL Linux workarounds excluded on Android (they would mute OpenSL/AAudio) |
| `GeneralsMD/.../SDL3GameEngine.cpp` | touch gesture state machine + lifecycle render gate, generalized to `SAGE_MOBILE_PLATFORM` |
| `GeneralsMD/.../SDL3Device/GameClient/TouchInput.{h,cpp}` | what a gesture *means*, resolved against the engine's own rules with no synthesized pointer — see §2a |
| `android/` | Gradle shell: `GeneralsZHActivity extends SDLActivity`, asset extraction, missing-game-data dialog, placeholder adaptive icon |
| `scripts/build/android/{build,package}-android-zh.sh` | build + verify artifacts (AArch64, `Sdl3WsiDriver` compiled in), stage jniLibs/Java/assets, gradle assemble |
| `vcpkg.json` | fontconfig excluded on android; ffmpeg enabled for android |

`dx8wrapper.cpp` needed **no change**: its existing Linux branch dlopens
`libdxvk_d3d8.so` by bare name, which on Android resolves through the app's
linker namespace to the APK-packaged library. Same for DXVK's Vulkan loader:
its non-Apple list already tries `libvulkan.so` first — that *is* Android's
system Vulkan loader.

## 2. The device / driver matrix (read this before filing "black screen" bugs)

> **Since 1.2.1, OpenGL ES is the default backend**, in the launcher and in
> the engine alike (`d3d8gles_ShouldUseVulkanBackend()` returns false with no
> `render_backend.cfg`). Everything in this section describes Vulkan, which is
> now the *opt-in* path — faster where the driver handles it, and reported
> unusable on PowerVR BXM and Samsung Xclipse, where it renders magenta
> patches, distorted geometry and corrupted menus that nothing above the
> driver can fix. Setup's Render Backend card shows the GPU's own name, since
> every such report has been a driver family rather than a device model.

DXVK's own minimum was originally Vulkan 1.3 with a handful of features
(robustness2, null descriptors, etc.), which is still the *best-supported*
path. `Patches/dxvk-vulkan11-adaptive.patch` (see the archive note at the
bottom of the repo's plan history, or just `git log --oneline -- Patches/dxvk-vulkan11-adaptive.patch`)
lowered the hard floor to **Vulkan 1.1**, with the required features chained
through KHR/EXT fallback extensions when the device doesn't have them
natively — plus a long tail of Mali/Bifrost-specific fixes (see the
`dxvk-mali-g76-*.patch` series in `Patches/`) for real crashes found on
real Mali-G76 hardware. A device with no usable Vulkan driver at all
(< 1.1, or a completely broken one) still gets a clear on-screen message
instead of a silent black screen.

| Device | SoC / GPU | Vulkan | Verdict |
|---|---|---|---|
| **Poco F8 Pro** | Snapdragon 8 Elite / **Adreno 830** | 1.3+ (excellent proprietary driver) | **Best case.** GPU-wise a 2003 title is a rounding error; expect native-res 120 Hz. |
| **Redmi Note 8 Pro** | Helio G90T / **Mali-G76 MC4** | **1.1 only** | **Supported**, via the adaptive Vulkan 1.1 path above. Frame rate is CPU-bound rather than GPU-bound on this class of device — expect noticeably lower FPS than an Adreno 7xx/8xx phone, and occasional freezes on map/mission load, not a GPU limitation. |

Any other Vulkan-1.1-capable Mali GPU (Mali-G57 and similar Bifrost/Valhall
chips) is expected to behave the same way as the Redmi Note 8 Pro above —
supported, CPU-bound. Adreno phones below Vulkan 1.3 get an optional bundled
Mesa Turnip fallback driver instead (see below); this does not apply to
Mali/PowerVR.

**About the helper repos provided alongside this one:**
- **Turnip_drivers_adreno / Mesa Turnip** — the open-source Vulkan driver for
  **Adreno 6xx/7xx**. It does *not* support the Adreno 830 (a8xx support is
  still maturing in Mesa), and the 8 Elite's stock driver is already Vulkan
  1.3-complete, so Turnip is **not needed** on either flagship device above.
  It's the automatic fallback for older/lower-Vulkan Adreno phones (e.g.
  Adreno 642L), via the AdrenoTools loading mechanism below.
- **AdrenoToolsDrivers** — packaged driver bundles consumed by
  [libadrenotools](https://github.com/bylaws/libadrenotools), which lets an app
  load a replacement Vulkan driver *into its own process* (how Winlator and the
  Switch emulators do it). The Setup app's Driver Options card uses this to
  apply the bundled Turnip build automatically on a qualifying Adreno phone,
  or let you import your own adrenotools-format driver .zip.
- **Winlator / MiceWine** — the *other* way to run Zero Hour on Android: the
  unmodified Windows binary under Wine + Box64 + DXVK-as-DLLs. It works today
  but pays the x86→ARM emulation tax and fights input/latency. This repo is the
  native path: the real engine compiled for ARM64, zero emulation, RTS-tuned
  touch controls. The Winlator tree remains valuable as a **reference** for
  Android-side DXVK configuration and driver quirks.

## 2a. Input: native touch, not mouse emulation

Battlefield input does **not** synthesize `MSG_RAW_MOUSE_*`. Taps, double
taps, the long press, the two-finger cancel and ability targeting are
resolved directly through the engine's own decision-making:

```cpp
TheTacticalView->pickDrawable(&screenPoint, forceAttack, pickType);
TheTacticalView->screenToTerrain(&screenPoint, &worldPos);
TheGameClient->evaluateContextCommand(draw, &worldPos, DO_COMMAND);   // or EVALUATE_ONLY
```

This is the same path a click on the radar minimap has always used
(`ControlBarCallback.cpp`), and it produces exactly the same `MSG_DO_*`
orders the mouse path produced — so multiplayer and replays are untouched.
`EVALUATE_ONLY` answers "what order *would* this be" without issuing it,
which is what drives target feedback and resolves the one ambiguous tap
(tapping your own unit: select it, or get in / repair it).

Deliberately still on the click path: taps on the control bar and menus (a
tap on a button *is* a click), building placement (`PlaceEventTranslator` is
its own press/drag/release state machine), and the selection box. The window
manager gets first refusal on every battlefield tap, so a tap on a panel
cannot fall through to the map underneath.

Screen-edge scrolling is off on touch: it is defined by a pointer resting
near an edge and ends only when a later pointer event reports a position back
inside the safe zone, which cannot happen without a pointer.

**If you are going to touch input code, read
[`docs/WORKDIR/lessons/LESSON-touch-input-is-not-a-mouse.md`](../WORKDIR/lessons/LESSON-touch-input-is-not-a-mouse.md)
first.** It documents why the emulation approach failed, the five pointer
properties a finger lacks, the two patterns that keep coming back (preview
code must be *told* the aim point, not ask the mouse; engine state a mouse
would refresh constantly must be re-asserted once a frame), and how to add a
gesture without reintroducing any of it.

Debugging: Setup → Diagnostics → **Touch input overlay** draws the gesture
phase, the finger, the engine's pointer position and the camera scroll anchor
together, and writes `[gxtouch]` lines to the log on every phase or
camera-mode change.

## 3. Building

### Option A — GitHub Actions (no local NDK/toolchain needed)

`.github/workflows/build-android.yml` builds the whole stack on a GitHub-hosted
runner and uploads a ready-to-sideload APK as a workflow artifact:
vcpkg deps → DXVK d3d8/d3d9 → `libmain.so` → Gradle `assembleDebug`, with the
same artifact verification (`Sdl3WsiDriver` compiled in, AArch64 ELF) the local
build script does.

Trigger it from the **Actions** tab → *Build Android* → *Run workflow*, or just
push to `main`/`claude/**` touching engine or `android/` files. Download the
`GeneralsXZH-android-<run>.apk` artifact from the run summary and `adb install`
it (or transfer + tap-install on the phone).

**Every CI build is signed with the same committed debug key**
(`android/app/debug.keystore` — a fixed, non-secret debug key checked into the
repo instead of the machine-local key Android tooling normally auto-generates)
and gets a strictly increasing `versionCode` from the workflow run number.
That combination is what makes consecutive CI builds installable **as updates
over each other** — tap a newer APK on the phone without uninstalling first —
instead of Android refusing the install over a signature mismatch.

**On a fork, Actions must be enabled once**: GitHub disables workflow runs on
forks by default. Go to the repo's **Actions** tab → click **"I understand my
workflows, go ahead and enable them"** (or Settings → Actions → General →
"Allow all actions and reusable workflows"). This is a one-time, per-repo
setting; it isn't something a workflow file can turn on for itself.

### Option B — Local build

Host: Linux or macOS.

```sh
# One-time
git clone <this repo> && cd <repo>
git submodule update --init references/fbraz3-dxvk
git clone https://github.com/microsoft/vcpkg ~/vcpkg && ~/vcpkg/bootstrap-vcpkg.sh
export VCPKG_ROOT=~/vcpkg
# Android Studio SDK Manager (or cmdline-tools): install NDK 26+, platform 35, build-tools
export ANDROID_NDK_HOME=~/Android/Sdk/ndk/<version>
# meson + ninja + pkg-config via pip/brew/apt

# Build native code (game -> libmain.so, DXVK -> libdxvk_d3d8/9.so) and verify
./scripts/build/android/build-android-zh.sh

# Stage everything into the Gradle shell and produce the APK
./scripts/build/android/package-android-zh.sh --install
```

The first configure builds vcpkg deps (ffmpeg, curl+openssl, freetype…) for
`arm64-android` — expect 30–60 minutes cold.

## 4. Game data and first run — the in-app Setup flow (no adb, no PC needed)

No assets ship in the APK (2.7 GB, and they're the user's own). Installing
the APK also installs a **second launcher icon, "GeneralsZH Setup"**
(`SetupActivity`) — a standalone screen for everything that used to require
`adb`:

1. **Install the APK, then open "GeneralsZH Setup"** (not the game icon yet).
2. Tap **Select Game Folder**. First time, Android will ask for the "All
   files access" permission (`MANAGE_EXTERNAL_STORAGE`) — a normal system
   permission screen, no root, no PC. Grant it, come back, tap the button
   again.
3. A plain folder browser opens, starting at the device's internal storage
   root. Copy your own Command & Conquer Generals Zero Hour install (the
   `*.big` archives, `Data/`, `ZH_Generals/`) into **any** folder you like
   first — e.g. `Downloads/GeneralsZH/`, reachable from any file manager or a
   normal USB-cable "transfer files" connection, no special app needed — then
   navigate to it in the picker and tap **Use This Folder**. The picker
   flags a folder green once it sees `INIZH.big`/`INI.big`.
4. Tap **Launch Game** (or go back to the regular game icon — both work; the
   folder choice is saved).

The engine reads the picked path from a marker file
(`SDL3Main.cpp` chdir logic) written by Setup — no `adb push` into the
scoped-storage-restricted `Android/data/<pkg>/files` folder is needed anymore
(that convention still works if you already have files there, but is no
longer the documented path). Auto-extracted runtime files (`fonts/`,
`dxvk.conf`, `DefaultOptions.ini`) still land in the app's own external files
dir automatically on first launch, independent of where the game data lives.

User data (Options.ini, saves, map cache) lives in **internal** storage via
`HOME` and survives reinstalls of the game-data folder. The DXVK shader cache
goes to the app cache dir (OS-purgeable).

### Reading logs without adb

Open **GeneralsZH Setup → View Logs**. It shows, without any PC:
- `crash.log` — written by a signal handler that installs the instant
  `libmain.so` is loaded (an ELF constructor, before any engine code runs),
  so it captures crashes from *before* the engine's own logging is even set
  up — the single biggest gap in the original adb-only workflow, where a
  crash at library-load time was invisible without a rooted device.
- `generals-stderr.log` (+ `-prev.log`) — the regular engine log, active once
  `main()` starts.

Use the **Share** button there to send the logs to yourself (email, messaging
app, anywhere) directly from the phone — it bundles `crash.log` (+
`crash-prev.log` if present) and `generals-stderr.log` (+ `-prev.log`) into a
single `generalszh-logs.zip` so you don't need to attach several files.
`adb pull`/`adb logcat` still work too and remain useful for anything the
in-app viewer can't show (an OS-level tombstone needs adb + often root;
`crash.log` is the no-root substitute for the common case).

### Diagnostic marker files (opt-in extra logging)

None of these are on by default — a plain log from a fresh install is small
and readable. If a bug report needs more detail, ask for one of these: **drop
an empty text file with the exact name below into the same game data folder
you picked in Setup → Select Game Folder** (the one that has your `.big`
files in it — no `adb`, no rebuild, just a file manager). Delete the file (or
uninstall/reinstall the game data) to turn it back off; each one is read once
per launch.

| File to create | What it does | When to ask for it |
|---|---|---|
| `gx_trace.txt` | Turns on full `[GX-TRACE]` logging (font/UI/INI internals, very verbose). | Chasing a hang or a specific UI/parsing bug. **Warning:** on a long session this can produce a 50-100+ MB log — use `gx_perf.txt` instead if you only need frame-timing numbers. |
| `gx_perf.txt` | Turns on just the low-volume `[GX-PERF]` line (one per second: time spent in radar/audio/client/network/logic/render, per subsystem). Implied automatically by `gx_trace.txt`. | Performance/FPS reports — this is the one that actually shows where frame time goes, safe to leave on for a whole session. |
| `dxvk_hud.txt` | Turns on DXVK's own HUD (on-screen counters, mirrored into the log as `DXVK_HUD: ...` lines). Leave the file empty for the safe default (`fps,drawcalls,submissions,pipelines`); anything you type inside becomes the element list. `frametimes`/`memory` are refused and fall back to the default — those two crash on this port's Vulkan 1.1 devices. | GPU-side numbers: draw call counts, submissions, pipeline count. |
| `dxvk_validation.txt` | Turns on the Vulkan validation layer (`DXVK_DEBUG=validation`) — adds real per-call overhead, so only for a specific repro, not everyday testing. | Suspected Vulkan API misuse (validation errors/UB), not a performance report. |
| `dxvk_verbose_log.txt` | Bumps `DXVK_LOG_LEVEL` from this build's default of `error` up to `info`, surfacing DXVK's normally-silent "Presenter: Actual swapchain properties" line (format, present mode, image count, composite alpha). Negligible overhead. | Tracking down a device-specific compositing/present artifact (e.g. a screenshot- or task-switcher-only glitch). |

All five are read from the working directory the engine `chdir()`s into at
startup — the game data folder, same place as above, not the app's internal
storage. Setup → Diagnostics has a switch for each one, so testers don't need
a file manager to create them by hand.

**→ For sharing this section directly:
[docs/port/ANDROID_PORT.md#diagnostic-marker-files-opt-in-extra-logging](docs/port/ANDROID_PORT.md#diagnostic-marker-files-opt-in-extra-logging)**

## 5. Verification checklist for first device bring-up

In dependency order; each gate isolates a failure class (the iOS port's ladder,
§8.2 of the playbook):

1. **vcpkg deps build for arm64-android** — watch ffmpeg and curl/openssl; both
   are supported by upstream vcpkg but versions move. `PKG_CONFIG_PATH` is
   blanked by the preset so host libs can't leak.
2. **Game code compiles under NDK clang/bionic** — expect a small round of
   fixes: bionic lacks some glibc-isms the desktop Linux build may lean on
   (`glob.h` exists since API 28 — that's part of why minSdk is 28).
3. **DXVK meson cross-build** — `dxvk-android.patch` + `dxvk-ios.patch` apply
   automatically at configure. Verify after build (the build script does):
   `strings libdxvk_d3d9.so | grep Sdl3WsiDriver`, `llvm-readelf -h` says
   AArch64, and `llvm-readelf -d libdxvk_d3d8.so` shows `SONAME libdxvk_d3d8.so`
   (no `.0` suffix — that's what the android patch is for).
4. **APK loads: `System.loadLibrary("main")` succeeds** — failure here =
   missing DT_NEEDED in jniLibs (check `llvm-readelf -d libmain.so` against the
   staged file list).
5. **D3D init: DXVK creates a device** — check `generals-stderr.log` for
   `Direct3DCreate8 returned` non-null and the adapter line naming the real GPU
   (`Adreno 830`). Failure modes: Vulkan feature gaps (Mali!), swapchain/WSI.
6. **Menu renders at native res** — the project's true halfway point.
7. **Touch controls** — tap/drag/long-press/two-finger pan/pinch, then the
   corner-tap scaling check (synthetic events carry windowID; wrong scaling
   lands taps off toward screen edges).
8. **Background/resume torture** — app switcher in and out ×10 during a
   skirmish; the lifecycle gate must keep DXVK off the dead surface. Android
   *destroys* the surface on background (unlike iOS which only seizes it) — if
   resume shows a black screen, DXVK's `VK_ERROR_SURFACE_LOST` handling needs
   the next round of work.
9. **Audio** — openal-soft must pick OpenSL or AAudio (the desktop-Linux
   `ALSOFT_DRIVERS` overrides are explicitly compiled out on Android). EVA,
   music, unit responses; then the §8.2/8.3 playbook regressions (stuck
   `disallowSpeech`, chirping zombie streams) — those fixes are in the shared
   code and should just hold.
10. **10-minute skirmish stability**, then a full Generals Challenge run
    (exercises the radar-format fallback fixed in `W3DRadar.cpp` — also shared
    code, also expected to hold).

## 6. Known gaps / next steps

### Native touch input, and OpenGL ES as the default (06/09/2026, v1.2.1)

Battlefield input stopped synthesizing a pointer. See §2a for the shape and
the lesson document for why. What changed, and what each item had been
reported as:

| Was reported as | Actually was |
|---|---|
| "the building silhouette appears in the middle of the screen" | `handleBuildPlacements()` read the mouse position, which on a touchscreen is a leftover from the last gesture |
| "the map flies around by itself, only the pause menu fixes it" | `SCROLL_RMB` latched by a right-button-up that `SelectionXlat` legitimately destroys to cancel a GUI command; and `SCROLL_SCREENEDGE`, whose exit condition needs a pointer |
| "the ability circle flashes and disappears" | the decal is created once and destroyed by any of seven `setRadiusCursorNone()` sites; the mouse path recreates it a frame later, native aiming does not |
| "the description vanishes while I'm still holding" | the popup was being *deleted*, not hidden, whenever the show flag was false for one frame — and the control bar rebuilding its command windows mid-hold made that happen |
| "the intro can't be skipped by tapping" | skipping was never in the translator chain; `WindowXlat` watches for an unconsumed raw left-button-down |
| "tapping in the powers panel also set a rally point" | the native path skipped the window manager's first refusal, which the synthetic click used to get for free |

Also in this release: screen-edge scrolling off on touch; a **Touch input
overlay** diagnostic (Setup → Diagnostics) drawing the gesture, the finger,
the engine's pointer and the camera anchor together with `[gxtouch]` log
lines; **OpenGL ES promoted to the documented default** with the picker text
rewritten to match reality and the GPU name shown; and the launcher finished
in all 13 languages (the log viewer and DXVK config card were still English).

### UX overhaul: in-app Setup, folder picker, crash log viewer (07/07/2026)

Real-device feedback: sideloading testers don't reliably have a PC handy for
`adb`, and a crash before the engine's own logging starts (library-load time)
was completely invisible without root. Addressed with three new pieces
(§4 documents the user-facing flow):
- `AndroidCrashHandler.cpp` — a signal handler installed as an ELF
  constructor (runs at `dlopen()` time, before `JNI_OnLoad`/`main()`/any
  engine code), writing signal + fault address to internal storage via raw
  `open()`/`write()`/`close()` syscalls only (no libc buffering, survives a
  corrupted heap), then chains to the previous handler so the OS tombstone
  still generates too. Path is derived from `getuid()/100000` rather than a
  hardcoded `/data/data/...`, since that shortcut only resolves correctly for
  Android's primary user profile (breaks under a work profile / secondary
  user / guest mode).
- `SetupActivity` / `FolderPickerActivity` / `LogViewerActivity` — a second,
  always-present launcher icon ("GeneralsZH Setup") with a plain
  `java.io.File`-backed folder browser (deliberately not the SAF
  `ACTION_OPEN_DOCUMENT_TREE` picker, which hands back a `content://` tree
  the engine's plain `fopen()`/`chdir()` can't use without copying the whole
  2-3 GB game data first) gated behind the `MANAGE_EXTERNAL_STORAGE`
  permission, plus an in-app log viewer with Copy/Share buttons.
- `GeneralsZHActivity` no longer calls `super.onCreate()` (which triggers
  `System.loadLibrary("main")`) at all when no valid game folder is
  configured — redirects to Setup instead, so a missing-game-data install
  can never look like, or mask, a native crash.

### Fixed from real-device testing (07/07/2026)

- **`libgamespy.so` missing from the APK → `dlopen` crash on launch.**
  GamespySDK's own `CMakeLists.txt` does
  `set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})` — and
  `CMAKE_BINARY_DIR` names the *outermost* project's build dir even from
  inside a FetchContent'd subproject, so the library lands directly at
  `${BUILD_DIR}/libgamespy.so`, not under `_deps/gamespy-build/` like every
  other FetchContent output in this tree. `package-android-zh.sh` now looks
  there, and every SDL3_image/openal/gamespy library is a hard packaging
  failure instead of a warning. CI additionally verifies, via
  `llvm-readelf -d`, that every non-system `DT_NEEDED` of `libmain.so` is
  actually present in the built APK — the general form of this check, so the
  next missing library fails CI instead of shipping.
- **D3D device creation fails on a fresh install (`D3DERR_NOTAVAILABLE`).**
  `dx8wrapper.cpp` forces `_PresentParameters.Windowed = TRUE` on all
  non-Windows platforms (an existing Linux/Wayland fix), but the *format
  selection* branch a few lines below still keyed off the raw `IsWindowed`
  game setting — which defaults to `false` (fullscreen) with no `Options.ini`
  yet. That took the `else` branch's `Find_Color_And_Z_Mode()`, which has no
  real adapter enumeration under DXVK-native and returns `D3DFMT_UNKNOWN`,
  and `CreateDevice` refuses the mismatched result. Fixed by making the
  format-selection branch agree with the same platform reality
  (`useWindowedFormatPath`), plus a `D3DFMT_X8R8G8B8` fallback (Windows-only
  behavior unchanged) when `GetAdapterDisplayMode` itself reports
  `D3DFMT_UNKNOWN`. Traced from
  [tarek369/GeneralsZH-Android](https://github.com/tarek369/GeneralsZH-Android)'s
  engineering log, then independently confirmed against our own
  `dx8wrapper.cpp`.
- **`RTS_GAMEMEMORY_ENABLE=OFF` on the `android-vulkan` preset.** The engine's
  custom pool allocator intercepts global `operator delete` process-wide;
  freeing memory that OpenAL or libc++ containers allocated through the
  *system* allocator via that pool corrupts its bookkeeping. This is the same
  class of conflict the ASAN build config already disables the pool for —
  applied preemptively here rather than waiting for the heap-corruption crash
  tarek369's log describes hitting.

### Investigated, not changed

- **Archive-override priority (`ArchiveFileSystem::loadIntoDirectoryTree`)**:
  tarek369's log describes a `std::multimap` insertion-order bug causing most
  Zero Hour locomotors to be shadowed by base-Generals ones. Independent
  review of our copy: `dirInfo->m_files.insert(fileIt, ...)` with
  `fileIt = m_files.find(token)` is the standard, correct idiom for
  "insert immediately before the first existing entry of this key" in a
  multimap — the C++ standard guarantees O(1) insertion exactly there when the
  hint is valid for that position, which `find(token)` always is here. No
  reproduction on our tree; **flagged for verification during real gameplay
  testing** (compare Zero Hour unit movement/locomotor behavior against
  retail) rather than patched speculatively — this code runs identically on
  every platform this project ships, so a wrong fix here would be a silent,
  wide-blast-radius regression.
- **FFmpeg on Android**: tarek369's log states vcpkg's arm64-android FFmpeg
  port was broken for them and they stubbed it out
  (`RTS_BUILD_OPTION_FFMPEG=OFF`). Our own CI has FFmpeg **on** and links
  successfully against the pinned vcpkg commit (`VCPKG_COMMIT` in
  `build-android.yml`) — contradicts their finding, likely a difference in
  vcpkg version. Left enabled; watch cutscene playback specifically during
  device testing since that's the one path that would surface a silently
  broken decode.
- **Fresh-install language/registry fallback**: tarek369's log describes a
  crash in a `LanguageRegistry::init()`-equivalent when no `Options.ini`
  exists yet. No class by that name exists in our tree (different fork
  lineage); `registryini.cpp`'s `GetStringFromRegistry` already reads through
  an INI-based compat shim rather than a real registry, which should degrade
  gracefully, but this is unverified without a fresh-install device test.

- **Gradle wrapper is not committed** (binary jar). Use a system Gradle 8.x or
  open `android/` once in Android Studio to generate it.
- **Multiplayer works on Android** via GeneralsOnline (see the status note at
  the top of this doc) — this is no longer the "broken everywhere, GameSpy is
  dead" story that's still true for the untouched macOS/iOS builds. Retail LAN
  play (cross-platform float determinism against a Windows client) remains
  unverified, as on every other port.
- **Mali/Vulkan 1.1 devices** (e.g. Redmi Note 8 Pro / Mali-G76): can't run the
  DXVK 2.6 path — hardware limitation, not fixable in software. The app now
  shows a clear native dialog ("please make sure you have DirectX 8.1 or
  higher...") instead of silently closing, via `SDL_ShowSimpleMessageBox` in
  `Debug.cpp`'s crash-message paths. d3d8to9 + DXVK 1.10 remains the
  theoretical route to real Vulkan-1.1 support if demand exists.
- **libadrenotools driver replacement**: optional future hook for non-target
  Adreno devices with broken vendor drivers (Turnip_drivers_adreno /
  AdrenoToolsDrivers repos are the driver source for that).
- **Back button**: fixed — SDL reports it as `AC_BACK`, which was being read
  through a scancode variable one byte narrower than `SDL_Scancode`, truncating
  it and misdispatching to an unrelated in-game command. Now correctly opens
  the pause/options menu. On-screen keyboard (`SDL_StartTextInput`) for save
  names still needs verification (the engine already gates text input on
  entry-field focus).
- **Performance**: no tuning expected to be needed (2003 game, 2024+ silicon),
  but `CADisableMinimumFrameDurationOnPhone`'s Android analog — high-refresh
  frame pacing via `SDL_HINT_ANDROID_LOW_LATENCY_AUDIO` and Swappy-style pacing
  — is unexplored.
