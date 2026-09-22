# Scripts Directory

This folder is organized by function for easier maintenance and discovery.

XR public-identity check: `AAPT2=/path/to/aapt2 bash scripts/qa/xr-branding-test.sh`
verifies all compiled launcher/app labels, XR locale overlays, OpenXR naming,
unchanged package IDs, the unaffected Android flavor and the named APK export.

## Directory Structure

Game-data setup checks: `JAVA_HOME=/path/to/jdk bash scripts/qa/game-data-validator-test.sh`
runs synthetic archive/layout tests. Pass a Zero Hour folder and optional
Generals folder to additionally validate real files read-only. No retail data
is bundled with the tests. With the app installed and already configured on a
test device, build `:app:assembleXrDebugAndroidTest`, install the separate test
APK, and run `adb -s <serial> shell am instrument -w
com.generalsx.zerohour.xr.test/com.generalsx.zerohour.GameDataSetupSmoke`.
This smoke verifies live files, invalid selection/cancellation, locale resources,
and recreation without committing folders or changing language preferences.
It interrupts the running app; finish any match first. Remove the separate
`.xr.test` package after testing, never the main app or its data.

### `build/` - Build & Deployment Scripts per Platform

#### `build/linux/` - Linux & Docker Build
Scripts for Linux native and Docker-based builds:
- `run-lan-diagnostic-zh.sh /absolute/path/to/lab` - Run a separately staged
  native Zero Hour CRC diagnostic from a graphical PC terminal. Requires
  `runtime/GeneralsXZH` and its shared libraries, plus an independent Steam-layout
  game-data copy in `game/` (including `ZH_Generals`). Keeps engine settings,
  saves, DXVK caches and unique diagnostic logs under the lab; leaves `HOME`
  unchanged and refuses Steam-library/symlinked lab destinations. Enables
  `GX_LAN_SNAPSHOT=1` and disables optional SagePatch injection. This is a diagnostic
  comparator, not a replacement for retail Steam compatibility. See PLAN-025.
  Run `bash scripts/qa/lan-diagnostic-launcher-test.sh` for asset-free launcher
  preflight, quoting, isolation, unique-log and exit-status regression tests.
- `build-linux-appimage-generals.sh` - Package GeneralsX as AppImage (portable single-file Linux distribution)
- `docker-configure-linux.sh` - Configure CMake for Linux build
- `docker-build-linux-zh.sh` - Build GeneralsXZH (Zero Hour) for Linux
- `docker-build-linux-generals.sh` - Build GeneralsX (base game) for Linux
- `docker-build-mingw-zh.sh` - Cross-compile Windows .exe via MinGW in Docker
- `build-linux-appimage-zh.sh` - Package GeneralsXZH as AppImage (portable single-file Linux distribution)
- `bundle-linux-zh.sh` - Bundle compiled binaries
- `deploy-linux-zh.sh` - Deploy to runtime directory
- `run-linux-zh.sh` - Launch the game windowed

#### `build/macos/` - macOS Build
- `build-macos-zh.sh` - Configure + build GeneralsXZH
- `build-macos-generals.sh` - Configure + build GeneralsX
- `bundle-macos-zh.sh` - Bundle app
- `bundle-macos-generals.sh` - Bundle app
- `deploy-macos-zh.sh` - Deploy binaries
- `deploy-macos-generals.sh` - Deploy binaries
- `run-macos-zh.sh` - Launch the game

#### `build/windows/` - Windows Build (Pending)
Reserved for modern Windows toolchain (VS2022 + SDL3 + DXVK + OpenAL)

### `env/` - Environment Setup

#### `env/docker/` - Docker Configuration
- `docker-build-images.sh` - Build pre-configured Docker images (Linux + MinGW)
- `docker-install.sh` - Docker environment validation

#### `env/cache/` - Compiler Cache
- `setup_ccache.sh` - Configure ccache (GCC/Clang)
- `test_ccache.sh` - Test ccache functionality
- `setup_sccache.ps1` - Configure sccache (Windows)
- `test_sccache.ps1` - Test sccache functionality

### `tooling/` - Code Analysis & Utilities

