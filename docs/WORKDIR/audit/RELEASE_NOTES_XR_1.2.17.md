# Generals: Zero Hour XR — 1.2.17 preview

This is the current **private, offline Meta Quest 3 preview**. It adds a clear
Victory, Defeat or Match over card when a match ends, including the transition
to statistics. The fix observes the original game's end state; it does not
change win conditions, simulation rules or retail game data. The previous
tabletop, campaign, Skirmish and controller features remain available.

## Install

Download `Generals-Zero-Hour-XR.apk` and the accompanying `.sha256` file from
this release. The APK SHA-256 is
`8913648e9c8c124367ac812a4a3e9db0d0f296ec3a2c7e83a7d22627c2182869`.
With Quest Developer Mode enabled, connect the headset by USB, accept its
debugging prompt and run:

```sh
adb devices
adb -s <quest-serial> install -r Generals-Zero-Hour-XR.apk
```

Use `-r` to preserve an existing compatible installation and its settings;
do not uninstall it first. The package ID remains
`com.generalsx.zerohour.xr`, versionCode is `10217`, and versionName is
`1.2.17-xr-preview`. This APK is ARM64-only, non-debuggable and signed with
the same private release certificate as 1.2.15. The short 1.2.16 headset
test confirmed the result card; the 1.2.17 production APK has the debug-only
test controls disabled. The maintainer has not claimed a separate worn-headset
test of every end path in these exact release bytes.

On first launch, select your own complete, legally obtained *Generals* and
*Zero Hour* installation with the in-app assistant. Steam and fully
installed/extracted CD/ISO layouts are supported; raw ISO images and Windows
installers are not. **No original game archives, maps, videos or other retail
assets are included.** See the [Quest installation and controller guide](../../HOWTO/INSTALLATION_XR.md)
and [game-data guide](../../HOWTO/GETTING_THE_GAME_FILES.md).

## Scope and limits

- Campaign and offline AI Skirmish are the supported play modes.
- Human LAN/Internet multiplayer, replay XR, general controller text entry,
  keyboard/mouse input and persistent room anchors are not release-ready.
  Quest–PC Direct Connect is under separate diagnosis because matches lose
  simulation synchronization; no LAN diagnostic code or APK is in this release.
- Balanced resolution, Light shadows and Multiview are the Quest 3 defaults.
  Higher quality modes can reduce frame rate in demanding missions.
- This is an independent, modified community port, not an Electronic Arts
  product or endorsement. Its engine source retains GPLv3 and EA's additional
  terms in [LICENSE.md](../../../LICENSE.md); retail game data must be supplied
  separately by the user. Repository visibility remains private.
