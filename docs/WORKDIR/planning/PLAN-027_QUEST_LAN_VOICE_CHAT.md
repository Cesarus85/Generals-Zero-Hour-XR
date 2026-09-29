# PLAN-027: voice chat for Quest LAN matches

**Status:** Implemented 2026-09-29 on `codex/xr-high-default-voice`; device test with two Quests pending.

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

## Implementation, 2026-09-29

The owner chose **voice activation** instead of push-to-talk, because every
controller button already has a function. Deviations from the plan above:
uncompressed 16 kHz mono PCM16 instead of Opus (about 256 kbit/s per active
speaker, fine on a LAN; Opus is a follow-up for internet play), and no
per-player mute yet.

- `android/.../LanVoiceChat.java`: capture with `AudioRecord`
  (`VOICE_COMMUNICATION` source, `AcousticEchoCanceler` and `NoiseSuppressor`
  when available), 20 ms frames, adaptive energy gate with a 300 ms hangover.
  Unicast UDP on port **8094** (lobby 8086 and match 8088 stay untouched);
  packets are `GXV1` + seq + sample count + PCM, and only accepted from the
  current game's peer addresses. Playback uses one
  `USAGE_VOICE_COMMUNICATION` `AudioTrack` with per-peer jitter queues
  (primed at 2 frames, capped at 10) and clipped mixing. Sessions and capture
  runs are separate objects that end on their own, so the XR thread never
  waits.
- `XrHelloActivity.micPermission`: `RECORD_AUDIO` runtime prompt, requested
  once when the player switches voice chat on (never at launch). Voice is
  shut down in `onDestroy`.
- Native: `XrGameBoot_LanVoicePeers` reads the human slots of
  `TheLAN->GetMyGame()` (read-only; own IP excluded). `XrVoiceChat.h`
  publishes peers and mode about twice a second over JNI and reads status
  bits (active, you speak, a player speaks, no microphone).
- UI: Display page → "Sprachchat: AUS / AN / STUMM" (menu id 18; 17 is the
  global close). The layout stores the choice (v12 field, default off). A
  non-interactive plate below the UI/Commands/Ground View column shows
  SPRACHE / DU SPRICHST / SPIELER SPRICHT / STUMM / MIKRO FEHLT while a
  session is active.
- Tests: menu routing (Off→On asks for the microphone→Muted→Off), layout v12
  voice persistence and rejection of invalid values, menu geometry, bilingual
  panel text.

Device acceptance still open: two Quests in a LAN lobby and match; speech in
both directions; echo with game audio; latency; permission prompt in the
immersive session; no frame-time change.