#### `tooling/clang-tidy/` - Custom clang-tidy Plugin
- `plugin/` - Custom clang-tidy checks source (C++ checks for AsciiString, Singleton patterns)
- `run.py` - Unified clang-tidy runner with batch processing and quiet output

#### `tooling/cpp/maintenance/` - C++ Code Maintenance
Utilities for large-scale code refactoring and fixes:
- `fix_*.py` - Targeted fixes (matrix conversions, water rendering, noise, debug logging, Windows API)
- `monitor-dxvk-build.py` - DXVK build monitoring tool
- `*_refactor_*.py` - Code transformation scripts (string classes, etc.)
- `remove_*.py` / `replace_*.py` - Include guard and pragma cleanup
- `unify_move_files.py` - Move files between Generals/GeneralsMD/Core with CMakeLists.txt updates

### `qa/` - Quality Assurance & Testing

- `xr-uniform-cache-test.sh` / `.cpp` - Extracts the production uniform
  keys, ProgramInfo, uniform application and GL-cache invalidation. Compares
  the legacy global policy with XR per-program caching on real GLES, including
  true multiview, changing matrices/materials/lights, independently absent
  material uniforms and foreign GL mutation. Checks reflected GPU values and
  both eye pixels; reports upload counts and a bounded submission benchmark,
  not campaign FPS. Run with
  `CXX=<NDK aarch64-linux-android29-clang++> bash scripts/qa/xr-uniform-cache-test.sh <fresh-output-dir>`;
  push/run `uniform-cache-test` on Quest. No game assets or GPU timer queries.

- `xr-multiview-device-test.sh` / `.cpp` - Extracts actual stereo allocation,
  destruction, coverage publication and array-texture getters. Tests the shared
  multiview shader contract and fallback on a real OVR_multiview2 EGL device.
  Run `CXX=<NDK aarch64-linux-android29-clang++> bash scripts/qa/xr-multiview-device-test.sh <fresh-output-dir>`;
  push/run `multiview-test` on Quest. No game assets or GPU timer queries.

- `xr-world-copy-test.sh` / `.cpp` - Extracts the production XR frame boundary,
  full/partial color clear, native Present publication, UI snapshot/MRT setup
  and composed/planar getters. P17 checks missing-world suppression, empty or
  partial Present refusal, full-clear recovery and forced-copy rejection.
  Real GLES verifies copy
  counts, same-frame readiness, identical composed/UI pixels, stale-image
  suppression, late stereo loss and recovery. Includes a small 1280x720 copy
  benchmark; it is not game FPS. Run with `CXX=<NDK aarch64-linux-android29-clang++>
  bash scripts/qa/xr-world-copy-test.sh <fresh-output-dir>`, then push/run the
  resulting `world-copy-test` on an Android EGL device. No game assets needed.

- `xr-scene-test.sh` / `.cpp` - P19 production room geometry, async query
  ownership/failure, plinth support, optional fitting and exclusive preview
  input. Host C++17/UBSan; no scene permission or real room data required.
  Run `bash scripts/qa/xr-scene-test.sh [configured-android-build]`; CXX optional.
- `xr-hover-info-test.sh` / `.cpp` - Extracts the actual native tooltip
  evaluator and tests discounts, purchase restrictions, prerequisites, upgrades
  and science-point costs without creating UI or changing gameplay. Host only.
  Run `bash scripts/qa/xr-hover-info-test.sh`; CXX optional.
- `xr-build-controls-test.sh` / `.cpp` - P18.1 actual native building-preview
  rotation adapter plus physical hand mapping and editing-outline geometry.
  Includes missing/replaced ghost, line-build, tracking ownership, modal,
  trigger freeze, neutral stick rearm and DE/EN guide cases. Run with
  `bash scripts/qa/xr-build-controls-test.sh [configured-android-build]`.
  Real placement legality/order execution still requires a worn-headset game.
- `xr-height-test.cpp` - P18 stable map datum, constant plinth/underside,
  explicit zoom/scale, close-zoom terrain bounds, and render/ray agreement
  across panning, yaw, tilted boards and map elevations. Build C++17 with
  `-IGeneralsMD/Code/Main -ICore/Libraries/Source/d3d8gles/include` and the
  configured OpenXR include directory. Runs on host (UBSan) or Android NDK.
