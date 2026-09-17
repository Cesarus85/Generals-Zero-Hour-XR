# PLAN-025: passive XR ground observer prototype

**Status:** isolated prototype implemented, reviewed and installed; host/native/package
checks pass, headset acceptance pending, 2026-09-17

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

- [x] Code and focused host checks pass. Source checkpoint `413eab1` on
      `codex/xr-ground-observer-prototype`, started from `66cdfae` (official
      `main` baseline `6af7abd`). `xr-ground-observer-test` covers production
      mapping/state and source-order invariants; the updated production height,
      interaction, loading-presenter and bilingual panel tests pass. The
      tabletop workspace, console, endgame, trigger, scene, world, height,
      board, input and loading host regressions also pass. No Android build or
      worn-headset claim is implied by these checks.
- [x] Tabletop regressions pass and Android native/package build succeeds.
      Final native source is `d955d29`; XR release and both Android debug
      flavors build locally. Parent independently reran observer (38),
      interaction (145), loading presenter (152) and bilingual panel (20,693)
      checks successfully. Hosted CI was not requested.
- [x] Test APK version/hash/signature are recorded separately from 1.2.17.
      Version 10218 / `1.2.18-xr-ground-observer`, file
      `build/apk/Generals-Zero-Hour-XR-1.2.18-ground-observer.apk`, SHA-256
      `88d8e4e87ebedd28cb01712aecd322b2c518fd87bfd0a1b5c75cdd989887ba1e`.
      Android v3 signature uses the established production certificate;
      package identity, ARM64-only ABI, non-debuggable manifest, no retail
      archives and `RTS_DEBUG_CHEATS=OFF` verified. Installed as an in-place
      update on Quest 3 `2G0YC5ZG9609PY`; package manager reports 10218 and
      the earlier first-install date. Worn-headset acceptance remains open.
- [ ] In-headset: choose ground, enter, 360-degree look, lean, return; no orders
      or changed workspace; verify both handedness modes and loss/return cases.
- [ ] In-headset: inspect infantry, vehicles, buildings, slopes, effects,
      horizon and terrain edges; record practical frame rate/comfort.

Only after the above decide whether to polish this mode, broaden to campaigns,
or add a stabilized unit-follow camera. Stock model/detail limitations and the
existing RTS audio presentation are acknowledged prototype questions.

## Implemented prototype contract and open gates

The View window exposes a bilingual Ground view action only while a live
offline Skirmish can render stereo. Arming captures all controller/game input;
a terrain-only ray accepts visible clear ground inside map margins, with
blocking drawable, slope and nearby-height checks. Entry fixes a world-to-room
anchor at 10 game units per metre and a 1.65 m nominal eye height; subsequent
tracked head motion remains physical. The observer uses the existing per-eye
stereo render camera, a 60 m render far plane, viewer-relative CPU visibility,
bounded terrain coverage and a shader path without the tabletop volume clip.
It draws an opaque background and hides the board, build/command/settings
panels and decorations. A persistent bilingual card explains the logical
B/cancel return (Y in left-handed mode); entry and exit use a brief black
transition veil. All mode state is session-only; no layout save is performed.

The test build, signature, version/hash and installation are intentionally
owned by the parent build/review worktree. Physical acceptance remains open:
headset stereo and horizon quality, scale, 360-degree head movement and lean,
layout restoration, all input gates, left-handed return, renderer recovery,
performance/comfort and model/detail judgment. This prototype does not
authorize release, multiplayer, campaigns or replays.
