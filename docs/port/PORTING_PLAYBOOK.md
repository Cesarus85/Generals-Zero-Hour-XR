# Porting a Classic Windows Game to iOS — Complete Playbook

> **Companion doc:** [`PORTING_PATTERNS.md`](PORTING_PATTERNS.md) — the generalized from-scratch methodology distilled from the GeneralsX project's own dev diaries and lessons library. Read it when there's NO existing port to build on (strategy selection, compat-shim craft, portability bug taxonomy, determinism gates, process patterns).

**Case study: Command & Conquer Generals — Zero Hour (2003, Win32/DirectX 8) → iPhone 17 Pro Max + iPad mini, fully playable, June 2026.**

This documents every decision, problem, and fix from the project so that any engineer or agent can repeat it — for this game or a similar one. Total effort: one long working session. Source tree: `the repo root` (fork of fbraz3/GeneralsX). All file references below are relative to that tree unless absolute.

---

## 0. Final architecture (what "ported" means here)

```
Game code (1.6M LOC C++, GPL v3, EA source release)
  │  unmodified game logic — 1:1 gameplay, loads retail .big assets
  ├─ Windowing/input ........ SDL3 (3.4.2, in-tree FetchContent)
  ├─ Rendering ............... DirectX 8 calls → DXVK 2.6 d3d8/d3d9 (dylibs)
  │                            → Vulkan → MoltenVK 1.4.1 (dynamic framework)
  │                            → Metal → Apple GPU
  ├─ Audio ................... OpenAL (openal-soft 1.24.2, replaces Miles)
  ├─ Video ................... FFmpeg 8.1 (replaces Bink)
  ├─ Text .................... FreeType + bundled .ttf fonts (replaces GDI;
  │                            fontconfig on macOS/Linux, bundled-font lookup on iOS)
  └─ App shell ............... XcodeGen-generated signed bundle; assets inside
                               GameData/; saves in Library/Application Support
```

Distribution: personal development signing (paid Apple Developer team), installed via `devicectl`. Multiplayer is broken in ALL native ports of this engine (cross-platform float determinism) — "fully playable" = campaigns + skirmish vs AI.

---

## 1. Phase 0 — Research before engineering (half the outcome)

