# Generals: Zero Hour XR — 1.2.15 preview

This Meta Quest 3 preview turns your own *Command & Conquer: Generals – Zero
Hour* installation into a stereoscopic tabletop battlefield. The board, build
window and Commands window can be positioned and scaled in mixed reality. The
supported play modes in this release are **Campaign and offline AI Skirmish**.

## Highlights

- Original Zero Hour game world and gameplay in native OpenXR stereo.
- Touch-controller ray selection, drag-box selection, contextual orders,
  groups, waypoints, radar navigation and board camera controls.
- Spatial build, Commands and settings windows; optional table/floor or manual
  placement, with a safe free-standing layout at every new app session.
- English and German XR interface/help, left-handed mode, hover names and
  build costs.
- Balanced resolution, Light shadows and Multiview as Quest 3 defaults;
  higher-resolution options are available at a performance cost.

## Install

Download **`Generals-Zero-Hour-XR.apk`** and its `.sha256` file from this
release. Confirm the APK SHA-256 is
`bafff443d77e7b9e925a73fcf16b5bf7234c54f256e2cb7fa0f95d1aad008d2b`.
With Quest Developer Mode and Android platform-tools, connect the headset,
accept its USB debugging prompt and run:

```sh
adb devices
adb -s <quest-serial> install -r Generals-Zero-Hour-XR.apk
```

Use `-r` to preserve an existing compatible app installation; do not uninstall
if you want to retain its settings and game-data selection. On first launch,
the in-app assistant helps select a complete, legally obtained installation
of both *Generals* and *Zero Hour*. Steam and fully installed/extracted CD/ISO
layouts are supported; raw ISO images and Windows installers are not. The APK
does **not** include the original game archives, maps, videos or other retail
assets. See the [installation and controls guide](../../HOWTO/INSTALLATION_XR.md)
and [game-data guide](../../HOWTO/GETTING_THE_GAME_FILES.md).

Package: `com.generalsx.zerohour.xr`. Version: `1.2.15-xr-preview` (10215).
This APK is non-debuggable and privately release-signed. The maintainer's
Quest 3/API 34 update from diagnostic 10214 retained the app installation,
settings and game data; the installed APK hash matched this release file. The
maintainer subsequently confirmed Skirmish and Campaign launch in the
headset. This does not certify every mission, a clean first-time import or
every possible prior signing lineage.

## Known limits

- Human LAN/Internet multiplayer, replay XR, keyboard/mouse control and
  persistent room anchors are not release-ready. Quest–PC Direct Connect can
  start a match on an experimental branch, but currently loses simulation
  synchronization; that diagnostic build is **not** this release.
- Ultra/Ultra+ graphics settings can reduce frame rate in demanding missions.
  Balanced resolution and Light shadows are the tested default compromise.
- This community port is not affiliated with or endorsed by Electronic Arts.
  Source is provided under GPLv3 with EA's additional conditions; original
  game data remains user-supplied. See [licensing](../../../LICENSE.md) and
  [source provenance](../../../README.md#source-lineage-and-licensing).
