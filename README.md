# Generals: Zero Hour XR

An unofficial Meta Quest 3 tabletop port of *Command & Conquer: Generals – Zero Hour*. The original game's terrain, units and effects appear as a stereoscopic miniature battlefield, with separate spatial build, command and settings windows. This is a community project, not an Electronic Arts product or endorsement.

![A Generals: Zero Hour XR tabletop battlefield with a mountain map, base and floating build window](docs/media/quest-tabletop/mountain-tabletop.jpeg)

<p align="center">
  <img src="docs/media/quest-tabletop/base-and-build-window.jpeg" width="49%" alt="Quest tabletop base with the detached build window and unit commands">
  <img src="docs/media/quest-tabletop/commands-and-groups.jpeg" width="49%" alt="Overhead tabletop view with the spatial Commands window and control groups">
</p>

<p align="center">
  <img src="docs/media/quest-tabletop/snow-battlefield.jpeg" width="70%" alt="Snow-covered Zero Hour battlefield displayed as a Quest tabletop">
</p>

These are headset captures supplied by the project maintainer. Map, faction, graphics settings and room lighting vary; the images illustrate the tabletop presentation rather than certify a particular APK build.

## What works in the Quest preview

- Native OpenXR stereo tabletop gameplay using the original Zero Hour game world, not replacement models.
- Campaign and offline AI Skirmish in the tabletop presentation; cinematics and full native dialogs appear upright.
- Touch-controller selection, drag-box multi-selection, contextual orders, map pan/rotate/zoom, radar navigation, building rotation, groups and waypoint routes.
- Detached build window, persistent Commands window and a spatial UI panel; move, tilt, scale or recenter the workspace. Optional detected-surface or manual-height placement does not require a room scan.
- English and German XR interface/help, left-handed controller mode, hover names and build costs.
- Quest 3 defaults: Balanced resolution, Light shadows and Multiview. Higher quality modes, including Ultra+, are optional and may cost performance.

The **supported preview path is offline play**. Human LAN/Internet multiplayer, replay XR, keyboard/mouse input and persistent room anchors are **not release-ready**. Direct Connect can start an experimental LAN match, but Quest–PC games currently fail simulation synchronization. Do not use a LAN diagnostic APK as the public preview. See [multiplayer status](docs/WORKDIR/planning/MULTIPLAYER_STATUS.md).

## Get the tested offline APK

The current offline candidate is [P23: command and Ultra+ preview](https://github.com/Cesarus85/Generals-Zero-Hour-XR/releases/tag/xr-preview-2026-09-16-p23). Download its [`Generals-Zero-Hour-XR.apk`](https://github.com/Cesarus85/Generals-Zero-Hour-XR/releases/download/xr-preview-2026-09-16-p23/Generals-Zero-Hour-XR.apk) asset, not a diagnostic APK from the LAN branch. This is a development preview; its signing and debuggable settings are still under review for a public release.

- APK SHA-256: `978c627e276ab1627d014f68a8e1ad436ecaae215e8946ef900e5b999077273f`
- Android package: `com.generalsx.zerohour.xr` (retained for update compatibility)
- Android versionCode: `10209`; the inherited versionName is `1.2.9-p20.1-thin-underbody` even though this tagged asset includes P23.

GitHub currently marks the older 1.2.8 preview as “Latest” because P23 is a prerelease. The explicit P23 link above is intentional. A final public release has **not** been published from this preparation work.

## Install and play

1. Supply your own legally obtained, complete *Generals* **and** *Zero Hour* installation. The APK/repository contains no retail `.big` archives, maps, videos or other game data. See [which files to copy](docs/HOWTO/GETTING_THE_GAME_FILES.md).
2. Enable Quest Developer Mode, connect the headset by USB and accept its debugging prompt. With Android platform-tools installed, run `adb devices`, then `adb -s <quest-serial> install -r Generals-Zero-Hour-XR.apk`. The `-r` update keeps the existing app's settings and selected game folders; uninstalling does not. A development Quest already running diagnostic versionCode 10214 cannot use this ordinary update command to roll back to P23's 10209; see the [release-preparation audit](docs/WORKDIR/audit/RELEASE_PREPARATION_XR.md).
3. Open **Generals: Zero Hour XR** in the Quest app library. On first run, the English/German data assistant guides you through choosing the Zero Hour folder and, if needed, the separate base Generals folder. It accepts complete Steam installs and installed/extracted CD/ISO layouts, not raw ISO images or Windows installers.
4. Start a campaign or offline Skirmish. The board, build window and Commands window appear in front of you. Use `UI → Set up play space` only if you want to reposition them or choose a detected table/floor.

See the [full Quest installation, placement and controller guide](docs/HOWTO/INSTALLATION_XR.md). If something fails, the Setup app's **View Logs → Share** exports diagnostic logs; please avoid posting private paths or room details publicly.

### Essential controls (right-handed default)

| Control | Action |
|---|---|
| Right Trigger | Select or order at the ray target; hold and drag for a selection box |
| Left Grip + selection | Add or remove a unit |
| Left Thumbstick | Pan the map; click to return the map camera to your base |
| Right Thumbstick | Rotate and zoom |
| A | Show/hide Commands |
| B or system menu | Open/close the game menu |
| X | Recenter the board and companion windows in front of you |
| Right Grip | Cancel an armed order or clear selection |

The Commands window also has an in-game help guide. In left-handed mode, the logical hand and A/B/X/Y roles swap. For groups: select units → **Replace group** → number, or select additional units → **New / Extend** → number. For waypoints: select units → **Waypoints** → destinations → **Waypoints** again. See the [complete control reference](docs/HOWTO/INSTALLATION_XR.md#controller-controls) for radar, building rotation and window editing.

## Source, lineage and licensing

This repository contains the Quest-focused extension of the community [GeneralsZH-Android](https://github.com/tarek369/GeneralsZH-Android) work, through the developer's [Android fork](https://github.com/Cesarus85/GeneralsZH-Android), together with the [GeneralsX](https://github.com/fbraz3/GeneralsX), [Fighter19](https://github.com/Fighter19/CnC_Generals_Zero_Hour) and [TheSuperHackers](https://github.com/TheSuperHackers/GeneralsGameCode) lineage. The Android foundation and XR work are distinct contributions; the original game and assets remain EA's. Engine source is released under GPLv3 with EA's additional terms; see [LICENSE.md](LICENSE.md) and the source history for conditions and attribution. No affiliation with EA, Westwood Studios or any other rights holder is implied.

To build from source, initialize submodules and follow the [Android port guide](docs/port/ANDROID_PORT.md). With its SDK/NDK and vcpkg prerequisites installed:

```sh
git submodule update --init --recursive
./scripts/build/android/build-android-zh.sh
GX_FLAVORS=xr ./scripts/build/android/package-android-zh.sh
```

The XR APK is emitted at `build/apk/Generals-Zero-Hour-XR.apk`. Builds do not fetch retail game data. The source tree also retains inherited Android/Apple targets, but their features and release status are not claims about this Quest edition.

The [XR current status](docs/WORKDIR/planning/XR_CURRENT_STATUS.md) tracks acceptance and remaining work. [Release preparation](docs/WORKDIR/audit/RELEASE_PREPARATION_XR.md) records the exact candidate, checks and publication gates.