- `xr-height-bridge-test.sh` / `.cpp` - Extracts the actual P18 mapping
  adapter from XrGameBoot.cpp. Spies deliberately offer no terrain-pick or
  camera-write methods: panning/height changes cannot accidentally query or
  mutate them. Tests map replacement, cinematic gates and invalid extents.
  Run `bash scripts/qa/xr-height-bridge-test.sh [configured-android-build]`.
  Multiview/world-device tests also exercise the production raised-terrain
  and feedback clipping shaders on both Reference and Multiview paths.

- `xr-stereo-state-test.sh` / `.cpp` - Extracts production stereo draw dispatch,
  fixed-state application/key and D3D converters. Real GLES compares narrow
  restoration against the old full restoration over 1024 scenarios: state,
  both eyes, ordinary target, following draw and dispatch/cache counters.
  Run `CXX=<NDK aarch64-linux-android29-clang++> bash scripts/qa/xr-stereo-state-test.sh <fresh-output-dir>`;
  push/run the resulting `stereo-state-test` on an Android EGL device. No game
  assets or OpenXR session. P13 also compares separate targets with an eye
  atlas, tests the actual XR compositing shaders against cross-eye filtering,
  and prints a small target-switch benchmark at Balanced resolution. Its
  timings are not game FPS and do not establish an atlas performance win.
  P15 additionally compares true multiview and injected shader-failure layer
  fallback, and checks direct array-layer compositing without eye bleed.
  P17 compares both modes again with ordinary-world omission, verifies exact
  dispatch-count savings and identical eye pixels/state. The existing atlas
  benchmark does not measure P17's game-frame speedup.

- `audio-cache-test.sh` / `.cpp` - Extracts actual cache acquisition, release,
  eviction and sample-buffer detachment methods. A fixture decoder/filesystem
  and source-aware fake AL driver check shared leases, failed loads, bounded
  memory, priority eviction and 300 forced attack/sound/decay transitions.
  Run `bash scripts/qa/audio-cache-test.sh <fresh-output-dir>` (host, UBSan).
- `audio-cache-device-test.cpp` - Uses the extracted `audio-release.inc` from
  that directory with real OpenAL in a silent loopback context. Compile with
  the NDK C++17 compiler, `-static-libstdc++`, the output-directory and vcpkg
  include paths; link the Android vcpkg `libopenal.a`, `-lOpenSLES -llog -ldl
  -lm`. Push/run on Android. Checks that stopped-but-bound buffers cannot be
  deleted and that the production detachment helper permits safe eviction.

- `xr-performance-test.cpp` - Session-only P12 shadow A/B, warm-up epochs and
  production asynchronous timer-query ring with host driver spies. Compile
  like the standalone XR tests (C++17, UBSan, Main + OpenXR include paths).
  Checks unavailable APIs, full ring, nonblocking availability, disjoint before
  and during readout, profile isolation, cancelled movies, overflow and teardown.
- `xr-shadow-scope-test.sh` / `.cpp` - Compiles actual `XrGameBoot_Frame` with
  engine spies. Both profiles and non-XR preserve all original shadow flags
  after normal/exceptional exits and execute simulation exactly once. Run
  `bash scripts/qa/xr-shadow-scope-test.sh [configured-android-build]`.
- `xr-performance-device-test.cpp` - Real GLES3/EGL timer extension discovery,
  asynchronous query completion and cleanup on Adreno. NDK command is in the
  file header. No game data, OpenXR session or frame-rate claim; fixture-only
  flush/sleeps provide GPU progress without `glFinish`.

- `initial-language-test.sh` / `InitialLanguageTest.java` - Compiles the actual
  shared Java first-run policy without Android runtime. Tests DE regional/script
  tags, other/unknown locales and missing German text. Set `JAVA_HOME` to a JDK;
  run `bash scripts/qa/initial-language-test.sh`. Saved XR language precedence
  is covered by `xr-workspace-test.sh`.

