# Generals: Zero Hour XR 1.2.34 preview

This preview adds experimental LAN multiplayer between two Meta Quest 3
headsets, with the tabletop view in LAN matches, on top of the 1.2.33
command-console release.

## Changes since 1.2.33

- LAN matches between two Quest 3 headsets running this same version stay synchronized. Tested with two Quest 3 headsets on one Wi-Fi network.
- LAN matches now use the same tabletop view as Campaign and Skirmish.
- The LAN lobby now lists games hosted by another Quest. Direct Connect by IP address also still works.
- The native keyboard no longer reopens after Enter when you then press another button, such as Direct Connect's connect button.
- Simulation math now gives identical results on every device running this version.
- A mismatch between players is now reported reliably. It was previously missed in some two-player slots.
- New optional Setup switch "LAN sync checkpoints" writes synchronization checkpoints to the game log for bug reports. It does not change gameplay.

## Scope and requirements

- Meta Quest 3; native ARM64 OpenXR build.
- LAN multiplayer is an experimental preview. Every player must run 1.2.34 with identical game files.
- Matches against the PC version of Zero Hour (Steam or other), or against older XR versions, are not supported and will report a mismatch.
- Internet multiplayer and replays remain outside this preview. Ground View stays limited to Campaign and offline Skirmish.
- Supply your own legally obtained complete Generals and Zero Hour game files. No retail game data is included.
- This is an independent community project, not an Electronic Arts product or endorsement.

APK SHA-256: `20f987324b14e27e4ed7dd8ccc149af27883de830bc7a1066f53265baf097d60`

See the repository README and Quest installation guide for setup and controls.
