# PLAN-027: voice chat for Quest LAN matches

**Status:** Roadmap. Nothing is implemented. Added 2026-09-26 at the owner's request.

**Decision 2026-09-26:** the owner schedules Option B (built-in push-to-talk) for
later, after the 1.2.35 release. Until then, Option A remains the no-code
interim option.

## Finding

Generals: Zero Hour has no voice chat, and the codebase contains none (LAN,
GameSpy or GeneralsOnline). Two paths exist.

### Option A: Meta system calls/parties (no code, test first)

Horizon OS party voice chat continues when users switch apps. Two players who
follow each other start a call from the Navigator (Quick controls → Chats →
Call) and then launch the game. The app does not use Meta's System VoIP API,
so it should not suppress the call. Unverified on this app: check that the
call audio stays audible over the game's OpenAL output and survives entering
and leaving a match.

References:
[Parties and Party Chat](https://developers.meta.com/horizon/documentation/unity/ps-parties/),
[Calls on Meta Quest](https://www.meta.com/help/quest/3507156169537057/).

Meta's Platform SDK VoIP is
[deprecated](https://developers.meta.com/horizon/documentation/unreal/ps-voip/)
and needs a store-registered app ID. It is not an option for this sideloaded
build.

### Option B: built-in LAN voice (implementation candidate)

Scope: push-to-talk voice between players in the same LAN lobby/match.

- **Capture:** Android `AudioRecord` (or Oboe) with the `VOICE_COMMUNICATION`
  source/preset, which brings the platform's echo cancellation and noise
  suppression. The Quest speakers sit close to the microphones; raw OpenAL
  capture would echo.
- **Codec:** Opus (vcpkg `opus`), 20 ms frames, mono, about 24 kbit/s.
- **Transport:** its own UDP socket and port next to the lobby (8086) and match
  (8088) ports. It is never part of lockstep commands or CRCs, so it cannot
  cause desyncs. Unicast to the lobby slot IPs, sequence numbers and a small
  jitter buffer.
- **Playback:** one OpenAL source per remote player on the existing audio device.
  Optionally duck game audio slightly while someone speaks.
- **Controls:** a push-to-talk controller button; a speaking/muted indicator
  per player on an XR panel; mute per player; a Setup/menu toggle, default off.
- **Permission:** `RECORD_AUDIO` runtime prompt, requested only when voice is
  first enabled, not at launch.
- **Compatibility:** same-version Quests only at first. The packet format is
  versioned, so a future PC port build could join.

Effort estimate: about 3-5 working days including device tests with two
headsets. Main risks: permission prompt behavior inside the immersive session,
echo/latency tuning, and a free controller button for push-to-talk.

## Acceptance (Option B)

- No change to simulation, lobby messages or CRCs (LAN snapshot pair agrees).
- Two Quests: intelligible speech both ways, under ~250 ms mouth-to-ear, no
  audible echo with game audio playing.
- Push-to-talk, mute and default-off work; denying the permission leaves the
  game fully playable.
- A 15-minute match with voice active shows no frame-time regression.

## Order

1. Release 1.2.35 first (horizon and panel fixes plus the owner's next 1-2 features).
2. Then implement Option B on its own branch from main, with the acceptance gates above.
3. Option A can be used at any time as an interim; it needs no build.
