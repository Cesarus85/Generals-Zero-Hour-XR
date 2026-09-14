# Generals: Zero Hour XR

> **This is an unofficial, community-made fan port.** It is not affiliated
> with, endorsed by, or produced by Electronic Arts, Westwood Studios, or any
> other rights holder, and it is not an official Google Play / App Store
> release. It's a fork maintained by volunteers (see
> [Lineage & credits](#lineage--credits)) built from EA's own GPL v3 source
> release. If you saw this described anywhere as an "official" release, that's
> wrong — please don't spread it further.

## Meta Quest tabletop edition

**Generals: Zero Hour XR** is this checkout's native Meta Quest 3 tabletop
edition. The original Zero Hour engine renders a stereoscopic battlefield
through OpenGL ES / OpenXR, with separate build and command windows, controller
selection/orders, DE/EN XR controls and optional real-surface placement.
Campaign and offline AI Skirmish are supported test paths; human multiplayer
and replay XR remain a dedicated future milestone, not validated by Skirmish.

- Public app name: **Generals: Zero Hour XR** (unchanged across languages).
- Android application ID: `com.generalsx.zerohour.xr`, deliberately retained
  for update compatibility and existing local data.
- Java/JNI namespace remains `com.generalsx.zerohour`; underlying engine name
  remains `GeneralsX`. These are technical lineage, not public product names.
- Android foundation: the original [tarek369/GeneralsZH-Android](https://github.com/tarek369/GeneralsZH-Android)
  supplies the Android application/packaging, SDL3 device layer, DXVK/GLES
  runtime and the proven game-data setup path. `Cesarus85/GeneralsZH-Android`
  is the working fork of that project; this repository's `xr` flavor is the
  Quest-focused extension: native OpenXR stereo, tabletop windows, room
  placement and controller interaction are added here.
- Build output: `build/apk/Generals-Zero-Hour-XR.apk`.
- Own legally obtained retail game data is required and is not included.
- Current scope, progress and acceptance:
  [Quest roadmap](docs/WORKDIR/planning/PLAN-023_QUEST_TABLETOP_RECOVERY.md).

## Meta Quest 3 — Installation and controls

This is the practical guide for the XR build. It is written for a Meta Quest 3
with both Touch controllers. The XR package keeps the update-compatible Android
ID `com.generalsx.zerohour.xr`; do not uninstall an older build when you want to
keep its saved window layout, game-folder choice and settings.

The complete Quest installation and controls guide is
[`docs/HOWTO/INSTALLATION_XR.md`](docs/HOWTO/INSTALLATION_XR.md).

### Install the current APK

1. Install *Command & Conquer: Generals – Zero Hour* through a legitimate
   source and copy the game folder to the Quest (or to storage that the Quest's
   Android folder picker can access). The folder must contain the original
   archives, including `INIZH.big` and the base-Generals data such as
   `Terrain.big`, `Textures.big` and `W3D.big`. **The repository and APK do not
   contain copyrighted game data.** See
   [`docs/HOWTO/GETTING_THE_GAME_FILES.md`](docs/HOWTO/GETTING_THE_GAME_FILES.md).
2. Enable Developer Mode for the Quest and connect it by USB. Accept the USB
   debugging dialog inside the headset.
3. Download [`Generals-Zero-Hour-XR.apk`](../../releases/latest/download/Generals-Zero-Hour-XR.apk)
   from the latest release. From a computer with Android platform-tools, run:

   ```sh
   adb devices
   adb -s <quest-serial> install -r Generals-Zero-Hour-XR.apk
   ```

   `-r` performs an update under the retained package ID. SideQuest or another
   trusted APK sideloader works as well. The release APK is a development
   preview, not a Play Store package.
4. Start **Generals: Zero Hour XR** from the Quest app library. If this is the
   first launch, the setup screen opens automatically. Select the game folder
   and grant the requested file/scene permissions. A denied scene permission
   does not block the manual placement mode.

### First tabletop setup

The battlefield starts in the tabletop presentation. Open `UI → Set up play
space` when you want to change it.
Choose one of
these paths in order:

- **Free board**: place the board in front of you with the laser and confirm.
- **Room surface**: load room data, choose a detected table or floor, preview
  the fit, then confirm. Cyan outlines are detected surfaces; orange is the
  valid board footprint; red means the board is too large.
- **Manual height**: record controller height, aim at the desired location and
  confirm again.

The room workflow is optional and is not a permanent furniture anchor. If no
surface is selected, the current build restores the saved arrangement; the next
XR milestone changes that fallback so a fresh board and build window appear in
front of the player instead of inheriting an inconvenient travelling setup.

`UI → Windows` selects the object to edit. The selected table or build window
receives an orange outline and check mark. One Grip moves and tilts it; both
Grips scale it uniformly. With no Grip, the pointing-hand stick changes
size and the other-hand stick changes distance. Press the pointing-hand stick to
switch between table and build window. The reset button returns the selected
object in front of you. Finish, the commands button or Back exits editing.

### Controller reference (right-handed default)

| Input | Effect |
|---|---|
| Right Trigger, short | Select a unit or use the context order at the laser target |
| Right Trigger, hold + drag | Draw a selection rectangle; release to select |
| Left Grip while pressing | Add to selection / remove one unit |
| Left Thumbstick | Pan the map |
| Right Thumbstick left/right | Rotate the map/camera |
| Right Thumbstick up/down | Zoom in/out |
| Right Grip | Cancel an armed order; otherwise clear selection |
| A | Toggle the commands window |
| B or system menu | Open/close the game menu |
| Left Grip + Right Stick left/right while placing | Rotate a building preview before placement |

Select units first, then choose `Move`, `Attack move` or `Guard` in the
commands window and point at the target on the table. `STOP` and `Scatter` act
immediately. To create a group, select units → `Save selection` → a number;
pressing the number later recalls it. To extend one, select more units → `Add to
selection` → the number → `Save selection` again. `Commands → Help` contains
the same four-page guide in German and English. `UI → View` switches handedness
and language; in left-handed mode the logical roles and A/B/X/Y mappings swap.

The first-run launcher language follows the system: German for a German Quest,
English otherwise. The game-text language is kept separate and only switches to
German when German game data is present; all XR product labels remain
**Generals: Zero Hour XR**.

### Features and known boundaries

- Native OpenXR stereo tabletop battlefield with movable, scalable table,
  build and commands windows.
- Offline AI Skirmish and campaign playback, unit selection, drag-box,
  groups, orders, waypoints, building rotation, hover prices/requirements and
  a controller guide.
- Optional room-surface placement, manual height placement, left-handed
  controls and German/English localization.
- Quest defaults are Balanced resolution, Light shadows and Multiview; the
  performance measurement overlay is off for normal play.
- Human LAN/internet multiplayer, replay XR access, keyboard/mouse support and
  persistent room anchors remain roadmap work. Offline AI Skirmish is the
  supported XR test path and does not validate network synchronization.

If the game starts without a board, use `UI → Set up play space` and choose
Free board or Manual height. If a release update appears to have reset
everything, verify that the old package was updated rather than uninstalled;
the application ID is intentionally unchanged. For a bug report, enable only
the requested diagnostic marker in Setup and use `View Logs → Share`.

### Build from source

```sh
git clone https://github.com/Cesarus85/Generals-Zero-Hour-XR.git
cd Generals-Zero-Hour-XR
git submodule update --init --recursive
./scripts/build/android/build-android-zh.sh
./scripts/build/android/package-android-zh.sh
```

The XR output is `build/apk/Generals-Zero-Hour-XR.apk`; the normal Android
(`zh`) APK is emitted separately. Android NDK/SDK, Gradle, vcpkg and the
asset-free build prerequisites are described in
[`docs/port/ANDROID_PORT.md`](docs/port/ANDROID_PORT.md). Build scripts never
fetch or redistribute the original game archives.

## Inherited Android / Apple port background

The following background documents the upstream phone/desktop ports and their
capabilities. It is not a promise that every networking or platform feature is
already available in the XR edition. Original attribution and licenses remain.

<img width="500" height="281" alt="IMG_3457_500" src="https://github.com/user-attachments/assets/aeaf6692-36e6-40c8-b9f8-8066d014ec4b" />

**Zero Hour running natively on Android** — campaign, skirmish, Generals Challenge,
and full online multiplayer (GeneralsOnline: lobby, custom match, quickmatch,
persona/stats, friends & social), with touch controls built for RTS (tap-select,
drag-box, long-press deselect, two-finger pan, pinch/anchor zoom). No emulation:
this is the real 2003 engine compiled for ARM64, rendering DirectX 8 →
[DXVK](https://github.com/doitsujin/dxvk) → **Vulkan native** — no translation
layer beyond DXVK itself, since Android speaks Vulkan directly.

There is a **second, independent renderer**: DirectX 8 → **OpenGL ES 3.0**,
through a native translation layer written for this port (optionally routed
through [ANGLE](https://github.com/google/angle)), with **no DXVK and no
Vulkan involved at all**. Pick it in the Setup app — it exists for phones
whose Vulkan driver can't carry DXVK, which until now had no way to run the
game. Vulkan stays the default where it works.

The same codebase also runs on Apple Silicon Macs, iPhone, and iPad (DirectX 8 →
DXVK → [MoltenVK](https://github.com/KhronosGroup/MoltenVK) → Metal) — that's
where this port started, and it's still maintained, but active development has
shifted to Android, which is now the most complete and most heavily
real-device-tested target.

Built on EA's GPL v3 source release, standing on a chain of community work —
[TheSuperHackers](https://github.com/TheSuperHackers/GeneralsGameCode),
[Fighter19's original Unix port](https://github.com/Fighter19/CnC_Generals_Zero_Hour), and
[fbraz3/GeneralsX](https://github.com/fbraz3/GeneralsX) — this fork adds the
Android and iOS/iPadOS ports, GeneralsOnline multiplayer, and a set of engine
fixes. See [Lineage & credits](#lineage--credits) for who built what. The
original GeneralsX README lives on the `upstream-main` branch.

**No game assets are included or distributed.** You need your own copy
([Steam](https://store.steampowered.com/app/2732960/), ~$5 on sale).

## Status

| Feature | Status |
|---|---|
| Campaign / Skirmish / Generals Challenge | ✅ Working |
| Rendering — OpenGL ES (DirectX 8 → GLES 3.0, no DXVK) | ✅ Working — **the default.** A native backend written for this port; it works on every device tested so far, including ones whose Vulkan driver renders corrupted graphics. |
| Rendering — Vulkan (DirectX 8 → DXVK → Vulkan) | ✅ Working — selectable in Setup. Native Vulkan 1.3 (Adreno 7xx/8xx), adaptive Vulkan 1.1 fallback (Mali-G76/G57). Faster where the driver handles it; unusable on some (PowerVR BXM, Samsung Xclipse — see [Known issues](#known-issues)). |
| Audio | ✅ Working (OpenAL, OpenSL/AAudio backends) |
| Video / cutscenes | ✅ Working (FFmpeg) |
| Touch controls | ✅ Working — native touch, not mouse emulation (see [Touch controls](#touch-controls)) |
| Online multiplayer (GeneralsOnline) | ✅ Working — real matches between real players, P2P transport |
| macOS / iOS / iPadOS build | ✅ Working (campaign/skirmish/Challenge; no GeneralsOnline there yet) |
| Performance on Vulkan-1.1-only GPUs (Mali) | ⚠️ Playable, but CPU-bound — expect lower FPS and occasional freezes on weaker/older phones |

- **Android**: primary target. Grab a prebuilt APK from
  [Releases](../../releases/latest), or build via GitHub Actions (**Actions
  tab → Build Android → Run workflow**) — no local toolchain needed either
  way. Campaign, skirmish, and Generals Challenge run natively. **Online
  multiplayer works, including actual matches**: GeneralsOnline (a from-scratch
  NGMP-based backend, not the long-dead GameSpy servers) drives account login,
  the multiplayer lobby, Custom Match (create/browse/join, live room + player
  lists, chat), Quickmatch, My Persona (stats/rank), and Communicator
  (friends/social) — and matches now actually start and play: a P2P transport
  (Valve's [GameNetworkingSockets](https://github.com/ValveSoftware/GameNetworkingSockets),
  built from source for Android with native ICE/STUN/TURN, no external WebRTC
  dependency) replaces the legacy transport that internet games never had a
  real implementation for. This has been shaken out against real players and
  real devices, including bug reports from outside testers via this repo's
  [issue tracker](../../issues) — see
  [`docs/port/ANDROID_PORT.md`](docs/port/ANDROID_PORT.md) for the device/driver
  matrix and the full bring-up log.
- **macOS / iOS / iPadOS**: fully working (campaign, skirmish, Generals
  Challenge), maintained, not currently receiving the same volume of new work.
  GeneralsOnline multiplayer has not been ported to these platforms yet — the
  Android build is where that backend was built.

## Touch controls

Battlefield input is **native**: a finger is not turned into a mouse. Taps,
double taps, long presses, the two-finger cancel and ability targeting are
resolved directly against the engine's own decision-making — pick what is
under the finger, project it into the world, ask the engine what order that
means. The orders produced are identical to the ones the mouse path
produces, so multiplayer and replays are unaffected.

Taps on the control bar and menus are still ordinary clicks, deliberately: a
tap on a button *is* a click, and the window manager's handling of it is
correct. The window manager also gets first refusal on every battlefield tap,
so a tap on a panel never falls through to the map underneath.

| Gesture | Action |
|---|---|
| Tap on the map | Select what is under your finger, or — with units selected — give them the order that point implies (move, attack, enter, repair …) |
| Tap a unit you own | Select it. If the engine says your current selection has a *specific* interaction with it (get in, repair), that happens instead |
| Double-tap | Select every unit of that type on screen |
| Press and hold, then drag | Selection box |
| Drag | Pan the camera (direct, no mouse involved) |
| Two fingers | Pan and zoom together |
| Long press on the map (~0.6s, no movement) | Cancel an armed ability or a pending building; otherwise clear the selection |
| Two-finger tap | The same cancel |
| Tap a UI button | Press it |
| Hold a UI button | Read its description, without pressing it |
| **With an ability armed**: touch the map, drag, release | Aim it — the radius circle follows your finger; release fires it where you let go; a second finger cancels |
| **With a building picked**: tap where it goes | Place it. The ghost appears under your finger, not before you point |

Every gesture is classified only once the intent is unambiguous (a short
delay or distance threshold), so an ordinary tap never misfires into a
selection box, and pan and zoom don't flicker into each other.

Screen-edge scrolling is **off** on touch. It is defined by a pointer resting
near an edge, and it ends only when a later pointer event reports a position
back inside the safe zone — a condition that cannot occur without a pointer,
which is why it used to leave the camera scrolling on its own. Dragging with
a finger is the touch equivalent and is already direct.

If controls misbehave, turn on **Touch input overlay** in Setup →
Diagnostics: it draws the current gesture, where your finger is, where the
engine thinks the pointer is, and the camera's scroll anchor, and writes
matching `[gxtouch]` lines into the log. Those four things are the same thing
on a desktop and different things on a touchscreen, and their disagreement is
what every control bug here has turned out to be.

For how this is built and how to add a gesture, see
[`docs/WORKDIR/lessons/LESSON-touch-input-is-not-a-mouse.md`](docs/WORKDIR/lessons/LESSON-touch-input-is-not-a-mouse.md).

## What this port actually involved

"Porting" undersells how weird this journey was, so here's the honest shape of it.
The lineage below built the foundation: EA's source release, the community's
modernization, Fighter19's original Unix port, GeneralsX's macOS/Linux work.
None of that included a mobile online-multiplayer backend, or Android at all —
and both are hostile territory for a 2003 Windows RTS:

- **GameSpy is dead. The retail multiplayer stack assumes it isn't.** Zero
  Hour's entire online layer — matchmaking, lobbies, buddy lists, stats — was
  built on GameSpy SDK calls to servers EA shut down over a decade ago. Getting
  multiplayer working again meant building a real backend (GeneralsOnline,
  NGMP-based: REST + WebSocket, its own auth/session/lobby/stats/social
  services) and re-wiring the original `.wnd` UI screens and GUI callbacks —
  largely untouched since 2003 — to talk to it instead, one menu at a time
  (Welcome screen, Custom Match, Quickmatch, My Persona, Communicator).
- **Async callbacks + screen teardown is a loaded gun.** Almost every real
  crash chased down on real devices during multiplayer bring-up turned out to
  be the same shape: an HTTP or WebSocket completion callback captured a raw
  pointer (a `GameWindow*`, a roster entry, a listbox) that was still valid
  when the request was *sent*, but the user had already backed out of the
  screen — or a second refresh had already torn it down — by the time the
  response landed. The fix pattern that recurred: capture a stable ID, not a
  pointer, and re-resolve (or bail) inside the callback.
- **The engine assumes a writable filesystem wherever it lives, and a mouse.**
  Android's scoped storage and SDL3's raw touch events needed the same kind of
  rerouting and gesture-to-mouse translation work the iOS port pioneered — tap
  defers until the 2003 GUI processes hover, a drag becomes a selection box or
  a camera pan depending on how it started, one held finger + a second moving
  vertically is zoom (not classic two-finger pinch, which fought camera pan),
  and a double-tap now does what a PC double-click always did (select all of
  one unit type on screen).
- **Old data, new parser, sharp edges.** Zero Hour's `.ini` data layers on top
  of base Generals data, and the two games were split into separate build
  targets at some point in this codebase's history — with a couple of
  genuinely-still-used tokens (`DamageType=FLESHY_SNIPER`, `KindOf=AIRFIELD`)
  accidentally compiled out of the Zero Hour build in that split. Both looked
  like "someone's mod is doing something weird" until traced back to a
  preprocessor guard on the wrong side of an `#if`.
- **And a memory-corruption hunt that went all the way to the allocator.**
  Unresolved-crash-PC segfaults with no clean call stack, days apart, no
  obvious pattern — eventually traced to the engine's global `operator
  delete` override having no way to tell "one of ours" from a pointer a
  separately-linked `.so` (OpenAL, DXVK) allocated through its own copy of
  `new`. Fixed with an ownership cookie instead of blind trust.

**→ The Android engineering log: [docs/port/ANDROID_PORT.md](docs/port/ANDROID_PORT.md)**
**→ The macOS/iOS war stories: [Porting Playbook §8 — the bug archaeology](docs/port/PORTING_PLAYBOOK.md#8-post-ship-bug-hunts-junejuly-2026--the-archaeology-section)**
**→ The complete macOS/iOS engineering log: [docs/port/PORTING_PLAYBOOK.md](docs/port/PORTING_PLAYBOOK.md)**
**→ How to do this to another game: [docs/port/PORTING_PATTERNS.md](docs/port/PORTING_PATTERNS.md)**

Worth saying plainly: this was a **human + AI collaboration**. The engineering —
the C++, the cross-builds, the device debugging, the multiplayer backend — was
done by [Claude Code](https://claude.com/claude-code) (Anthropic's Claude),
directed and playtested by a human who described symptoms like *"the lobby
list is empty"* and *"it crashes right after I press Back"* and owned every
decision. Neither half ships this alone: one of us can't write C++, and the
other can't play-test on a real phone.

## Quick start — Android

Same engine, one translation layer fewer than iOS: DirectX 8 → DXVK →
**Vulkan native** (no MoltenVK). DXVK's own minimum was lowered from Vulkan
1.3 to **Vulkan 1.1**, with an adaptive feature/extension fallback path, so
it now runs on a much wider range of hardware: Snapdragon with Adreno
7xx/8xx (native 1.3), older Adreno below 1.3 (via an optional bundled Mesa
Turnip fallback driver), and Vulkan-1.1-only Arm Mali GPUs (Mali-G76,
Mali-G57, and similar Bifrost/Valhall chips — e.g. the Redmi Note 8 Pro) are
all supported. A device with no usable Vulkan driver at all still gets a
clear on-screen message instead of silently closing. Frame rate on
Vulkan-1.1-only / older-CPU devices is CPU-bound, not GPU-bound — expect
lower FPS and occasional freezes on weaker phones; see the doc for the full
device/driver matrix and driver-replacement options.

**If Vulkan doesn't work on your phone, there is a second renderer.** The
Setup app has a **Render Backend** picker with three options: *Vulkan*
(default, DXVK), *OpenGL ES*, and *OpenGL ES + ANGLE*. The GLES options do
not use DXVK or Vulkan at all — they run DirectX 8 through a translation
layer written for this port
([`Core/Libraries/Source/d3d8gles/`](Core/Libraries/Source/d3d8gles)) straight
onto OpenGL ES 3.0, which every Android GPU speaks. That covers devices whose
Vulkan driver exists but can't carry DXVK, which previously had nothing to
fall back on. Switching backends needs no rebuild: change it in Setup and
restart the game. Vulkan is still the one to prefer where it works — GLES is
the newer path and has had less device exposure.

**Simplest option — no build, no CI**: grab a prebuilt APK from the
[Releases page](../../releases/latest) and sideload it.

**No local toolchain needed either** — push to a `claude/**` branch (or run it
manually) and GitHub Actions builds the APK: **Actions tab → Build Android →
Run workflow**. Every CI build is signed with the same committed debug key and
gets an increasing versionCode, so you can install a newer run **over** an
older one without uninstalling. (On a fork, enable Actions once: Actions tab →
"I understand my workflows, go ahead and enable them".) Download the APK
artifact from the run and install it.

**No adb needed either, for setup or logs**: the APK installs a second icon,
**"GeneralsZH Setup"**, with an in-app folder picker (point it at wherever
you copied your own game files — Downloads, an SD card, anywhere) and a log
viewer with Clear/Share buttons. See [docs/port/ANDROID_PORT.md §4](docs/port/ANDROID_PORT.md#4-game-data-and-first-run--the-in-app-setup-flow-no-adb-no-pc-needed)
for the full first-run walkthrough. A default log is small and readable —
if a bug report needs more (frame-timing breakdown, full trace, DXVK HUD
counters, Vulkan validation), see [**Diagnostic marker files**](docs/port/ANDROID_PORT.md#diagnostic-marker-files-opt-in-extra-logging)
for which plain-text file to drop into the game folder and what it turns on.

Building locally instead needs the Android NDK (r26+), vcpkg, meson/ninja:

```sh
cd GeneralsX
git submodule update --init references/fbraz3-dxvk
export ANDROID_NDK_HOME=~/Android/Sdk/ndk/<version>
./scripts/build/android/build-android-zh.sh        # game -> libmain.so, DXVK -> .so, verified
./scripts/build/android/package-android-zh.sh --install
```

**→ The full guide (device/driver matrix, storage layout, multiplayer
architecture, bring-up log): [docs/port/ANDROID_PORT.md](docs/port/ANDROID_PORT.md)**

## Quick start — macOS

Prerequisites (one time):

```sh
# Toolchain
xcode-select --install
brew install cmake ninja meson pkgconf
brew install --cask steamcmd

# vcpkg (full clone — a shallow clone breaks manifest baselines)
git clone https://github.com/microsoft/vcpkg ~/vcpkg && ~/vcpkg/bootstrap-vcpkg.sh
export VCPKG_ROOT=~/vcpkg          # add to your shell profile

# LunarG Vulkan SDK (NOT the Homebrew cask) — https://vulkan.lunarg.com/sdk/home
export VULKAN_SDK=$HOME/VulkanSDK/<version>/macOS   # add to your shell profile
```

Clone, build, get assets, play:

```sh
git clone https://github.com/ammaarreshi/Generals-Mac-iOS-iPad.git GeneralsX
cd GeneralsX
./scripts/build/macos/build-macos-zh.sh     # checks deps, configures, builds
./scripts/build/macos/deploy-macos-zh.sh    # creates ~/GeneralsX/GeneralsZH + run.sh
./scripts/get-assets.sh <your_steam_username>   # fetches game data you own
cd ~/GeneralsX/GeneralsZH && ./run.sh -win
```

## Quick start — iPhone / iPad

On top of the macOS prerequisites: full Xcode (signed into your Apple ID),
`brew install xcodegen`, and a (free or paid) Apple Developer team.

```sh
cd GeneralsX
git submodule update --init references/fbraz3-dxvk   # iOS DXVK is built from this + Patches/dxvk-ios.patch
./scripts/build/ios/fetch-moltenvk.sh                # pinned MoltenVK.framework (checksummed)
./scripts/build/ios/stage-fonts.sh                   # Liberation fonts, renamed as the game expects
cmake --preset ios-vulkan
cmake --build build/ios-vulkan --target z_generals
GX_TEAM_ID=<your-team-id> GX_BUNDLE_ID=com.you.generalszh \
    ./scripts/build/ios/package-ios-zh.sh --install  # assembles, signs, installs
```

Find your team id in Xcode → Settings → Accounts. Assets ship inside the app
bundle (self-contained install); `--dev` skips the ~2.7 GB copy for fast code
iteration.

## Where things are

| Path | What it is |
|---|---|
| [`docs/port/ANDROID_PORT.md`](docs/port/ANDROID_PORT.md) | The Android port: architecture, GeneralsOnline multiplayer backend, device/driver matrix, build + bring-up log |
| [`docs/port/PORTING_PLAYBOOK.md`](docs/port/PORTING_PLAYBOOK.md) | The complete macOS/iOS engineering log: every failure mode, root cause, fix — start with [§8, the bug archaeology](docs/port/PORTING_PLAYBOOK.md#8-post-ship-bug-hunts-junejuly-2026--the-archaeology-section) |
| `docs/port/PORTING_PATTERNS.md` | Generalized methodology for porting classic Windows games to Apple/mobile platforms |
| `docs/port/RELEASE_CHECKLIST.md` | Gate for public release |
| `scripts/get-assets.sh` | Steam asset fetcher (your own copy; app 2732960) |
| `scripts/build/android/`, `scripts/build/macos/`, `scripts/build/ios/` | Build, deploy, packaging pipelines |
| `android/` | Gradle shell app (SDLActivity) that packages `libmain.so` + DXVK into an APK, plus the Setup/FolderPicker/LogViewer/GeneralsOnline-account activities |
| `ios/` | XcodeGen signing-stub project + `ios/config/` (staged Options.ini, dxvk.conf) |
| `GeneralsMD/Code/GameEngine/Source/GameNetwork/GeneralsOnline/` | The GeneralsOnline multiplayer client: auth, lobby, rooms, stats, matchmaking, social — talks to a REST + WebSocket backend, not GameSpy |
| `Core/Libraries/Source/d3d8gles/` | The DirectX 8 → OpenGL ES 3.0 renderer: device/state emulation, fixed-function-to-GLSL shader generation, texture upload (including a software BC1-3 decoder for GPUs without S3TC) |
| `Patches/dxvk-android.patch`, `Patches/dxvk-ios.patch` | DXVK changes the Android/iOS d3d8/d3d9 `.so`/dylib builds are built from |

## Known issues

- Long sessions on iPad can be killed by iOS for memory (~3 GB+ resident); the app
  exits to the home screen with no dialog. Session logs (current + previous) are in
  the Files app under the game's folder. Under investigation.
- Backgrounding mid-game can occasionally crash on iOS — the lifecycle pause covers
  the common paths; a rare race remains. Save often.
- Android on Vulkan-1.1-only GPUs (Mali-G76, Mali-G57, and similar) is
  CPU-bound: expect lower frame rates and occasional freezes on weaker/older
  phones, especially during map/mission loading, and please share logs if you
  hit something worse than that — see
  [docs/port/ANDROID_PORT.md §2](docs/port/ANDROID_PORT.md#2-the-device--driver-matrix-read-this-before-filing-black-screen-bugs)
  for the device/driver matrix.
- Vulkan renders corrupted graphics on some GPU drivers, and no setting in
  Setup can fix it — the corruption is below the game. Magenta patches,
  distorted geometry and duplicated/overlapping menu elements have been
  reported on PowerVR BXM and Samsung Xclipse. This is why OpenGL ES is the
  default; if Vulkan looks wrong on your device, switch back to it in Setup →
  Render Backend, which also now shows your GPU's own name.
- Magenta textures are usually a game-files problem rather than a renderer
  one, and the build says which: `[gxmiss] magenta substituted (<reason>):
  <file>` names the file and why (no thumbnail, archive missing, unreadable),
  and `[d3d8gles] MAGENTA:` names an unsupported texture format. Zero Hour is
  an expansion — if base *Generals* archives (`Terrain.big`, `Textures.big`,
  `W3D.big`) are absent, most of the artwork has nowhere to come from. Setup
  lists any missing archive by name and can be pointed at a separate base-game
  folder.
- Android multiplayer is under active real-device shakeout — most reported crashes
  have traced to a handful of recurring bug classes (see the "what this port
  actually involved" section above) and get fixed fast, but if something's still
  rough, check or file an issue. Matches now load and play (P2P transport,
  camera, and the load-screen crash chain all confirmed working solo, real
  device); cross-device matches against another live player are the next
  thing being shaken out.

## What's next: Renegade 👀

Generals had a chain of giants to stand on. **Command & Conquer: Renegade** — EA's
2002 FPS from the same GPL source release — has far less: no native macOS or iOS
build of the W3D engine has ever shipped (Mac players today go through Wine-based
compatibility layers). The [OpenW3D](https://github.com/w3dhub/OpenW3D) community
project has real cross-platform groundwork — a DXVK wrapper scaffold and SDL3 build
plumbing — with Mac/Linux on its roadmap, and that groundwork is exactly what we
built on.

Same methodology as this repo, much deeper water: OpenW3D's Win32 compat scaffold
expanded by ~3,000 lines (the engine calls raw Windows APIs for file finding,
keyboard state, COM), a case-sensitivity strategy for twenty thousand asset paths,
the DXVK/MoltenVK renderer bring-up, the audio/video stack, and FPS touch controls.
It's playable today — campaign, cinematics, mission scripts — on a Mac and an
iPhone. For scale: this Generals port added ~2,200 lines on top of GeneralsX;
Renegade needed ~6,700 on top of the Windows-only source.

Repo drops soon, with the OpenW3D lineage credited the way this repo credits its
chain. Same rules: GPL v3, bring your own copy, full engineering log.

## Lineage & credits

This port is the newest link in a long chain, and the earlier links did foundational
work that this repo inherits everywhere:

- **Westwood / EA Pacific** — the game; **EA** — the GPL v3 source release
- **[TheSuperHackers/GeneralsGameCode](https://github.com/TheSuperHackers/GeneralsGameCode)** —
  the community mainline: build modernization, VC6→modern toolchain, and much of the
  cross-platform groundwork, including the FFmpeg video backend authored by
  **[feliwir](https://github.com/feliwir)** (of [OpenSAGE](https://github.com/OpenSAGE/OpenSAGE)),
  who also authored the OpenAL audio device work this port's audio stack builds on
- **[Fighter19/CnC_Generals_Zero_Hour](https://github.com/Fighter19/CnC_Generals_Zero_Hour)** —
  the original Unix/64-bit port: SDL3 platform management, C++17
  filesystem/threading, Freetype/Fontconfig text rendering, and the DXVK approach
  this renderer path descends from
- **[fbraz3/GeneralsX](https://github.com/fbraz3/GeneralsX)** — the macOS/Linux port
  this fork builds on directly, integrating and extending the above
- **[tarek369/GeneralsZH-Android](https://github.com/tarek369/GeneralsZH-Android)** —
  an independent, parallel Android port of the same lineage; several real bugs in
  this port (a duplicate-symbol build break, an INI-parsing gap) were cross-checked
  and traced faster thanks to its published engineering log
- **This fork** — the Android port (GeneralsOnline multiplayer backend, touch
  controls, in-app Setup/log-viewer flow, device bring-up) and the iOS/iPadOS
  port (arm64-ios cross-build, DXVK-on-iOS, touch controls, app lifecycle,
  packaging), plus engine fixes throughout, offered upstream
- **DXVK, MoltenVK, SDL, OpenAL Soft, FFmpeg, Liberation Fonts** — the load-bearing walls

Engine code **GPL v3** (EA's source release → the chain above → this fork). Game
assets: not included, not licensed here.
