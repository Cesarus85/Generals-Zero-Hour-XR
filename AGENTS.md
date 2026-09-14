# GeneralsX: Instructions for AI Coding Agents

## What I Am
GeneralsX is a cross-platform port of Command & Conquer: Generals Zero Hour, porting
legacy Windows DirectX 8 + Miles Sound code to a modern stack (SDL3 + DXVK + OpenAL +
64-bit). This fork's active focus is **Android** — including GeneralsOnline, a
from-scratch NGMP-based online multiplayer backend replacing the long-dead GameSpy
servers (lobby, custom match, quickmatch, persona/stats, social) — with macOS and
iOS/iPadOS also fully working but not currently receiving the same volume of new
work. This is a **massive C++ game engine** (~500k LOC) preserving retail gameplay
while modernizing the platform layer.

## Must-Load Context
Before starting work, read:
- `.github/copilot-instructions.md` – quick reference
- `.github/instructions/generalsx.instructions.md` – full architecture
- `.github/instructions/git-commit.instructions.md` – commit standards
- `.github/instructions/docs.instructions.md` – documentation workflow
- `docs/port/ANDROID_PORT.md` – Android port + GeneralsOnline multiplayer: architecture, device matrix, bring-up log
- `docs/DEV_BLOG/YYYY-MM-DIARY.md` – current development notes

## Key Entry Points
- `GeneralsMD/Code/Main/SDL3Main.cpp` – Android/Linux/macOS/iOS entry point
- `Generals/Code/Main/WinMain.cpp`
- `Core/GameEngineDevice/Source/`
- `GeneralsMD/Code/GameEngine/Source/GameNetwork/GeneralsOnline/` – multiplayer backend client
- `android/` – Gradle shell app (SDLActivity), Setup/FolderPicker/LogViewer/GeneralsOnline-account activities
- `GeneralsMD/Code/GameEngineDevice/Source/SDL3GameEngine.cpp` – touch gesture state machine
- `GeneralsMD/Code/GameEngineDevice/Source/SDL3Device/GameClient/TouchInput.cpp` – what a gesture means, resolved by the engine's own rules