- `xr-loading-test.cpp` - Production loading-frame ownership and neutral-gated
  video skip policy. Compile like the standalone XR fixtures. Checks balanced
  begin/end, failure paths, reentrancy, inactive sessions and held launch input.
- `xr-loading-presenter-test.sh` / `.cpp` - Extracts the actual synchronous
  loading presenter from `XrHello.cpp` and compiles it against OpenXR spies.
  Checks current predicted eye poses, passthrough/projection ordering, empty
  frames when rendering is suppressed, invalid tracking, session stop, cleanup
  and no double-ending the interrupted outer frame. Run with
  `bash scripts/qa/xr-loading-presenter-test.sh [configured-android-build]`.

- `xr-trigger-bridge-test.sh` / `.cpp` - Extracts the production spatial trigger
  bridge verbatim and compiles it with engine spies under UBSan. Tests drag
  threshold, release-only selection, additive gestures, armed-command priority,
  queue handling and cancellation. Run with host clang++ and a configured
  Android build's OpenXR headers; does not require or modify a device.

- `xr-pick-bridge-test.sh` / `.cpp` - Compiles production W3D `pickDrawable`
  verbatim with engine spies under UBSan. Checks spatial versus ordinary GUI
  refusal, terrain occlusion and model hits without ground. Run with
  `bash scripts/qa/xr-pick-bridge-test.sh`; no device or game assets required.
- `lan-snapshot-test.sh` - UBSan tests of the universal bounded LAN snapshot
  emitter and production command adapter, plus strict paired-comparator fixtures.
  Tests activation/offline gates, ring overwrite, all object fields, typed argument
  padding, late mismatch, truncation, freeze/reset and incomplete input.
- `lan-snapshot-compare.py quest.log pc.log` - Compare complete universal snapshot
  dumps; use `--match-a N` / `--match-b N` for multi-match logs. Exit 0 only means
  retained observations agree, 1 means a difference, 2 is incomplete/incompatible.
  Reports object/type/field/global/RNG differences with execution-frame command
  context. See `docs/WORKDIR/planning/PLAN-025A_UNIVERSAL_DESYNC_SNAPSHOT.md`.
- `lan-crc-trace-test.sh` / `.cpp` - Compiles the opt-in LAN CRC observer with
  UBSan and warnings-as-errors. Tests marker/environment activation, offline
  silence, reset/new match, bounded generation/validation output and one-shot
  missing/different failure records. Source guards check the existing CRC
  cadence, message and traversal sites; these are not proof of full-engine
  CRC identity or retail multiplayer compatibility. Run with
  `bash scripts/qa/lan-crc-trace-test.sh`; no device or retail assets required.
- `lan-crc-detector-test.sh` / `.cpp` - Exercises the same pure network-slot
  evaluator called by the Zero Hour production validator, under UBSan. Includes
  non-identity player/slot indices, equal/different/zero CRCs, missing peers,
  stale disconnected entries and ambiguous mappings. Dispatcher/replay guards
  are source-level checks, not an end-to-end network simulation.
- `lan-crc-compare.py` - Offline comparison of two instrumented peers' generation
  records: `python3 scripts/qa/lan-crc-compare.py quest.log pc.log`. Requires
  matching map CRC, seed and interval; explicitly select `--match-a N` and/or
  `--match-b N` (one-based) for multi-match logs. Reports first observed rolling
  checkpoint difference, refuses missing/incompatible input, and does not
  equate validation frames with generation frames. Exit codes: 0 recorded
  samples agree, 1 difference, 2 inconclusive. Agreement is not full-match
  acceptance or proof that the builds/data match. Run parser/comparison tests
  with `python3 scripts/qa/lan-crc-compare-test.py`.
- `xr-workspace-test.sh` / `.cpp` - Compiles production native-dialog detection
  and video/loadscreen split eligibility plus surface crop/placement functions
  with spies. Checks science-tree expansion, full-screen video fallback, complete dialog corner
  hits, compact/full bottom-edge continuity, photo defaults applied once, v8
  preference validation and stereo extent bounds. P12.1 also compiles the actual
  cinematic/camera predicates, favorite/rotation methods and view-status text;
  verifies they do not cancel a scripted path after its native lock expires.
  Run with
  `bash scripts/qa/xr-workspace-test.sh [configured-android-build]`.
