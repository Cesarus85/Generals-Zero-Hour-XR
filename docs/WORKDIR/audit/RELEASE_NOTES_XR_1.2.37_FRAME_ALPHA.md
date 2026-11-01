# Generals: Zero Hour XR 1.2.37 Steam Frame alpha 1

**Very early alpha for Valve's Steam Frame.** The game runs on the Frame
through its Android layer (Lepton) and SteamVR's OpenXR runtime. Expect rough
edges; this build is for testers who are comfortable with ADB.

The same APK still runs on Meta Quest 3, but Quest players should stay on
**1.2.36**. This alpha has not been re-tested on Quest.

## What works on Steam Frame

- **Tabletop in your room:** SteamVR composites the room behind the game, so the board
  and windows float in your real surroundings. Ground View stays fully
  immersive.
- **Controllers:** both controllers work through SteamVR's Touch-compatible
  binding. As on Quest, the laser comes from the hand you point with.
- **Smooth:** about 70-72 fps in a match at the High resolution tier
  (1920 px per eye), and the engine boots in about 2 seconds.
- Campaign, Skirmish, menus, board placement and orders were checked on a
  real Frame.

## Install (Steam Frame)

1. Turn on Developer Mode on the Frame and start **Lepton Development**, so
   that the Android layer accepts ADB over Wi-Fi.
2. From a computer with Android platform-tools:

   ```sh
   adb connect <frame-hostname-or-ip>:5555
   adb install -r Generals-Zero-Hour-XR.apk
   ```

3. Copy your own Zero Hour installation to the Frame, for example to
   `Download/Command & Conquer Generals - Zero Hour`. Put the base Generals
   files in a subfolder of it (for example `ZH_Generals`) or next to it:

   ```sh
   adb push "Command & Conquer Generals - Zero Hour" /sdcard/Download/
   ```

4. Launch **Generals: Zero Hour XR**. On the first start the game-data
   assistant opens as a window: choose the Zero Hour folder, then
   **Use these folders and play**.

   If the assistant cannot read the files, grant file access over ADB and
   start the app again:

   ```sh
   adb shell appops set com.generalsx.zerohour.xr MANAGE_EXTERNAL_STORAGE allow
   ```

## Fixed for the Frame

- The game requests the generic OpenXR headset category and falls back to
  OpenXR 1.0, which SteamVR requires.
- Room view through SteamVR's alpha-blended environment instead of a black
  void (Quest keeps its own passthrough layer).
- The folder picker's **Cancel** / **Use This Folder** buttons are no longer
  hidden under Lepton's navigation bar.

## Known limitations

- **No text entry yet:** the Frame has no Meta system keyboard, so typing
  (Direct Connect IP address, player name, chat) does not work. LAN lobby
  discovery does not need typing.
- **LAN and voice chat between Frame and Quest are untested.** Frame vs Frame
  is untested too. All players need this same build.
- Refresh rate (about 72 Hz) and foveated rendering are not tuned for the
  Frame yet.
- Internet multiplayer and replays remain outside the XR preview.

## Scope and requirements

- Steam Frame with Developer Mode and Lepton; Meta Quest 3 remains the main
  target (stay on 1.2.36 there).
- Supply your own legally obtained complete Generals and Zero Hour game files.
  No retail game data is included.
- This is an independent community project, not an Electronic Arts or Valve
  product or endorsement.

APK SHA-256: `SHA256_PENDING`

Feedback from Frame testers is very welcome. Please include the Frame's
SteamOS version.