## Platform Focus
- **Active**: Android (`android-vulkan`) — primary target, most real-device testing
- **Also maintained**: macOS (`macos-vulkan`), iOS/iPadOS (`ios-vulkan`), Linux (`linux64-deploy`)
- **Future/Exploratory**: Windows (MinGW path, issue #29)
- **Legacy**: VC6 + DirectX 8 + Miles (reference only)

## Architecture
| Layer   | Technology          | Replaces                     |
|---------|---------------------|------------------------------|
| Graphics| DXVK                | DirectX 8 (d3d8.dll)         |
| Windowing| SDL3              | Win32 API                    |
| Audio   | OpenAL              | Miles Sound System           |
| Video   | FFmpeg              | Bink Video (intro/videos)    |
| Platform| SDL3 + libc         | Win32 POSIX calls            |

**CRITICAL**: Platform code must be isolated to `Core/GameEngineDevice/` and `Core/Libraries/Source/Platform/`. No native Win32/Cocoa/X11 calls in game logic.

## Golden Rules
1. **Single codebase** – Linux and macOS build from same source
2. **SDL3 everywhere** – No native platform calls in game code
3. **DXVK everywhere** – DX8 → Vulkan translation on all platforms
4. **OpenAL everywhere** – Cross-platform audio stack
5. **64-bit native** – x86_64 only (32-bit via VC6 upstream)
6. **Retail compatibility** – Original replays and mods must work
7. **Determinism** – Rendering/audio changes must not affect gameplay logic
8. **No band-aids** – Fix underlying issues, not symptoms
9. **Update dev blog** – `docs/DEV_BLOG/YYYY-MM-DIARY.md` before committing
10. **Reference repos** – Study patterns, don't copy-paste

## Reference Repositories
- **fighter19-dxvk-port** – Primary graphics/platform reference (DXVK + SDL3 on Linux)
- **jmarshall-win64-modern** – Audio reference (OpenAL implementation, Generals-only)
- **thesuperhackers-main** – Upstream baseline for regression checks

## Build Commands

### Android (no local toolchain needed)
Push to a `claude/**` branch or trigger manually: **Actions tab → Build Android →
Run workflow**. CI builds `libmain.so` + DXVK, packages a signed APK, verifies
`DT_NEEDED`/ABI. For a local build see `docs/port/ANDROID_PORT.md §3`:
```bash
git submodule update --init references/fbraz3-dxvk
export ANDROID_NDK_HOME=~/Android/Sdk/ndk/<version>
./scripts/build/android/build-android-zh.sh
./scripts/build/android/package-android-zh.sh --install
```

### Docker (recommended on Linux host)
```bash
# Linux build
./scripts/build/linux/docker-configure-linux.sh linux64-deploy
./scripts/build/linux/docker-build-linux-zh.sh linux64-deploy

# Optional: Windows via MinGW cross-build
./scripts/build/linux/docker-build-mingw-zh.sh mingw-w64-i686
```

### Native Linux
```bash
cmake --preset linux64-deploy
cmake --build build/linux64-deploy --target z_generals
```

### Native macOS
```bash
cmake --preset macos-vulkan
cmake --build build/macos-vulkan --target z_generals
```

## Target Priority
1. **GeneralsXZH** (Zero Hour) – Primary target, most feature-complete
2. **GeneralsX** (Base game) – Backport only when changes are clearly shared

## Backport Rules
**Backport to Generals when:**
- Change is platform/backend code (SDL3, DXVK, OpenAL)
- Change is in shared Core libraries
- Change is low-risk and clearly applicable

**Do NOT backport:**
- Zero Hour-specific gameplay/logic
- Expansion-specific features
- High-risk changes to Zero Hour

## DXVK Source of Truth (macOS)
- Default: GitHub fork branch `generalsx-macos-v2.6` (auto-update enabled)
- Local mode: `-DSAGE_DXVK_USE_LOCAL_FORK=ON`
- **Rule**: Never edit files in `build/_deps/...` directly. Always commit fixes in fork repo first.

## Common Pitfalls
- **Linux case sensitivity**: Include paths must match exact case. Use `scripts/tooling/cpp/fixIncludesCase.sh`.
- **DXVK needs Vulkan**: Install `vulkan-tools`, `mesa-vulkan-drivers` or GPU drivers.
- **-logToCon only in debug**: Available only with `RTS_BUILD_OPTION_DEBUG=ON`.
- **SDL3 from source**: Fetched via CMake FetchContent. No system package needed.
- **Manual memory**: Always delete/delete[]. Use STLPort for VC6 legacy builds.
- **Debug options break replays**: Use `RTS_BUILD_OPTION_DEBUG=OFF` for replay tests.
- **Touch input is not a mouse**: do NOT fix a control bug by synthesizing
  `MSG_RAW_MOUSE_*`. A pointer has five properties a finger does not (a position
  when nothing is pressed, hover, the ability to rest near a screen edge, a
  guaranteed release for every press, being singular), and the translator chain
  depends on all five — that mismatch, not five separate bugs, is what produced
  the placement ghost on the wrong widget, the camera that scrolled by itself,
  the frozen ability radius and the tooltip that would not stay up. Battlefield
  input goes through `TouchInput.cpp` instead, using `pickDrawable` /
  `screenToTerrain` / `evaluateContextCommand`. Read
  `docs/WORKDIR/lessons/LESSON-touch-input-is-not-a-mouse.md` **before** touching
  input code; it also covers the two patterns that keep recurring (preview code
  must be told the aim point rather than reading the mouse; engine state a mouse
  would refresh every frame must be re-asserted, not hunted).
- **Rendering correct on Vulkan but wrong on GLES (or vice versa)**: almost always a D3D↔GL
  *convention* mismatch, not a logic bug — the engine's D3D-era corrections (half-pixel offset,
  viewport Y origin, clip-space Y, render-target texture origin, default address mode) are
  right under DXVK and wrong under the native GLES backend. Read
  `docs/WORKDIR/lessons/LESSON-d3d-vs-gl-rasterization-conventions.md` **before** hypothesis-hunting;
  it lists the known instances and the debugging method (measure the transform's *output*, not
  its inputs).
