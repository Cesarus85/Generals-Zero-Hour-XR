# Generals: Zero Hour XR 1.2.35 preview

This release makes the tabletop smoother and sharper: less rendering work per
frame, a calmer Commands console and a stable horizon in Ground View.

## Performance

- **Fog of war is now rendered in a single pass.** Rocks, bushes, props and
  partly fogged buildings used to be drawn twice every frame: once normally
  and once more to darken them with the fog. They are now drawn once, with the
  fog applied in the same step. In our on-device CPU profile, these extra fog
  draws dropped by about two thirds, and rendering's share of the game thread
  fell from about 60% to 48%. The fog looks the same as before.
- **Trees are no longer rebuilt with every head movement.** Small head
  movements used to rebuild and relight all trees each frame. This per-frame
  cost is practically gone.
- The gain depends on the scene. It is largest on maps with a lot of scenery in
  fogged or explored areas, and makes higher resolution settings more
  affordable.

## Sharper and calmer visuals

- **Commands console no longer shimmers.** Its fine borders, small text and
  scanline pattern flickered with head movement at normal viewing distance.
  All controller panels now use mipmapped, anisotropic filtering tuned to keep
  text sharp.
- **Stable horizon in Ground View.** Distant hills no longer appear at the edge
  of your vision and vanish when you look straight at them. The horizon is now
  a fixed 60 m circle around you, whichever way you look.

## Unchanged

- Experimental LAN multiplayer from 1.2.34 between Quest 3 headsets running the
  same version, with tabletop view, lobby discovery and Direct Connect.
- Campaign, Skirmish, controller commands and game-data import.

## Scope and requirements

- Meta Quest 3; native ARM64 OpenXR build.
- LAN multiplayer stays an experimental preview. Every player must run 1.2.35
  with identical game files; matches against 1.2.34 or against the PC version
  of Zero Hour (Steam or other) are not supported.
- Internet multiplayer and replays remain outside this preview. Ground View
  stays limited to Campaign and offline Skirmish.
- Supply your own legally obtained complete Generals and Zero Hour game files.
  No retail game data is included.
- This is an independent community project, not an Electronic Arts product or
  endorsement.

APK SHA-256: `<filled in after signing>`

See the repository README and Quest installation guide for setup and controls.
