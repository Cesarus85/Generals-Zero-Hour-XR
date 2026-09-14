# Generals: Zero Hour XR - Quest 3 installation guide

This guide covers the **Generals: Zero Hour XR** build for Meta Quest 3. The
repository contains the ported source code and launcher, but no original maps,
textures, videos, or `.big` archives are included for licensing reasons. You
need your own legally obtained installation of *Command & Conquer: Generals -
Zero Hour*, including the base *Generals* data.

The Android foundation is the published
[`tarek369/GeneralsZH-Android`](https://github.com/tarek369/GeneralsZH-Android)
project, which supplies SDL3, Android packaging, DXVK/GLES support, and the
game-data folder setup. It was forked as
[`Cesarus85/GeneralsZH-Android`](https://github.com/Cesarus85/GeneralsZH-Android)
on the developer's GitHub account. This repository's XR flavor extends that
Android base with OpenXR stereo, tabletop windows, room placement, and Quest
controller interaction.

## Requirements

- Meta Quest 3 with a current Horizon OS release and both Touch controllers
- Developer Mode enabled and a USB-C data cable for ADB or SideQuest
- Your own Zero Hour files, including `INIZH.big` and the base archives
  `Terrain.big`, `Textures.big`, and `W3D.big`
- Approximately 250 MB of free storage for the preview APK, plus space for your
  game data

You can copy the original files from a Windows or Steam PC to the Quest, or to
storage that the Quest Android folder picker can access. Obtaining game files is
outside the scope of this project; see
[`GETTING_THE_GAME_FILES.md`](GETTING_THE_GAME_FILES.md).

## Install the APK

1. Download the latest
   [`Generals-Zero-Hour-XR.apk`](../../releases/latest/download/Generals-Zero-Hour-XR.apk)
   from GitHub Releases.
2. Enable Developer Mode in the Meta Quest mobile app, connect the headset over
   USB, and accept the USB debugging prompt inside the headset.
3. Check the connection and install the APK from a computer with Android
   platform-tools:

   ```sh
   adb devices
   adb -s <quest-serial> install -r Generals-Zero-Hour-XR.apk
   ```

   The `-r` flag matters: the XR application ID deliberately remains
   `com.generalsx.zerohour.xr`, so an update can keep existing settings and
   window positions. Do not uninstall the previous build when updating.
   SideQuest can install the same APK.
4. Launch **Generals: Zero Hour XR** from the Quest app library. On first
   launch, the game-data assistant opens automatically. Grant file access when
   requested. Scene permission is separate and optional, requested later when
   choosing real-surface placement.

## Select game data

The current release provides a three-step assistant:

1. **Prepare your game files.** Copy your own complete installed Generals and
   Zero Hour folders to the Quest, for example `Download/GeneralsZH`.
   **Steam or CD / ISO: which files?** explains the layouts. Steam installations
   often contain `ZH_Generals`; preserve that subfolder. CD/ISO editions must
   first be installed or fully extracted on a computer. Copy installed game
   data, not an ISO, `setup.exe`, MSI or CAB. The Quest app does not mount
   images, run Windows installers, download retail files or duplicate the game.
2. **Choose Zero Hour folder.** Select the folder containing `INIZH.big`, or
   its parent (bounded search: two levels, 128 folders). The in-app filesystem
   browser needs All files access; it is not Android's document-tree picker.
   After granting access, return and selection continues automatically.
   Nested `ZH_Generals`, merged data and an unambiguous adjacent base folder
   are detected. Otherwise use **Choose separate Generals folder**. When
   several Zero Hour installs exist, choose the intended one explicitly.
3. **Review and start.** Setup lists resolved folders, missing archives,
   incomplete/unreadable archive indexes and missing text/configuration data.
   Only **Use these folders and play** saves a valid choice. Choosing a wrong
   folder, cancelling or switching interface language preserves the previous
   configuration. Returning Quest launches recheck saved files silently inside
   the XR Activity: no importer window or 2D environment round trip. Setup
   opens automatically only when the files are missing or unusable.

The **English / Deutsch** button is visible on the assistant. System Default
remains available; changing the assistant language does not change game speech
or force a different game-text language. Keep all original `.big` archives and
the complete `Data`/video directories. No full payload/checksum or video-decoder
verification is performed: an accepted index alone does not certify every
asset. The launch step adds the port's fonts/configuration and Window overrides
without deleting retail archives. Use **Select Game Folder** in advanced Setup
to reopen the assistant without discarding the current configuration.

## Set up the tabletop board

The game starts in tabletop mode. Open `UI -> Set up play space` in the main
menu or during a supported Skirmish when you want to place it again. Follow one
of these paths:

1. **Free board** places a board freely in the room using the laser pointer.
2. **Real surface** loads room data after explicit confirmation. Cyan outlines
   show detected surfaces, orange shows a board footprint that fits, and red
   means the surface is too small.
3. **Manual height** records the controller height with the trigger; aim at the
   desired position and confirm a second time.

Surface detection is optional, session-based placement, not a permanent
furniture anchor. The current preview can restore the last confirmed layout. A
future XR milestone will use a safe fallback directly in front of the player
when a new room has no saved surface; until then, use **Set up play space** if
the restored layout is inconvenient.

Open `UI -> Windows` to choose **Table** or **Build window** as the editing
target. An orange outline and check mark identify the selected object. One Grip
moves and tilts it; both Grips scale it uniformly. With no Grip held, the
pointing-hand stick changes size and the other-hand stick changes distance.
Press the pointing-hand stick to switch between the two windows. **Reset**
returns the selected window in front of the player. **Finish**, **Commands**, or
Back exits editing.

## Controller controls

The default layout is right-handed. Open `UI -> View` to switch to left-handed
controls; the pointing hand, off hand, and A/B/X/Y roles are swapped logically.

| Input | Function |
|---|---|
| Right Trigger, short press | Select a unit or use the context order at the laser target |
| Right Trigger, hold and drag | Draw a selection rectangle; release to select |
| Left Grip while pressing | Add to the selection or remove one unit |
| Left Thumbstick | Pan the map |
| Right Thumbstick left/right | Rotate the map or camera |
| Right Thumbstick up/down | Zoom in or out |
| Right Grip | Cancel a targeted order; otherwise clear the selection |
| A | Toggle the Commands window |
| B or the system menu | Open or close the game menu |
| Left Grip + Right Stick while placing | Rotate a building preview before placement |

Select units first. Then choose **Move**, **Attack move**, or **Guard** in the
Commands window and point at a target on the table. **STOP** and **Scatter** act
immediately. To create a group, select units, choose **Save selection**, and
press a number. Press that number later to recall the group. To extend a group,
select more units, choose **Add to selection**, press the number, and choose
**Save selection** again. **Commands -> Help** opens the same four-page guide in
the app.

## Language and graphics

On first launch, the XR interface follows the Quest system language: German for
a German system, English otherwise. The language of the game data is separate
and switches to German only when a German language data set is available. The
public product name remains **Generals: Zero Hour XR** in every language.

Quest 3 defaults are **Balanced** resolution, **Light** shadows, and **Multiview**
stereo. The performance measurement overlay is off during normal play. Campaign
and offline AI Skirmish are the supported XR play paths. Human LAN or Internet
multiplayer, replay XR access, and keyboard/mouse support remain separate
roadmap items.

## Troubleshooting

- **Black or missing start screen:** open Setup again and select the folder
  containing `INIZH.big`; then check the base archives.
- **Board is outside the view:** choose `UI -> Set up play space -> Free board`
  or use `UI -> Windows -> Reset`.
- **No room surfaces:** grant scene permission in Horizon OS and run **Load room
  data** again. Free board and manual height are alternatives.
- **An update appears to reset settings:** verify that you used `install -r`
  and did not uninstall the previous package. The application ID is unchanged.
- **Diagnostics:** enable only the requested diagnostic marker in Setup,
  reproduce the issue, and share the log from **View Logs -> Share**.

## Build from source

```sh
git clone https://github.com/Cesarus85/Generals-Zero-Hour-XR.git
cd Generals-Zero-Hour-XR
git submodule update --init --recursive
./scripts/build/android/build-android-zh.sh
./scripts/build/android/package-android-zh.sh
```

The XR APK is written to
`build/apk/Generals-Zero-Hour-XR.apk`. Requirements, reproducible build notes,
and Android architecture are documented in
[`docs/port/ANDROID_PORT.md`](../port/ANDROID_PORT.md). The build scripts never
fetch or redistribute the original game archives.