- **A D3D state value that "cannot have meant that"**: in a translation layer, zero is a
  VALUE, not "unset" (`D3DRS_STENCILMASK`/`STENCILWRITEMASK`/`COLORWRITEENABLE` of 0 all mean
  "none"), D3D state is unsigned (`DWORD` -> `GLint` silently turns `0x80808080` into a
  negative that GL clamps to 0), and an unimplemented fixed-function op must warn rather than
  be silently approximated. Every `x ? x : DEFAULT` in a state translator is a latent bug of
  this kind; seed the API's documented defaults instead. Three GLES-only visual bugs in this
  port came from exactly this -- read
  `docs/WORKDIR/lessons/LESSON-d3d-state-value-semantics.md`, which also records which
  diagnostics were blind to it and why.
- **Slow/stuttering on the native GLES backend**: check the D3D8 **lock/usage flags** before
  anything else. `D3DLOCK_DISCARD`/`D3DLOCK_NOOVERWRITE` and the lock's `offset`/`size` are a
  synchronization contract; dropping them turns every dynamic-buffer update into a GPU stall,
  which is what cost this port ~45 fps. Cost that scales with *call count* and is flat per call
  means waiting, not working. Read
  `docs/WORKDIR/lessons/LESSON-gles-dynamic-buffer-stalls.md` — it also documents the
  per-subsystem draw and UI-time counters (`[d3d8gles] perf-draws/frame by source:`,
  `[d3d8gles] perf-ui ms/frame:`) that attribute a frame's cost from a device log.

## Testing & Validation
### Smoke test
```bash
./scripts/qa/smoke/docker-smoke-test-zh.sh linux64-deploy
```

### Replay testing
```bash
cd ~/GeneralsX/GeneralsMD
./run.sh -win -logToCon 2>&1 | grep -v "D3DRS_PATCHSEGMENTS" | tee ~/GeneralsX/logs/manual_run.log
```

### Debug GDB
```bash
mkdir -p logs && gdb -batch -ex "run -win" -ex "bt full" -ex "thread apply all bt" \
  ./build/linux64-deploy/GeneralsMD/GeneralsXZH 2>&1 | tee logs/gdb.log
```

## Important Commands
```bash
# Linux deployment
./scripts/build/linux/deploy-linux-zh.sh
./scripts/build/linux/run-linux-zh.sh -win

# macOS workflow
./scripts/build/macos/build-macos-zh.sh
./scripts/build/macos/deploy-macos-zh.sh
./scripts/build/macos/run-macos-zh.sh -win

# VS Code tasks recommended
# Linux: [Linux] Configure (Docker), [Linux] Build GeneralsXZH, [Linux] Run GeneralsXZH
# macOS: [macOS] Configure, [macOS] Build GeneralsXZH, [macOS] Run GeneralsXZH
```

## Branching & Sync
### TheSuperHackers upstream sync
```bash
git remote add thesuperhackers git@github.com:TheSuperHackers/GeneralsGameCode.git
git fetch thesuperhackers
git merge thesuperhackers/main
```

**Conflict resolution**:
- Platform code (`Core/GameEngineDevice/`): keep ours
- Game logic (`GeneralsMD/Code/GameEngine/`): keep theirs
- Build system: merge carefully, test both versions

## Code Conventions
- **Annotate changes**: `// GeneralsX @keyword author DD/MM/YYYY Description`
- **Keywords**: `@bugfix` / `@feature` / `@performance` / `@refactor` / `@tweak` / `@build`
- **Attribution**: Add upstream PR references with author and GitHub URL
- **English only**: All code, comments, documentation
- **No lazy code**: No empty stubs, empty catch blocks, or commented-out code