- `xr-tactical-bridge-test.sh` / `.cpp` - Compiles production formation,
  guard/escort, force-move, cancellation and map-bookmark functions with spies.
  Checks native message arguments, all modifier-state restoration combinations,
  invalid targets, unavailable selection, empty slots and session reset.
- `xr-console-bridge-test.sh` / `.cpp` - Compiles actual group/communicator/
  native-language bridge functions with spies. Tests all ten slots, empty
  groups, locked input, text-pack validation and atomic marker updates, plus
  the German/English catalog. Run with bash and a configured Android build.
- `xr-panel-text-test.sh` / `.cpp` - Compiles actual custom-panel text generation,
  checks localized labels and fixed hit-layout payload slots, and emits
  NUL-delimited Canvas fixture payloads. No game assets or device needed.
- `xr-comfort-test.cpp` - Pure handedness-role mapping, grazing-ray click/drag
  intent, board-basis pan speed/direction and atomic v1–v6 layout migration.
  Compile like the other standalone XR host/ARM64 fixtures; pass a temporary
  layout filename as its only argument (the fixture writes/removes that file).

- `xr-terrain-device-test.sh` / `.cpp` - Extracts the production terrain
  `Cast_Ray` verbatim and links actual WWMath against a flat-height fixture.
  Tests inside/outside bounding-box endpoints and true misses on Android
  ARM64. Set `ANDROID_NDK_HOME`; the shell script prints the test executable
  for manual push/run using an explicit device serial. No game assets needed.
- `xr-input-test.cpp` - Production XR input router with engine spies: world/UI
  ownership, release-only commits, cancellation on crossing or tracking loss,
  no duplicate camera keys/wheel, ordinary pointer fallback, visible rays on
  misses. On-demand menu capture is tested separately. Compile with
  the same Main/OpenXR include paths as the other host XR tests.
- `xr-menu-test.cpp` / `xr-menu-routing-test.cpp` - Shared workspace button
  geometry and production actions/ray routing: release-only activation,
  background capture, focus loss, window tilt and bounded map/table scale.
  Use the Main/OpenXR include paths; no Java or graphics context is needed.
  P10 extends these checks to the nonmodal command console, ten direct groups,
  one-shot group modifiers and click-origin isolation.
- `xr-tactics-test.cpp` - Production spatial selection rectangle, native
  selection-cap policy and mode/waypoint cancellation. Does not simulate a
  battle; actual order execution still needs the game/headset acceptance test.
- `xr-board-test.cpp` - Sampled cut-face mesh, finite bounds, closed base trim,
  vertex layout and maximum decoration budget. Use Main/OpenXR includes.
- `xr-world-test.cpp` - World-to-board mapping, expanded map coverage/culling, uniform height/scale,
  rotation, invalid-input rejection, per-eye projection, stencil policy and
  spatial-ray clipping, beam endpoint geometry and bounded HQ eye extents.
  Include `Core/Libraries/Source/d3d8gles/include`
  as well as Main/OpenXR when compiling this test.
- `xr-world-device-test.cpp` - Standalone Android GLES test using the production
  projection/volume-clip shader snippets with synthetic P6 geometry. Checks
  stereo, depth, clipping, camera-space particles, stencil volume/fill and
  unchanged MR coverage under shadows; optional PPM capture.
  P9 also compiles the exact decoration shaders and checks frame alpha,
  feedback clipping and depth occlusion against the same eye-like target.
  Requires the d3d8gles include directory; see its header. This does not test
  the complete engine draw path or actual headset presentation.
- `xr-diorama-test.cpp` - Host checks for P6 stereo height, uniform board scale,
  common-anchor invariance and mesh validity; supports UBSan.
- `xr-diorama-device-test.cpp` - Standalone Android GLES test of the production
  P6 mesh/shaders, stereo pixel differences and depth occlusion. Optional new
  PPM output path captures a stereo pair. Build instructions are in its header.
