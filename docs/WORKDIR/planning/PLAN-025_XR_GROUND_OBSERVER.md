# PLAN-025: passive XR ground observer prototype

**Status:** authorized for implementation, 2026-09-17

## Goal and boundary

Let the player choose a visible outdoor ground position on the tabletop,
enter a stereoscopic human-scale view, watch an offline Skirmish with normal
head tracking, and return to the identical tabletop workspace with one button.
The first experiment proves presentation, comfort and asset quality. It does
not add unit possession, walking, follow-camera motion, multiplayer, replay
support, new game rules or replacement assets. Campaign cinematics and menus
remain on the existing presentation path; entry is limited to live offline
Skirmish for this first prototype. The official 1.2.17 release is preserved.

## User flow

1. An English/German Ground view / Bodenansicht action in the existing spatial
   UI arms a placement ray and explains that the next ground click chooses the
   observer's location. Cancel is directly available.
2. Only a valid, visible map-ground destination is accepted. Reject locations
   outside the map or obscured by shroud; avoid putting the viewer inside a
   structure. Do not change the selection or issue a gameplay order.
3. Fade briefly into a fixed observer location, with approximately human eye
   height and an explicit world-units/metre conversion. Head rotation and short
   real-world translations remain one-to-one at the chosen scale. No automatic
   camera movement, roll, bobbing, artificial locomotion or unit following.
4. Hide the tabletop plinth and gameplay panels while observing. Use an opaque
   immersive scene/background rather than room passthrough through unrendered
   regions. A small readable English/German hint explains the direct return
   button. B (logical cancel, mirrored for left-handed mode) returns immediately;
   an additional clearly documented emergency return is acceptable.
5. Fade back to the saved tabletop arrangement, camera and UI visibility. Held
   buttons must not produce a click/order on either transition. The mode is
   never persisted across process or match changes.

## Implementation contract

- Add an explicit XR-only observer state and mapping mode. Reuse the current
  per-eye OpenXR poses, multiview and separate render camera. Do not reposition
  simulation objects or mutate the tactical camera to pretend to be the viewer.
- Replace board-volume CPU/shader clipping with bounded viewer-relative
  visibility for this mode. Both eye frusta must be covered. Terrain coverage
  must follow the observer rather than the tabletop tactical-camera target;
  keep allocations and far visibility bounded. Preserve fog/shroud rules.
- Preserve the tabletop mapping, picking, performance defaults and source
  behavior when the observer state is inactive. No persistent setting migration
  or permanent panel transform changes are needed.
- Suppress all gameplay input while armed/observing, including gestures, target
  commands, groups, minimap orders and thumbstick camera navigation. Clear
  pending input on entry/exit and require release before rearming.
- Match end, loading, movie/scripted-camera transition, focus/tracking loss and
  invalid render readiness must leave observer mode safely. Never leave a
  frozen immersive frame with no return path. Preserve the XR result card.
- Keep the state transition and mapping maths testable on the host. Cover
  transform scale/orientation, terrain/map validation, mode guards, return and
  held-button suppression. Test real production integration where practical,
  not only a copy of the intended algorithm.
- Existing relevant XR regressions must pass, including workspace, console,
  panel text, world/height/board and loading/endgame. The parent agent owns the
  incremental ARM64 build, release-key-compatible test packaging and review.
- Documentation stays English; UI/help supports English and German. Update
  XR_CURRENT_STATUS.md and the current diary with evidence and open gates.

## Acceptance and follow-up

- [ ] Code and focused host checks pass.
- [ ] Tabletop regressions pass and Android native/package build succeeds.
- [ ] Test APK version/hash/signature are recorded separately from 1.2.17.
- [ ] In-headset: choose ground, enter, 360-degree look, lean, return; no orders
      or changed workspace; verify both handedness modes and loss/return cases.
- [ ] In-headset: inspect infantry, vehicles, buildings, slopes, effects,
      horizon and terrain edges; record practical frame rate/comfort.

Only after the above decide whether to polish this mode, broaden to campaigns,
or add a stabilized unit-follow camera. Stock model/detail limitations and the
existing RTS audio presentation are acknowledged prototype questions.
