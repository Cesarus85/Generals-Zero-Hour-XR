# Generals: Zero Hour XR 1.2.36 preview

Talk to your opponent in LAN matches, and enjoy a sharper tabletop by default.

## New: LAN voice chat (experimental)

- **Voice-activated:** no button to hold. Your microphone only sends while you
  are actually speaking, and silence is not transmitted. Voice uses the
  headset's echo cancellation and noise suppression.
- **Turn it on:** UI → Display → **Voice chat: OFF / ON / MUTED**. It is off
  by default. The first time you switch it on, the Quest asks for microphone
  permission. **MUTED** lets you listen without sending.
- **See who is talking:** while voice chat is active, a small plate below the
  UI, Commands and Ground View shortcuts shows VOICE, YOU SPEAK, PLAYER
  SPEAKS, MUTED or NO MICROPHONE.
- **Where it works:** in the LAN lobby and during LAN matches, with every human
  player of that game. All players need 1.2.36 on the same local network.
- Voice runs on its own network channel (UDP port 8094) and is completely
  separate from the game's synchronization, so it cannot cause a mismatch.
- This is a first version: there is no per-player mute yet, and voice is sent
  uncompressed, which is fine on a home network. Feedback is welcome.

## Sharper by default

- **High resolution (1920 px per eye) is now the default**, made affordable by
  the rendering savings in 1.2.35. If you were on Balanced, you are moved to
  High once; Ultra+ stays as it is. You can switch back anytime under UI →
  Display → Resolution.

## Unchanged

- Everything from 1.2.35: single-pass fog rendering, calmer Commands console,
  stable Ground View horizon.
- Experimental LAN multiplayer between Quest 3 headsets running the same
  version, with tabletop view, lobby discovery and Direct Connect.

## Scope and requirements

- Meta Quest 3; native ARM64 OpenXR build.
- LAN multiplayer and voice chat are experimental previews. Every player must
  run 1.2.36 with identical game files; matches against older XR versions or
  the PC version of Zero Hour (Steam or other) are not supported.
- Internet multiplayer and replays remain outside this preview. Ground View
  stays limited to Campaign and offline Skirmish.
- Supply your own legally obtained complete Generals and Zero Hour game files.
  No retail game data is included.
- This is an independent community project, not an Electronic Arts product or
  endorsement.

APK SHA-256: `<filled in after signing>`

See the repository README and Quest installation guide for setup and controls.