**The single highest-leverage step.** The naive plan (port EA's raw source) is a multi-month job. The actual job was "port the best community fork," which was a one-session job. Before touching a compiler:

1. Map the ecosystem: original repo → most-starred forks (`gh api repos/<owner>/<repo>/forks?sort=stargazers`), the central community fork, and what each actually achieves. Distinguish **merged-and-working** from **WIP-branch** from **README claims**.
2. For an iOS target, the checklist a base must already pass:
   - **Compiles and runs 64-bit** (iOS is arm64-only; legacy Win32 games are 32-bit by policy in many community forks)
   - **Runs on ARM64** (proves no x86 inline-asm or endianness/alignment landmines remain)
   - **Windows API layers replaced**: windowing (SDL), D3D (DXVK or GL/Metal rewrite), audio (OpenAL), video codecs (FFmpeg), file dialogs/registry shims
   - Loads retail assets unmodified
3. What we found (June 2026): upstream community repo = still 32-bit Windows-only by policy (VC6 retail compat); Fighter19 fork = Linux x64/ARM64 native; **fbraz3/GeneralsX = macOS ARM64 native with SDL3+DXVK+MoltenVK+OpenAL+FFmpeg — chosen base**. No prior iOS/Android effort existed anywhere.

**Lesson:** if a fork already runs on Apple Silicon macOS, the iOS port is "cross-compile + sandbox + lifecycle + touch," not "port a game."

---

## 2. Phase 1 — Bring it up on the host (macOS) first

Always make the macOS (or Linux) build work before attempting iOS. It validates the whole stack in a debuggable environment and becomes your fast iteration loop later (asset issues, gameplay verification).

**Toolchain**: Xcode + CLT, Homebrew cmake/ninja/meson, vcpkg (full clone), LunarG Vulkan SDK (`~/VulkanSDK/<ver>`; `source setup-env.sh` before configuring — CMake's FindVulkan needs `VULKAN_SDK`).

### Build failures hit on macOS, and fixes
| Symptom | Root cause | Fix |
|---|---|---|
| vcpkg `baseline does not contain entry` / `git show versions/baseline.json failed` | vcpkg cloned `--depth 1`; manifest `builtin-baseline` commit not in history | Use a **full** vcpkg clone (`git fetch --unshallow`) |
| Link errors `fmt::v12::...` from openal-soft objects | Homebrew fmt 12 headers at `/opt/homebrew/include` shadow openal's vendored fmt 11 (include-order leak from another dep) | `target_include_directories(<alsoft targets> BEFORE PRIVATE <vendored fmt include>)` — see `cmake/openal.cmake` |
| Game renders nothing / D3D init fails (would have) | **Silent dependency fallback:** DXVK's meson found no SDL3.pc, silently compiled SDL2 WSI; game window is SDL3 | Generate `sdl3.pc` for the in-tree SDL3 and prepend to `PKG_CONFIG_PATH` for the DXVK meson run (`cmake/dx8.cmake`). **Verify**: `strings libdxvk_d3d9*.dylib | grep WsiDriver` must show `Sdl3WsiDriver` |

**Meta-lesson (recurring all day):** after every "successful" build of a plugin/dylib, verify the artifact with `strings`/`nm`/`otool -L` — silent fallbacks and stale binaries lie. Twice the packaging shipped stale dylibs because a build step failed mid-pipeline while the script kept going (`set -e` doesn't help across `grep` pipelines; check artifacts, not exit codes).

### Game assets (user owns the game on Steam; macOS has no depot)
- **SteamCMD downloads the Windows depot on macOS**: `+@sSteamCmdForcePlatformType windows +login <user> +force_install_dir <dir> +app_update 2732960 validate` (ZH = 2732960, base Generals = 2229870). Needs interactive Steam Guard.
- **SteamCMD-on-macOS Gatekeeper failure** (`Failed to load steamconsole.dylib` → Breakpad `code signature not valid... disallowed by system policy`): `xattr -dr com.apple.quarantine /opt/homebrew/Caskroom/steamcmd` + `codesign --force --deep --sign - <Breakpad.framework>`.
- **The depot's `ZH_Generals/` folder (1.5 GB) is the BASE GAME's data — the expansion requires it. Never filter it out.**
- Strip from the depot copy: `*.exe *.dll *.dat *.ico *.bmp *.doc *.lcf MSS/ Manuals/ steamapps/ RedistInstallers/ _CommonRedist/ *.txt 00000000.*` — Windows-only runtime files.
- The depot ships `Options_Helper/Options.ini` with `StaticGameLOD = High` — see §7 for why this matters enormously.

---

## 3. Phase 2 — Cross-compile every dependency for iOS

Strategy: prove each dependency for `arm64-ios` *standalone* before integrating; vcpkg classic mode (`cd /tmp && vcpkg install <pkgs> --triplet=arm64-ios`) is the cheap feasibility probe (note: vcpkg refuses package args in *manifest* directories).

| Dependency | Route | Gotchas |
|---|---|---|
| zlib, glm, gli, freetype, curl[ssl], openal-soft | vcpkg `arm64-ios` | gperf (host tool) needs `brew install autoconf autoconf-archive automake libtool` |
| fontconfig | **dropped on iOS** | Its dep libiconv fails autotools cross-detect (configure exit 77: same host/build triple, tries to run iOS binaries). Don't fight it — fontconfig exists for *system font discovery*, which iOS doesn't offer anyway. Replaced with bundled-font lookup (§4). Manifest: `"platform": "!windows & !ios"` |
| FFmpeg | vcpkg, **version override required** | Project baseline pinned 7.1.1 → fails on iOS (`tls_securetransport.c` partial-availability with `-Werror=partial-availability`). Override to 8.1.1 in `vcpkg.json` `overrides`; 8.x also needs an override entry for helper port `ffmpeg-bin2c` (absent from old baselines) |
| SDL3 | standalone CMake: `-DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_ARCHITECTURES=arm64 -DSDL_SHARED=ON -DSDL_STATIC=ON` | Trivial; SDL3's iOS support is first-class |
| DXVK 2.6 (d3d8+d3d9) | meson **cross file** (`cmake/meson-arm64-ios-cross.ini.in`) | `[host_machine] system = 'darwin'`; sysroot/min-version in `[built-in options]` args. SDL3 must resolve via `PKG_CONFIG_PATH` at `meson setup` or you get the silent SDL2 fallback again. Verify `LC_BUILD_VERSION platform 2` (`otool -l`) and `Sdl3WsiDriver` (`strings`) |
| MoltenVK | Two artifacts, two roles | **Static** `libMoltenVK.a` (Vulkan SDK xcframework `ios-arm64` slice) satisfies CMake `find_package(Vulkan COMPONENTS MoltenVK)` at link. **Dynamic** `MoltenVK.framework` (Khronos GitHub release `MoltenVK-ios.tar`, `dynamic/MoltenVK.xcframework/ios-arm64/`) is what DXVK `dlopen`s at runtime |

**Checking an artifact really targets iOS:** `otool -l <bin> | grep -A2 LC_BUILD_VERSION` → `platform 2`.

---

## 4. Phase 3 — Engine code changes for iOS (complete list)

Everything needed beyond "it compiles for macOS." Each was discovered by attempting the build/run and reading the failure.

### Build system
- **`PLATFORM_ID` generator expressions don't match iOS via `Darwin`** — audit `grep -rn "PLATFORM_ID"`; add `iOS` where intended (e.g. `$<$<PLATFORM_ID:Linux,Darwin,iOS>:SAGE_USE_FREETYPE>` — missing this made font code fall through to Win32 GDI paths → undeclared identifier errors).
- **FindVulkan on iOS needs explicit cache vars** (preset): `Vulkan_INCLUDE_DIR` (SDK headers are platform-neutral), `Vulkan_LIBRARY` + `Vulkan_MoltenVK_LIBRARY` → the ios-arm64 static lib, `Vulkan_MoltenVK_INCLUDE_DIR` → SDK include (component requires both lib *and* `MoltenVK/mvk_vulkan.h`).
- **Static MoltenVK pushes its frameworks onto the consumer**: link `Metal IOSurface CoreGraphics QuartzCore Foundation UIKit` (the macOS *dylib* resolved these itself — undefined `_MTL*`/`_IOSurface*`/`CAMetalLayer` symbols at link = this).
- **pkg-config framework flags poison CMake imported targets** (the nastiest one):
  - `pkg_check_modules(... IMPORTED_TARGET)` splits `-framework CoreFoundation` into two list items;
  - in `INTERFACE_LINK_OPTIONS` CMake then **de-duplicates token-wise**, mangling repeated entries into `-framework VideoToolbox CoreFoundation CoreMedia CoreVideo` → linker reads bare names as filenames;
  - fix: merge `-framework;X` pairs into single `"-framework X"` items and move them into `INTERFACE_LINK_LIBRARIES` (`cmake/ffmpeg_framework_fix.cmake`);
  - **trap:** imported targets from `pkg_check_modules` are **directory-scoped**, so `if(NOT TARGET PkgConfig::FFMPEG)` guards in sibling directories *re-create unfixed copies* — the fix must be applied after **every** `pkg_check_modules` call site.
- **SDL3_image + libpng**: no shared libpng exists for iOS and SDL3_image hard-rejects a static one (`"libpng16.a" is not a .dylib`). Set `SDLIMAGE_PNG_LIBPNG=OFF` (+`SDLIMAGE_PNG_SHARED=OFF`) — PNG still decodes via its stb and Apple ImageIO backends. Also guard any "force Homebrew libpng dylib" macOS hacks with `elseif(CMAKE_SYSTEM_NAME STREQUAL "iOS")` — a macOS dylib in an iOS link is a hard error.
- **Stale CMake cache bites**: options FORCE-set into cache by an earlier configure survive logic changes (`cmake -U "PNG_*" -U "SDLIMAGE*" <builddir>` to purge).
- iOS preset essentials (`CMakeUserPresets.json`): `CMAKE_SYSTEM_NAME=iOS`, `CMAKE_OSX_ARCHITECTURES=arm64`, `CMAKE_OSX_SYSROOT=iphoneos`, `CMAKE_OSX_DEPLOYMENT_TARGET=16.0`, `VCPKG_TARGET_TRIPLET=arm64-ios`, disable tools/extras/updater/crash-dumps, `PKG_CONFIG_PATH=""` in env (keep Homebrew out).

### Runtime code
- **Entry point** (`GeneralsMD/Code/Main/SDL3Main.cpp`):
  - `#include <SDL3/SDL_main.h>` on iOS (SDL wraps `main` in `UIApplicationMain`; without it the app never starts);
  - working directory: chdir to `<bundle>/GameData` if present (assets-in-bundle mode), else `$HOME/Documents` (dev mode). Engines that resolve assets CWD-relative make this the entire "VFS port";
  - `SDL_WINDOW_HIGH_PIXEL_DENSITY` on iOS for a native-resolution Metal drawable;
  - inject `-xres <W> -yres <H>` argv (from `SDL_GetWindowSizeInPixels`) so the internal render resolution matches the display exactly — fixes pillarboxing AND makes input mapping uniform (engine has resolution-aware font scaling in `GlobalLanguage`, so UI text stays sane);
  - `SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0")` — the gesture layer owns all synthesis;
  - `DXVK_STATE_CACHE_PATH` → `$HOME/Library/Caches` (purgeable, not backed up, not user-visible);
  - first-run seeding: copy bundled `DefaultOptions.ini` → user-data dir if absent.
- **dlopen on iOS resolves nothing by bare name.** Apps may only dlopen from their own bundle:
  - engine loading DXVK: `LoadLibrary("@executable_path/Frameworks/libdxvk_d3d8.0.dylib")` (`dx8wrapper.cpp`);
  - DXVK loading Vulkan: prepend `@executable_path/Frameworks/MoltenVK.framework/MoltenVK` to its loader list (`src/vulkan/vulkan_loader.cpp` in the local DXVK fork; harmless no-op on macOS);
  - *exception that confuses people*: dlopen by leaf name **does** succeed if a lib with that install name is already loaded in the process (how DXVK's WSI finds the game's SDL3).
- **DXVK's SDL WSI calls SDL via a runtime-loaded function-pointer table** (`SDL_PROC` list in `src/wsi/sdl3/wsi_platform_sdl3_funcs.h`), NOT direct linking. Adding any new SDL call (we needed `SDL_GetWindowSizeInPixels`) requires a table entry; a direct call = undefined symbol at link (no `-lSDL3` anywhere, by design).
- **High-DPI completeness**: with a high-density window, *every* size query in the present path must be in pixels. DXVK WSI `getWindowSize` switched `SDL_GetWindowSize` → `SDL_GetWindowSizeInPixels`; symptom of missing this = game renders 1:1 in the **corner** of the screen (points-sized swapchain in a pixels-sized layer).
- **Fonts without fontconfig** (`render2dsentence.cpp/h`): iOS implementation of the font locator → normalize face name (lowercase, strip spaces) → `fonts/<name>.{ttf,otf,ttc}` relative to CWD → fall back to `fonts/arial.ttf`. Ship Liberation fonts renamed (`LiberationSans→arial.ttf` etc.) — metric-compatible with Arial/Times/Courier, freely redistributable.
- **DXVK source patches need a local fork**: the superbuild pins a remote commit; edits to `_deps/` checkouts are disposable. `git clone <fork> references/fbraz3-dxvk && git checkout <pinned>`, build with `SAGE_DXVK_USE_LOCAL_FORK=ON`.

---

## 5. Phase 4 — Packaging, signing, deploying (no full Xcode project for the game)

The game builds with CMake/Ninja; only a thin shell needs Xcode. **The shell-app pattern:**

1. **XcodeGen** spec (`ios/project.yml`): app target, one stub `main.m`, `CODE_SIGN_STYLE: Automatic`, your `DEVELOPMENT_TEAM`, `TARGETED_DEVICE_FAMILY: "1,2"`, Info.plist keys: `UIFileSharingEnabled` + `LSSupportsOpeningDocumentsInPlace` (Files-app access for dev mode), landscape-only orientations, `UIRequiresFullScreen`, `UIApplicationSupportsIndirectInputEvents`, `CADisableMinimumFrameDurationOnPhone` (120 Hz).
2. `xcodebuild -allowProvisioningUpdates` builds the shell → valid signed bundle + provisioning profile, **without ever opening Xcode**.
3. Packaging script (`scripts/build/ios/package-ios-zh.sh`): copy shell app → **replace stub executable with the real game binary** → embed dylibs in `Frameworks/` (DXVK d3d8/d3d9, SDL3, SDL3_image, openal, gamespy) + `MoltenVK.framework` → `install_name_tool -add_rpath @executable_path/Frameworks` → rsync game assets into `GameData/` (skippable via `--dev` for 40 MB instead of 2.7 GB) → re-sign inside-out (`codesign` each dylib/framework, then the app with entitlements extracted from the shell: `codesign -d --entitlements - --xml`).
4. **Install-name matching matters**: embedded filename must equal the binary's load entry (e.g. binary wants `@rpath/libopenal.1.dylib` → rename `libopenal.1.24.2.dylib` on embed). Check both sides with `otool -L` / `otool -D`.

### devicectl crib sheet (Xcode 15+)
```bash
xcrun devicectl list devices
xcrun devicectl device install app  --device <UUID> <path/to/App.app>     # absolute path (shell cwd resets!)
xcrun devicectl device process launch --console --device <UUID> <bundle-id>
xcrun devicectl device info processes --device <UUID> | grep <name>
xcrun devicectl device info files --device <UUID> --domain-type appDataContainer \
     --domain-identifier <bundle-id> --subdirectory Documents
xcrun devicectl device copy to/from ... --domain-type appDataContainer ...   # push/pull app data
```
- **☠️ NEVER use `copy to --remove-existing-content true`** — it wiped the **entire app data container** (Documents *and* Library: assets, settings, saves), not just the destination path. There is no safe remote delete; prevent junk at the source, or have the app clean its own container with allow-listed `std::filesystem::remove_all` calls.
- Launch via icon vs devicectl differ: devicectl launches bypass the iOS **watchdog**; icon launches get killed if the main thread stalls during init. Keep first-frame time reasonable; suspect the watchdog when "runs from CLI, dies from icon."
- Device not in the provisioning profile (`0xe8008012`): one `xcodebuild -destination "platform=iOS,id=<UUID>" -allowProvisioningUpdates -allowProvisioningDeviceRegistration build` registers it and refreshes the profile; then repackage + install. New devices must be cable-paired + trusted + Developer Mode enabled first.
- **App icons on sideloaded apps**: compiled `Assets.car` + `CFBundleIconName` (+ auto-generated `CFBundleIcons~ipad`) is *correct but often not sufficient* — SpringBoard caches aggressively for dev-signed installs. The full unstick kit, in order: (1) loose `AppIcon60x60@2x.png`/`AppIcon76x76@2x.png`/`AppIcon83.5x83.5@2x.png` in the bundle root (always honored), (2) bump `CFBundleVersion`, (3) **restart the device** (what finally worked). Icon source: the game's own 256px `.ico` frame composited onto an opaque gradient (iOS icons cannot have alpha).

### Asset/data layout (the iOS-sanctioned shape)
- **Read-only game assets inside the signed bundle** (`App.app/GameData/`) — self-contained installs, honest storage accounting, atomic delete.
- **Saves/settings** → `Library/Application Support/...` (engine's user-data path already pointed there on Apple) — survives reinstalls, iCloud-backed.
- **Regenerable caches** (DXVK shader cache) → `Library/Caches` via `DXVK_STATE_CACHE_PATH`.
- **Nothing in Documents** in bundle mode; the app deletes legacy copies on first bundle-mode boot (allow-listed names only).
- Disable runtime debug logs for daily play (`dxvk.logLevel = none` in `dxvk.conf`, which DXVK reads from CWD — ship it in `GameData/`).

---

## 6. Phase 5 — Touch controls for a mouse-driven RTS

Architecture: translate touch → synthetic SDL mouse events injected through the **same code path real mice use** (`SDL3Mouse::addSDLEvent`). The game stays 1:1; only the input device is new. All code in `SDL3GameEngine.cpp` (iOS-guarded).

**The deferred-tap state machine** (`IDLE → PENDING → {tap | DRAGGING | LONGPRESSED | PAN}`) is the load-bearing design:
- **On finger-down, send NOTHING.** A premature LMB-down that gets "cancelled" later is still a real click to the game (our bug: every two-finger pan's first finger set rally points). Commit only when the gesture identifies itself:
  - finger up while `PENDING` → full tap: motion + LMB-down + LMB-up at the *original* touch point;
  - moved past dead zone (8 pt) → drag: LMB-down anchored at the original point, then motions (drag-select boxes anchor correctly);
  - second finger while `PENDING` → pan: RMB-drag at centroid (engine's camera scroll), **no left-click ever existed**;
  - held still 600 ms → long-press: pure RMB click (deselect).
- **Pinch** = wheel events every 6% distance change (camera zoom).
- **Long-press must be polled from the frame loop** — a stationary finger generates zero events, so an event-driven check never fires.
- **Synthetic events MUST carry a valid `windowID`** — the mouse layer looks the window up to scale window-points → internal-resolution coordinates and *silently skips scaling* on failure (symptom: taps land increasingly off toward screen edges).
- Defense in depth: also drop any `which == SDL_TOUCH_MOUSEID` events in the engine loop (no double delivery even if the hint fails).
- Tap-position rule: deliver down+up of a clean tap at the *same* point (press position), or dense UI buttons miss.

**App lifecycle** (same file): `SDL_AddEventWatch` for `WILL/DID_ENTER_BACKGROUND/FOREGROUND` (watcher, not poll — these can arrive after the loop stops); atomic flag gates `update()` to skip simulation *and presentation* while backgrounded (GPU work around suspension queues drawable-acquire timeouts that read as multi-second input hangs after resume); foreground/background mirror the desktop focus-lost/gained handling for mouse/audio state.

---

## 7. Phase 6 — Visual quality on modern hardware

- **2003-era GPU auto-detection is the #1 "looks worse than my PC" cause**: unknown GPU string (e.g. "Apple A19 Pro") → silently drops to Low LOD with `TextureReduction` (quarter-res textures). Steam itself ships an `Options.ini` forcing High — do the same: seed `IdealStaticGameLOD = High / StaticGameLOD = High / TextureReduction = 0` on first run.
- **Render at native panel resolution** (`SDL_WINDOW_HIGH_PIXEL_DENSITY` + matching internal `-xres/-yres`), only after confirming the engine scales fonts/UI with resolution (this one does: `GlobalLanguage::getResolutionFontSizeScale`, several methods incl. widescreen-aware "Balanced").
- **16× anisotropic filtering via the translation layer** (`d3d9.samplerAnisotropy = 16` in dxvk.conf) — RTS camera angles smear terrain with plain trilinear; aniso is the single biggest perceived-sharpness win and free on modern GPUs.
- First-use shader-compile hitches (menus) self-heal via DXVK's state cache; each new DXVK build invalidates it. Could pre-warm by baking a played-in cache into the bundle if it ever matters.
- Known upstream bug we inherited (not iOS-specific): some infantry render black (GeneralsX issue #88, deprioritized upstream). Candidate next fix.

---

## 8. Process & agent-workflow lessons (apply to ANY project like this)

1. **Research the ecosystem first.** An afternoon of fork archaeology converted a months-long port into a day-long one. Verify claims against artifacts (releases, CI configs), not READMEs.
2. **Climb the platform ladder**: Windows→macOS-ARM64→iOS. Each rung isolates a failure class (API portability / architecture / sandbox+lifecycle+signing).
3. **Trust no successful exit code.** Verify artifacts: `strings` for compiled-in drivers, `nm -u` for unresolved symbols, `otool -L/-l` for linkage and target platform, `lipo -info` for arch. The three silent failures of the day (SDL2-WSI fallback, stale dylibs shipped twice) all had green exit codes.
4. **Pipelines mask failures**: `build 2>&1 | grep -E "error"` exits with grep's status, and `&&`-chains continue past tools that "fail open." Make packaging scripts check that their inputs are newer than their sources (or verify content, as we did with `strings`).
5. **Shell hygiene for agents**: zsh aborts whole compound commands on glob misses (`rm x* && build` runs nothing if no `x*`); the working directory resets between tool calls after errors — **use absolute paths in anything important** (a relative path caused a misleading sandbox error during an iPad install).
6. **Long builds → background + notification; iterate on the log file.** Don't poll with sleeps; read the failure, fix, re-run.
7. **Fix problems at the layer you control**: vendored-dep header shadowing → include-order pin; un-forkable remote pins → local fork switch; pkg-config damage → post-process the imported target. Keep every fix in-tree and documented so upstream sync is possible.
8. **Distinguish "user must do" from "agent can do" early**: Steam login, device unlock/pair/trust, Developer Mode — front-load the ask so it overlaps with agent work.
9. **Write destructive-tool warnings into memory immediately** (the `--remove-existing-content` wipe is now permanently recorded). When recovery is possible, keep pristine sources on the host until the port is stable.
10. **A 2003 game on 2026 mobile silicon is GPU-trivial** — spend the budget on native resolution and filtering, not optimization.

---

## 9. File manifest (what was created/changed and why)

**In-tree (GeneralsX fork):**
| File | Purpose |
|---|---|
| `CMakeUserPresets.json` | `ios-vulkan` preset (CMAKE_SYSTEM_NAME=iOS, arm64-ios triplet, Vulkan/MoltenVK cache vars, tools off) |
| `cmake/meson-arm64-ios-cross.ini.in` | DXVK meson cross file (iPhoneOS sysroot) |
| `cmake/dx8.cmake` | sdl3.pc generation + PKG_CONFIG_PATH for DXVK meson; iOS cross-file selection; local-fork switch |
| `cmake/openal.cmake` | vendored-fmt include-order pin |
| `cmake/ffmpeg_framework_fix.cmake` | `-framework` pair merge for PkgConfig::FFMPEG (call after EVERY pkg_check_modules) |
| `cmake/sdl3.cmake` | iOS branch: SDLIMAGE_PNG_LIBPNG off (no Homebrew libpng) |
| `cmake/config-build.cmake` | iOS: static-MoltenVK framework deps |
| `Core/.../WW3D2/CMakeLists.txt` | SAGE_USE_FREETYPE for iOS; fontconfig/iconv skipped on iOS |
| `Core/.../WW3D2/render2dsentence.{h,cpp}` | iOS bundled-font locator |
| `Core/.../WW3D2/dx8wrapper.cpp` | iOS: dlopen DXVK from `@executable_path/Frameworks` |
| `GeneralsMD/Code/Main/SDL3Main.cpp` | SDL_main, bundle/Documents CWD, res injection, high-DPI flag, touch-hint, cache path, Options seeding, Documents cleanup |
| `GeneralsMD/.../SDL3GameEngine.cpp` | touch gesture state machine; lifecycle watcher + render gate; touch-mouse dedup |
| `references/fbraz3-dxvk/` | local DXVK fork @ pinned commit: vulkan_loader bundle paths; WSI `SDL_GetWindowSizeInPixels` (+ SDL_PROC table entry) |
| `vcpkg.json` | fontconfig `!ios`; ffmpeg for iOS + version overrides |
| `ios/project.yml`, `ios/Stub/` | XcodeGen shell app, Info.plist keys, asset catalog (AppIcon) |
| `scripts/build/ios/package-ios-zh.sh` | full packaging pipeline (see §5), `--dev` mode, icon PNG fallbacks |

**Host-side:** `~/GeneralsX/GeneralsZH` (game files + native macOS build), `~/GeneralsX/get-assets.sh` (SteamCMD fetch), `~/GeneralsX/ios-staging-config/{Options.ini,dxvk.conf}`, `~/GeneralsX/ios-staging/fonts/` (Liberation fonts renamed), `~/vcpkg`, `~/VulkanSDK/1.4.350.0`, `~/GeneralsX/MoltenVK` (dynamic framework — staged by `scripts/build/ios/fetch-moltenvk.sh`).

**Rebuild-from-scratch order:** macOS preset build → deploy script → verify with assets → `cmake --preset ios-vulkan` → `--target z_generals` → `package-ios-zh.sh` → `devicectl install`. Memory file `generals-ios-port-plan.md` (agent memory) holds current state + this file's location.

---

## §8 Post-ship bug hunts (June–July 2026) — the archaeology section

Three bugs found by playing on real devices after the port "worked". Each one is a
2003-era assumption meeting a 2026 platform. Failure mode → root cause → fix.

### 8.1 The black minimap (Generals Challenge only)

**Symptom:** minimap solid black — but only in Generals Challenge; skirmish fine.

**Root cause:** the engine queries D3D for a supported radar texture format and
falls back when the preference list all fails. On iOS, MoltenVK's caps query
reports NO radar format as supported, so **all three** radar textures (terrain,
overlay, shroud) take the fallback — which always returned `X8R8G8B8`, a format
with **no alpha channel**. The shroud (fog-of-war) layer must be transparent where
explored; opaque, it paints solid black over the whole map. Challenge matches
start fully shrouded, which is why only that mode showed it.

**Fix (`Core/.../W3DRadar.cpp`):** `findFormat()` takes a per-caller fallback —
`X8R8G8B8` for the opaque terrain layer, `A8R8G8B8` (alpha) for overlay and
shroud. Both universally supported on Vulkan-capable GPUs.

**Lesson:** when a modern translation layer fails a caps query wholesale, EVERY
texture in a subsystem rides the fallback path — a fallback written for one
"weird format" case becomes the main path, and its hidden assumptions (like
"nobody needs alpha here") become the bug.

### 8.2 The silent taunts / EVA lines (intermittent, mode-agnostic)

**Symptom (first report):** Challenge enemy taunts play once, then never again.
**Symptom (second report, weeks later):** EVA ("unit lost") silent in skirmish but
fine in Challenge — same build. Intermittent across sessions.

**Root cause, layer 1:** "uninterruptible" streamed speech sets a global
`disallowSpeech` flag so a speaker doesn't talk over himself, cleared when the
stream is detected stopped. A finished one-shot stream could linger "not stopped"
forever (layer 2), so the flag stuck and every later speech event was rejected
with `AHSV_NoSound`. Debug log from a real session: 65 speech events dispatched
at full volume, zero audible — while 17 music streams on the same code path
played fine (music never sets the flag).

**Root cause, layer 2 (the real one):** a drained OpenAL stream whose FFmpeg
decoder finished was restarted by the underrun-recovery guard, endlessly. The
stream never reached a stable AL_STOPPED, so the per-stream flag clear never
fired. Fixes landed in stages: report true EOF from the decoder
(`FFmpegFile::isAtEof`), latch `m_endOfData` so a finished stream is allowed to
stop, and a 15s backstop that force-clears a stuck flag.

**Lesson:** a global mutex-like flag cleared by "the audio stopped" inherits every
bug in stop-detection. Instrument the *dispatch* level (did the event fire, at
what volume) separately from the *device* level (did samples reach the mixer) —
the gap between them is where this class of bug lives.

### 8.3 The chirp (audible bug, found by ear)

**Symptom:** after an EVA line, a repeating "chirp" in the background, forever.
Reported by the player, not by any log.

**Root cause:** the §8.2 EOF latch had a hole. The latch relied on the decoder
reporting `isAtEof()` — but a decoder can fail *without* clean EOF (bad packet,
priming, non-audio frame). In that case the callback reported "more data coming"
forever, no data ever arrived, and the restart guard replayed the stream's
already-played buffer queue in a loop: the chirp. Same zombie stream also held
`disallowSpeech` (§8.2), so the chirp and the silence were one bug.

**Fix (`OpenALAudioStream.cpp`):** decode is synchronous — if a probe produces no
queue growth, waiting cannot help. Three *consecutive* no-growth probes latch
EOF (counter resets on any healthy refill, so transient hiccups over a long
track can never accumulate into a false stop).

**Lesson:** "end of stream" has two independent signals — what the decoder says
and what the buffer queue does. Trust their agreement; treat their disagreement
as termination with a bounded retry, never as "wait forever." And: a human ear
in the loop catches what logs structurally cannot — nothing logs a *sound*.