## GitHub PR/Issue Formatting
- Use `--body-file` with real Markdown file instead of `--body`
- Avoid literal `\n` sequences; prefer actual newlines in multi-line strings

## VS Code Tasks
- Prefer task-first execution for build/test/debug
- Logs captured to `logs/` directory
- Primary labels: `[Linux]`, `[macOS]`, `[Linux] Pipeline: Build + Deploy + Run ZH`

## Docs Workflow
1. Monthly diary in `docs/DEV_BLOG/YYYY-MM-DIARY.md` (YYYY=year, MM=month only, e.g., `2026-05-DIARY.md`)
2. Active work notes in `docs/WORKDIR/` (phases/planning/reports/support/audit/lessons)
3. Step-by-step tutorials in `docs/HOWTO/` (user-facing guides for common tasks)
4. Never drop working docs directly under `docs/` root

## GitHub CLI Examples
**Create issues:**
```bash
gh issue create \
  --title "Brief, actionable title" \
  --body "## Context\n...\n## Goal\n...\n## Acceptance Criteria\n..." \
  --label bug --label Linux
```

**Create PRs (use temp file for body):**
```bash
cat > /tmp/pr-body.md << 'EOF'
## Description
Fixes #123

## Changes
- Platform isolation
EOF
gh pr create --title "Description" --body-file /tmp/pr-body.md
```

**Verify PR body (check for literal \n):**
```bash
body=$(gh pr view <number> --json body --jq .body)
printf "%s" "$body" | rg '\\n' && echo "HAS_LITERAL_BACKSLASH_N=YES" || echo "HAS_LITERAL_BACKSLASH_N=NO"
```

## Build Presets Reference
- **android-vulkan** – NDK arm64-v8a, API 28, Vulkan native, no MoltenVK (PRIMARY TARGET)
- **ios-vulkan** – iOS ARM64, DXVK → MoltenVK → Metal
- **linux64-deploy** – GCC/Clang x86_64, Release (PRIMARY LINUX)
- **linux64-testing** – Debug variant
- **macos-vulkan** – macOS ARM64, RelWithDebInfo (PRIMARY MACOS)
- **mingw-w64-i686** – MinGW cross-compile (exploratory)
- **vc6** – Visual Studio 6, 32-bit (legacy)
- **win32** – MSVC 2022, experimental

## Directories
- `GeneralsMD/`: Zero Hour.
- `Generals/`: base game.
- `Core/`: shared libraries.
- `references/`: thesuperhackers-main, fbraz3-dxvk (active); archive/ (historical).
- `docs/WORKDIR/`: current work docs.
- `docs/HOWTO/`: user-facing step-by-step tutorials (SagePatch config, etc.)
- `logs/`: build/run/debug logs.

## Instruction Context Loading

`AGENTS.md` is the source of truth. The `.github/instructions/` files are scoped VS Code hints — they load only when the file path matches.

| Instruction File | applyTo | Purpose |
|---|---|---|
| `generalsx.instructions.md` | `**` | Stub → points to AGENTS.md |
| `git-commit.instructions.md` | `**` | Commit/PR message standards |
| `cpp-conventions.instructions.md` | `**/*.{cpp,h,hpp,c}` | Code style, annotations, platform isolation |
| `build.instructions.md` | `cmake/**,CMakeLists.txt,CMakePresets.json` | Build presets, DXVK source of truth |
| `platform-linux.instructions.md` | `scripts/build/linux/**` | Linux build notes |
| `platform-macos.instructions.md` | `scripts/build/macos/**,references/fbraz3-dxvk/**` | macOS/DXVK build notes |
| `docs.instructions.md` | `**/*.md` | Documentation structure and workflow |
| `scripts.instructions.md` | `scripts/**` | Script organization and naming |

Update this table when instruction files are added, removed, or renamed.