- Other `xr-*-test.cpp` files cover projection, placement/layout persistence,
  controller routing, camera profiles, split layers, color transfer and MRT.
  See PLAN-023 in `docs/WORKDIR/planning/` for the acceptance boundaries.

#### `qa/smoke/` - Smoke Tests
- `docker-smoke-test-zh.sh` - Quick startup validation (expects crash, checks init output)
- `run-bundled-game.sh` - Test bundled binary after deployment
- `collect-flatpak-vulkan-wsi-report.sh` - Collect reproducible Flatpak Vulkan/XCB diagnostics for upstream runtime issues

### `legacy/` - Deprecated & Compatibility

#### `legacy/compat/` - Old Scripts
- `docker-build.sh` / `dockerbuild.sh` - Deprecated Docker wrappers
- `apply-patch-13-manual.sh` - Historical patch utility
- `promote-linux-attempt-to-main.sh` - Promotion helper (legacy)

### Deprecated: `cpp/` and `clang-tidy-plugin/`
Backward-compatibility wrappers. See their README files for migration info.

---

## Quick Start

### Docker Prerequisites

```bash
# Check Docker installation
docker --version

# macOS: Install Docker Desktop
brew install --cask docker
```

### First Linux Build

```bash
# 1. Build Docker images (one-time, ~5-10 min)
./scripts/env/docker/docker-build-images.sh all

# 2. Configure
./scripts/build/linux/docker-configure-linux.sh

# 3. Build
./scripts/build/linux/docker-build-linux-zh.sh

# 4. Smoke test (will crash; check logs)
./scripts/qa/smoke/docker-smoke-test-zh.sh

# 5. Deploy
./scripts/build/linux/deploy-linux-zh.sh

# 6. Run
./scripts/build/linux/run-linux-zh.sh -win

# 7. Optional: build AppImage package
./scripts/build/linux/build-linux-appimage-zh.sh linux64-deploy

# 7b. Optional: build AppImage package for base Generals
./scripts/build/linux/build-linux-appimage-generals.sh linux64-deploy

# 8. Optional: run AppImage with explicit asset paths
CNC_GENERALS_ZH_PATH="/path/to/GeneralsZH_or_GeneralsMD" \
CNC_GENERALS_PATH="/path/to/Generals" \
./build/GeneralsXZH-linux64-deploy-x86_64.AppImage -win
```

### macOS Build

```bash
# All-in-one (configure + build + deploy + run)
./scripts/build/macos/build-macos-zh.sh

# Or step-by-step:
./scripts/build/macos/build-macos-zh.sh --build-only
./scripts/build/macos/deploy-macos-zh.sh
./scripts/build/macos/run-macos-zh.sh -win
```

### Windows Cross-Compile (from Linux/macOS)

```bash
# Build Windows .exe via MinGW in Docker
./scripts/build/linux/docker-build-mingw-zh.sh

# Output: build/mingw-w64-i686/GeneralsMD/GeneralsXZH.exe
# Test in Windows VM or Wine
```

---

## Docker Workflow

### Image Management

**`docker-build-images.sh [linux|mingw|all]`**

Builds pre-configured Docker images with all dependencies pre-installed (vcpkg, CMake, toolchains).

```bash
# Build both images (one-time setup, ~5-10 minutes)
./scripts/env/docker/docker-build-images.sh all

# Build specific image
./scripts/env/docker/docker-build-images.sh linux
./scripts/env/docker/docker-build-images.sh mingw
```

**Benefits**:
- ✅ 40-50% faster builds (no package installation per build)
- ✅ vcpkg shared volume (`~/.generalsx/vcpkg`)
- ✅ Image sizes: ~90MB (Linux), ~660MB (MinGW)
- ✅ Auto-detection (all build scripts check/build images if missing)

### Environment Variables

Scripts support customization:

```bash
# Custom Docker image base
export DOCKER_IMAGE="ubuntu:24.04"
./scripts/build/linux/docker-build-linux-zh.sh

# Flatpak PoC: inject newer libxcb/X11 libs from an external directory
export LIBXCB_POC_DIR="$PWD/flatpak/poc-libxcb"
./scripts/build/linux/build-linux-flatpak.sh linux64-deploy GeneralsMD

# Custom log directory
export LOG_DIR="my-logs"
./scripts/build/linux/docker-build-linux-zh.sh

# Verbose output
export VERBOSE=1
```

