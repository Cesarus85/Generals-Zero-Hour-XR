# Generals: Zero Hour XR — 1.2.24 preview

This is a **private, offline Meta Quest 3 preview**. It retains the Zero Hour
tabletop game and adds an optional passive, human-scale Ground View during
offline Skirmish. The user has accepted the board-side layout of the matching
UI, Commands and Ground View buttons. No game unit is possessed and the
simulation, multiplayer protocol and retail data are unchanged.

## Install

Download `Generals-Zero-Hour-XR.apk` and its `.sha256` file from this release.
The APK SHA-256 is
`bb685046bcfcdcd5222b648ac3ebe3a008dbe19bfc5a863b18b4ba749c17195e`.
With Quest Developer Mode enabled, connect the headset and run:

```sh
adb devices
adb -s <quest-serial> install -r Generals-Zero-Hour-XR.apk
```

Use `-r` to preserve an existing compatible installation and its settings;
do not uninstall it first. The package ID remains
`com.generalsx.zerohour.xr`, versionCode is `10224`, and versionName is
`1.2.24-xr-preview`. This APK is ARM64-only, non-debuggable and signed with
the same private release certificate as the earlier official previews.
The 1.2.23 test APK was accepted for button position and appearance; its
Ground View walking, collision and comfort have not been exhaustively tested
in the headset. This preview therefore treats stick locomotion as experimental.

The APK contains no original game archives, maps or videos. Supply your own
complete, legally obtained *Generals* and *Zero Hour* installation. The
[Quest installation and controller guide](../../HOWTO/INSTALLATION_XR.md)
explains the import and Ground View controls.

## Changes

- Matching board-side UI, Commands and Ground View shortcuts.
- In offline Skirmish, click Ground View and then visible, clear terrain to
  enter the passive observer. B/Y returns to the unchanged tabletop.
- Experimental physical left-stick walking and right-stick turning in Ground
  View, with terrain, map-edge and drawable movement guards.
- Loading, tracking loss, match results and mode transitions exit observation
  safely. Regular campaign and Skirmish tabletop play remain available.

## Limits

- Ground View is not unit control, first-person combat, campaign entry or
  multiplayer spectating. It does not add gameplay orders.
- The supported preview path is offline tabletop play. Human LAN/Internet
  multiplayer, replay XR, general controller text entry, keyboard/mouse input
  and persistent room anchors remain separate work.
- This is an independent community port, not an Electronic Arts product or
  endorsement. Source licensing and EA's additional conditions are in
  [LICENSE.md](../../../LICENSE.md); retail game data is user-supplied. The
  repository and release remain private.
