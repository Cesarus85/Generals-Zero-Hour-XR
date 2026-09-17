# Generals: Zero Hour XR — 1.2.25 preview

This is a **private, offline Meta Quest 3 preview**. It extends the optional
human-scale Ground View to live Campaign gameplay as well as offline Skirmish.
The normal stereoscopic tabletop remains the default and supported play view.
The game simulation, multiplayer protocol, save format and retail data are
unchanged.

## Install

Download `Generals-Zero-Hour-XR.apk` and
`Generals-Zero-Hour-XR.apk.sha256` from this release. Verify the download with
`shasum -a 256 -c Generals-Zero-Hour-XR.apk.sha256`, then update an existing
installation without uninstalling it. APK SHA-256:
`dcd79e4d6be706f0ddd01d01416d321abd977a37aad768d750232bd1fc65e009`.

```sh
adb devices
adb -s <quest-serial> install -r Generals-Zero-Hour-XR.apk
```

The app remains `com.generalsx.zerohour.xr` (versionCode `10225`, versionName
`1.2.25-xr-preview`) and uses the same private release-signing lineage. The
57.1 MB APK omits native debug sections but retains the same game code and
runtime dependencies. It does not contain the original game archives, maps or
videos. Supply your
own complete, legally obtained *Generals* and *Zero Hour* installation; see
the [Quest installation and controller guide](../../HOWTO/INSTALLATION_XR.md).

## Campaign Ground View

After a Campaign mission reaches freely controllable gameplay, click
**Ground View / Bodenansicht** beside the board or in **UI → View**, then click
visible, clear terrain. The physical left stick walks and the physical right
stick turns; **B** (**Y** in left-handed mode) returns to the unchanged table.
This is a passive observer camera, not unit possession or first-person combat.

Intros, videos, scripted camera movement, loading, dialogs, placement and
match results use the normal presentation. If one begins while observing,
Ground View exits automatically. The previous 1.2.24 preview's Skirmish path,
workspace layout and controls remain available.

## Limits and validation

- The merged source, Android ARM64 build, both Android debug flavors and
  focused observer/loading/workspace/scene/panel/endgame regressions pass.
  Campaign-specific mission transitions, frame rate, walking/collision and
  comfort still require a worn-headset test. The Quest was not ADB-connected
  during packaging; this release does not claim installation or all-mission
  acceptance.
- Human multiplayer, replay XR and general in-game text entry are not part of
  this offline preview. Do not substitute a separate LAN diagnostic APK.
- This independent community port is not an Electronic Arts product or
  endorsement. Source licensing and EA's additional conditions are in
  [LICENSE.md](../../../LICENSE.md); retail game data is user-supplied. The
  repository and release remain private.