---

## VS Code Tasks

All scripts are integrated into VS Code tasks (Cmd+Shift+P → "Tasks: Run Task"):

| Task | Script | Description |
|------|--------|-------------|
| [Linux] Configure (Docker) | `build/linux/docker-configure-linux.sh` | Configure CMake |
| [Linux] Build GeneralsXZH | `build/linux/docker-build-linux-zh.sh` | Build Zero Hour |
| [Linux] Build GeneralsX | `build/linux/docker-build-linux-generals.sh` | Build base game |
| [Linux] Deploy GeneralsXZH | `build/linux/deploy-linux-zh.sh` | Deploy binaries |
| [Linux] Run GeneralsXZH | `build/linux/run-linux-zh.sh -win` | Launch game |
| [macOS] Build GeneralsXZH | `build/macos/build-macos-zh.sh` | Build + deploy + run |
| [macOS] Build GeneralsX | `build/macos/build-macos-generals.sh` | Build base game |
| [macOS] Deploy GeneralsXZH | `build/macos/deploy-macos-zh.sh` | Deploy binaries |
| [macOS] Deploy GeneralsX | `build/macos/deploy-macos-generals.sh` | Deploy binaries |
| [macOS] Bundle GeneralsXZH | `build/macos/bundle-macos-zh.sh` | Bundle app (.app + zip) |
| [macOS] Bundle GeneralsX | `build/macos/bundle-macos-generals.sh` | Bundle app (.app + zip) |
| Validate: Check Docker Prerequisites | Verify Docker works | Pre-flight check |

---

## Troubleshooting

### "Docker not found"
```bash
# macOS
brew install --cask docker

# Start Docker Desktop and verify
docker --version
```

### "Permission denied" on scripts
```bash
# Make all scripts executable
find . -name "*.sh" -exec chmod +x {} \;
```

### "Preset not found"
```bash
# List available CMake presets
grep '"name"' CMakePresets.json
```

### Build errors or lingering state
```bash
# Check most recent build log
cat logs/build_zh_*.log | tail -50

# Clean build (remove cached objects)
rm -rf build/linux64-deploy
./scripts/build/linux/docker-configure-linux.sh
./scripts/build/linux/docker-build-linux-zh.sh
```

### Docker image issues
```bash
# List Docker images
docker images | grep generalsx

# Remove and rebuild images
docker rmi generalsx/linux-builder:latest generalsx/mingw-builder:latest
./scripts/env/docker/docker-build-images.sh all
```

---

## Notes

- **No brew/apt installs** during build: Docker containers pre-install all dependencies
- **Logs auto-created**: `logs/` directory created automatically with descriptive names
- **Verbose output**: All commands echo to terminal + log file
- **Error exit**: Scripts use `set -e` (stop immediately on first error)
- **Backward compatibility**: Old script paths (`scripts/run-clang-tidy.py`, `scripts/cpp/`) still work; forward compatibility maintained
- **macOS bundle dylibs**: macOS bundle scripts include non-system linked dylibs discovered via `otool -L` (including Homebrew paths when linked)
- **macOS bundle toggle**: set `GX_BUNDLE_INCLUDE_EXTERNAL_DYLIBS=0` to disable external dylib scanning when producing smaller local test artifacts

---

## For More Information

- **Build System**: See [CMakePresets.json](../CMakePresets.json)
- **Phase 1 Details**: See [docs/WORKDIR/phases/PHASE01_IMPLEMENTATION_PLAN.md](../docs/WORKDIR/phases/PHASE01_IMPLEMENTATION_PLAN.md)
- **Docker Workflow**: See [docs/WORKDIR/support/DOCKER_WORKFLOW.md](../docs/WORKDIR/support/DOCKER_WORKFLOW.md)
- **Instructions**: See [.github/instructions/scripts.instructions.md](../.github/instructions/scripts.instructions.md)
