# Audio dropout audit — "sound cuts out in places" on Android

## What this is

A player reported that sound "cuts out in places" during play on Android. This
document enumerates every mechanism in the shipping audio path by which a sound
the game asked to play can stop early, never start, or glitch, sorted into four
classes that need completely different fixes.

### What was examined

- `Core/GameEngineDevice/Source/OpenALAudioDevice/` — `OpenALAudioManager.cpp`,
  `OpenALAudioStream.cpp`, `OpenALAudioCache.cpp`, plus
  `Core/GameEngineDevice/Include/OpenALAudioDevice/*.h`. This is the audio device
  that ships on Android: `Core/GameEngineDevice/CMakeLists.txt:249-266` adds these
  three sources when `SAGE_USE_OPENAL` is on, and
  `GeneralsMD/Code/GameEngineDevice/CMakeLists.txt:216-220` explicitly *excludes*
  the alternative `GeneralsMD/Code/GameEngineDevice/Source/OpenALAudioManager.cpp`
  in that case. `build/android-vulkan/CMakeCache.txt:869` has
  `SAGE_USE_OPENAL:BOOL=ON`, and the shipped `libmain.so` contains the Core
  implementation's symbols (`_ZN18OpenALAudioManagerC1Ev`,
  `_ZN17OpenALAudioStream6updateEv`). The GeneralsMD-side file is dead on this
  platform and is not analysed here.
- `Core/GameEngine/Source/Common/Audio/` — `GameAudio.cpp`, `GameSounds.cpp`,
  `GameMusic.cpp`, `AudioEventRTS.cpp`.
- `Core/GameEngine/Source/Common/INI/INIAudioEventInfo.cpp`,
  `Core/GameEngine/Include/Common/AudioSettings.h`,
  `GeneralsMD/Code/GameEngine/Source/Common/GameLOD.cpp`.
- `Core/GameEngineDevice/Source/VideoDevice/FFmpeg/FFmpegFile.cpp` (the decoder
  behind both the sample cache and the streams).
- Lifecycle/platform glue: `GeneralsMD/Code/GameEngineDevice/Source/SDL3GameEngine.cpp`,
  `GeneralsMD/Code/Main/SDL3Main.cpp`,
  `android/app/src/main/java/com/generalsx/zerohour/`,
  `android/app/src/main/jniLibs/arm64-v8a/libopenal.so`, `cmake/openal.cmake`.
- `Core/GameEngineDevice/Source/MilesAudioDevice/MilesAudioManager.cpp` was read
  only as the reference for what the OpenAL port was supposed to reproduce.

### What could not be determined from source alone

- **The actual limit numbers.** `SampleCount2D`, `SampleCount3D`, `StreamCount`,
  `OutputRate`, `MinSampleVolume` and `AudioFootprintInBytes` are all parsed from
  `Data\INI\AudioSettings` (`GameAudio.cpp:101-125`,
  `GameAudio.cpp:218`), which ships inside the game's `.big` archives and is not
  in this repository. Per-event `Limit`, `Priority`, `Control` and `LoopCount`
  likewise come from `Data\INI\SoundEffects` / `Voice` / `Speech`
  (`INIAudioEventInfo.cpp:135-142`, `GameAudio.cpp:220-233`). Everything below
  cites where the value is *used*, and gives the engine's own comparable
  hardcoded defaults where they exist.
- **Codec frame sizes**, hence how many milliseconds of audio the stream queue
  actually holds. This is decided by FFmpeg per file format, at runtime.
- **Which OpenAL Soft backend actually opens**, and at what rate/period. The
  shipped library only *contains* OpenSL ES (see class D), but what the OpenSL
  backend negotiates with AudioFlinger is a runtime fact.
- **Whether any of the mechanisms below actually fire in the reported sessions.**
  None of this can be settled by reading; see "What to measure on-device".

---

## A. Engine-level voice limiting and priority culling

This is the 2003-era throttling, and it is intact. Every gate below runs before a
sound ever reaches OpenAL.

### The gates, in the order a sound passes through them

`AudioManager::addAudioEvent()` (`GameAudio.cpp:384-486`) rejects first:

- Empty name or `"NoSound"` → `AHSV_NoSound` (`GameAudio.cpp:386-388`).
- Category switched off — music/sound/3D-sound/speech (`GameAudio.cpp:406-423`).
- **`getDisallowSpeech()`** — while an uninterruptible streamed line is playing,
  *every* other `AT_Streaming` event is rejected outright
  (`GameAudio.cpp:418-419`). See the stuck-flag history in class C.
- Not for the local player (`GameAudio.cpp:433-438`).
- **Volume below `m_audioSettings->m_minVolume`** → `AHSV_Muted`
  (`GameAudio.cpp:462-469`).

Music then goes straight to `MusicManager` (`GameAudio.cpp:471-474`,
`GameMusic.cpp:98-119`) with no further gating. **Everything else — sound effects
*and* streamed speech — goes to `SoundManager::addAudioEvent()`
(`GameAudio.cpp:476-479`), which calls `canPlayNow()` and silently drops the
event if it returns false (`GameSounds.cpp:137-148`).**

`SoundManager::canPlayNow()` (`GameSounds.cpp:157-271`) is the real throttle:

1. **Distance cull** — for positional, non-`ST_GLOBAL`, non-`AP_CRITICAL` events,
   if listener-to-source distance `>= m_maxDistance` the sound is dropped
   (`GameSounds.cpp:171-184`). `m_maxDistance` is per-event INI data.
2. **Shroud cull** — `ST_SHROUDED` events under non-clear shroud are dropped
   (`GameSounds.cpp:188-195`).
3. **Voice cull** — `violatesVoice()`: if the event is `ST_VOICE` and its owning
   object already has a voice playing, it is dropped unless the event has
   `AC_INTERRUPT` (`GameSounds.cpp:199-213`, `274-280`, `283-286`). This is
   enforced against both the 2D and 3D playing lists
   (`OpenALAudioManager.cpp:2053-2079`).
4. **Per-event limit** — `TheAudio->doesViolateLimit()`
   (`GameSounds.cpp:215-221`, implemented at `OpenALAudioManager.cpp:1940-2020`).
   `m_limit` comes from the event's `Limit` INI field
   (`INIAudioEventInfo.cpp:138`); `0` means unlimited
   (`OpenALAudioManager.cpp:1943-1946`). The count includes both currently
   playing instances *and* queued play requests
   (`OpenALAudioManager.cpp:1976-1991`). When at the limit, the **oldest**
   instance of that event is marked for killing via `setHandleToKill()`
   (`OpenALAudioManager.cpp:1955-1959` / `1969-1973`), and `playAudioEvent()`
   later tears that instance down mid-playback
   (`OpenALAudioManager.cpp:889-902` for 3D, `948-961` for 2D). **This is a
   deliberate "kill oldest" policy and it does cut sounds off mid-sample.**
5. **Channel availability** — `getNumAvailable3DSamples()` /
   `getNumAvailable2DSamples()` must be `> 0` (`GameSounds.cpp:227-245`).
6. **Priority** — if no channel is free, the sound is admitted only if something
   of strictly lower priority is playing (`GameSounds.cpp:247-250`,
   `OpenALAudioManager.cpp:2147-2184`), or if it is `AC_INTERRUPT` and an
   instance of itself is already playing (`GameSounds.cpp:252-266`).
   Otherwise: dropped.

Delayed requests are re-checked against `canPlayNow()` when they finally fire
(`OpenALAudioManager.cpp:2374-2395`, `2716-2735`), so a sound admitted at request
time can still be culled a few frames later.

### Where the channel counts come from

`getNumAvailable2DSamples()` / `getNumAvailable3DSamples()` are
`m_num2DSamples - m_playingSounds.size()` and
`m_num3DSamples - m_playing3DSounds.size()`, clamped at 0
(`OpenALAudioManager.cpp:1920-1931`). `m_num2DSamples` / `m_num3DSamples` /
`m_numStreams` are set once, in `initSamplePools()`, from
`AudioSettings::m_sampleCount2D` / `m_sampleCount3D` / `m_streamCount`
(`OpenALAudioManager.cpp:3078-3090`).

The values themselves are INI data (not found in this repo). The nearest
in-repo numbers are `GameLOD.cpp:89-91` (`m_sampleCount2D=6`,
`m_sampleCount3D=24`, `m_streamCount=2`) and `GameLOD.cpp:255-257` (identical for
the "very high" preset) — but **GameLOD never applies them**: the assignment is
commented out with a `///@todo` at `GameLOD.cpp:586-591`. So the effective values
are exactly whatever `AudioSettings.ini` says, and the LOD slider does not change
them.

`m_numStreams` is read back by `getNumStreams()`
(`OpenALAudioManager.cpp:1934-1937`) but is **never used to limit anything** —
`playAudioEvent()` creates a new `OpenALAudioStream` unconditionally
(`OpenALAudioManager.cpp:802`). The stream count is effectively unenforced.

### A hard-fail mode worth ruling out first

`initSamplePools()` is only called from `selectProvider()`
(`OpenALAudioManager.cpp:1845`), and `selectProvider()` returns early if the
requested index equals the currently selected one
(`OpenALAudioManager.cpp:1763-1766`). The constructor initialises
`m_selectedProvider(PROVIDER_ERROR)` (`OpenALAudioManager.cpp:110`;
`PROVIDER_ERROR == 0xFFFFFFFF`, `GameAudio.h:77`), and `openDevice()` calls
`selectProvider(TheAudio->getProviderIndex(m_pref3DProvider))`
(`OpenALAudioManager.cpp:1553`). `getProviderIndex()` returns `PROVIDER_ERROR`
for any name that is not the single hardcoded provider string
`"Miles Fast 2D Positional Audio"` (`OpenALAudioManager.cpp:1745-1754`,
`123-125`). `m_pref3DProvider` comes from the user's `Options.ini`
`3DAudioProvider` key, falling back to `AudioSettings.ini`'s `Preferred3DSW`
(`GameAudio.cpp:1152`, `OptionPreferences.cpp:507-513`).

**Consequence:** if that preference string does not match exactly,
`selectProvider(PROVIDER_ERROR)` early-returns, `initSamplePools()` never runs,
`m_num2DSamples` and `m_num3DSamples` stay 0, and `canPlayNow()` rejects every
sound effect *and* every streamed speech line for the whole session. Music still
plays (it bypasses `SoundManager`). This produces "music only, no effects, no
speech", not intermittent dropouts — but it is cheap to rule out and it is a
silent failure.

`selectProvider()` also returns early if `isOn(AudioAffect_Sound3D)` is false
(`OpenALAudioManager.cpp:1759-1762`), with the same consequence. At `openDevice()`
time the flag defaults to TRUE (`GameAudio.cpp:151-154`), so this only bites if
3D sound is disabled before the device opens.

### Culling of sounds already playing

- **3D minimum-volume cull**: each frame, a playing 3D sound whose effective
  volume is below `m_audioSettings->m_minVolume` is released immediately unless
  it is `ST_GLOBAL` or `AP_CRITICAL` (`OpenALAudioManager.cpp:2490-2501`).
  Note that `getEffectiveVolume()` (`OpenALAudioManager.cpp:2872-2902`) is
  *event volume × category volume only* — it does **not** include distance
  attenuation, so this is a volume cull, not a distance cull.
- **Dead owner**: if the owning object/drawable no longer exists,
  `AudioEventRTS::getCurrentPosition()` flips `m_ownerType` to `OT_Dead`
  (`AudioEventRTS.cpp:732-752`) and `processPlayingList()` calls
  `stopAudioEvent()` (`OpenALAudioManager.cpp:2485-2489`).
- **Fade-out**: music stopped with `AHSV_StopTheMusicFade` is moved to
  `m_fadingAudio` and killed after `m_fadeAudioFrames` frames
  (`OpenALAudioManager.cpp:1029-1032`, `2625-2671`).
- **Volume-zero sweep**: `removeAllDisabledAudio()` releases every playing
  sound/3D sound/stream whose event volume is exactly `0.0f`
  (`OpenALAudioManager.cpp:2325-2367`).
- **Cache eviction** (see class B) can also kill a playing sound.

### Where the port diverges from Miles

- Miles enforced the channel cap a second time at the device layer: it took a
  handle from a preallocated pool and, on failure, called
  `killLowestPrioritySoundImmediately()` before giving up
  (`MilesAudioManager.cpp:714-728`, `778-793`, `2769-2795`). The OpenAL port
  removed the pool entirely (`initSamplePools()` only stores three integers,
  `OpenALAudioManager.cpp:3078-3090`) and calls `alGenSources()` unconditionally.
  `OpenALAudioManager::killLowestPrioritySoundImmediately()` exists
  (`OpenALAudioManager.cpp:2186-2236`) but **has no callers anywhere in the
  tree** — it is dead code. (It also contains a latent bug: the 2D branch erases
  an `m_playingSounds` iterator from `m_playing3DSounds` at
  `OpenALAudioManager.cpp:2227`, which would corrupt both lists' cached sizes and
  hence `getNumAvailable*Samples()` permanently. Because nothing calls it, this
  cannot be the reported symptom today, but it must not be wired up as-is.)
- `stopAudioEvent()` for a 2D or 3D sample only sets `m_requestStop = true`
  (`OpenALAudioManager.cpp:1058-1084`); it never calls `alSourceStop()`. Miles
  set `PS_Stopping` and `processPlayingList()` actually stopped the sample
  (`MilesAudioManager.cpp:2158-2161`). This makes sounds play *longer* than
  intended, not shorter — recorded here for completeness.
- `hasMusicTrackCompleted()` has its body commented out and always returns FALSE
  (`OpenALAudioManager.cpp:1492-1511`). `playStream()`'s `AL_LOOPING` set is also
  commented out (`OpenALAudioManager.cpp:2946-2949`), so music does not loop at
  the OpenAL layer.

---

## B. OpenAL resource exhaustion and swallowed errors

### Source allocation is unchecked

Sources are created per sound, not pooled:

```
ALuint source;                          // OpenALAudioManager.cpp:905 (3D), :964 (2D)
if (!handleToKill || foundSoundToReplace)
{
    alGenSources(1, &source);           // OpenALAudioManager.cpp:908 (3D), :967 (2D)
}
else
{
    source = 0;
}
```

`source` is declared uninitialised and `alGenSources()`'s result is never checked
with `alGetError()`. On failure OpenAL leaves the output untouched, so `source`
holds stack garbage; `if (source)` at `OpenALAudioManager.cpp:914` / `:973` then
passes, `playSample()` / `playSample3D()` issue `alSourcei`/`alSourcePlay` against
an invalid name (all silently failing), and the `PlayingAudio` node is left on the
playing list occupying a channel slot. `sourceIsStopped()`
(`OpenALAudioManager.cpp:99-105`) then calls `alGetSourcei()` on that invalid name,
which leaves `state` uninitialised — the node may be treated as playing forever,
permanently consuming one of the 2D/3D channel slots that class A hands out.

The same omission exists in `OpenALAudioStream`'s constructor
(`OpenALAudioStream.cpp:6-8`): neither `alGenSources()` nor `alGenBuffers()` is
error-checked.

### How close is the source limit?

OpenAL Soft's default is `SourcesMax = 256` (`alc/alc.cpp:3004-3006` of the
openal-soft 1.24.2 tree fetched by `cmake/openal.cmake:32-36`), and
`alGenSources()` raises `AL_OUT_OF_MEMORY` past it (`al/source.cpp:2685-2688`).
The port requests **no** `ALC_MONO_SOURCES` / `ALC_STEREO_SOURCES` attributes
(`OpenALAudioManager.cpp:1533`), so 256 is what it gets. With the engine's own
cap on the order of 6 + 24 + a couple of streams, exhaustion should be
unreachable *unless* sources leak. No leak was found by reading:
`releaseOpenALHandles()` deletes the source and the stream
(`OpenALAudioManager.cpp:1186-1206`), `processFadingList()` and
`processStoppedList()` both release (`OpenALAudioManager.cpp:2625-2688`), and the
Bink stream is a single reused instance (`OpenALAudioManager.cpp:3116-3133`).
The unchecked-`alGenSources` path above is the one way stale nodes can accumulate.

### The audio cache can kill sounds that are currently playing

`loadBufferForRead()` → `OpenALAudioFileCache::getBufferForFile()`
(`OpenALAudioManager.cpp:1165-1168`, `OpenALAudioCache.cpp:86-178`). When the
cache is over budget it calls `freeEnoughSpaceForSample()`
(`OpenALAudioCache.cpp:165-174`). That function first evicts entries with
`m_openCount == 0` (`OpenALAudioCache.cpp:258-270`), and if that is not enough it
**evicts entries with `m_openCount > 0` that are of lower priority than the
incoming sound** (`OpenALAudioCache.cpp:276-292`). Eviction calls
`releaseOpenAudioFile()`, which for an in-use entry calls
`TheAudio->closeAnySamplesUsingFile()` (`OpenALAudioCache.cpp:222-227`), and that
releases every `PlayingAudio` currently using that buffer
(`OpenALAudioManager.cpp:2803-2842`). **A low-priority sound is cut off
mid-playback so a higher-priority one can load.** If nothing can be freed, the
incoming sound simply does not play (`OpenALAudioCache.cpp:168-173`).

Two things make this more likely to fire than the INI suggests:

- **`setMaxSize()` is a no-op.** `OpenALAudioFileCache::setMaxSize()`'s body is
  commented out (`OpenALAudioCache.cpp:212-219`), so
  `m_audioCache->setMaxSize(getAudioSettings()->m_maxCacheSize)` at
  `OpenALAudioManager.cpp:516` does nothing and the budget stays at the
  constructor's hardcoded **14 MiB** (`OpenALAudioCache.cpp:18`).
- **The budget is charged in compressed bytes, not decoded bytes.** The decode
  callback accumulates decoded size into `m_fileSize`
  (`OpenALAudioCache.cpp:48`), but line `OpenALAudioCache.cpp:163` then
  overwrites it with the on-disk file size captured at
  `OpenALAudioCache.cpp:135`. The OpenAL buffers actually hold decoded PCM, so
  real memory use is a multiple of the accounted 14 MiB, and evictions happen at
  a different point than intended in either direction.

There is also a reentrancy hazard the header explicitly describes: the guard
field `PlayingAudio *m_reentrantProcessingAudio` is declared at
`Core/GameEngineDevice/Include/OpenALAudioDevice/OpenALAudioManager.h:236` with a
long comment about `processPlayingList()` → `notifyOfAudioCompletion()` →
`startNextLoop()` → `getBufferForFile()` → `freeEnoughSpaceForSample()` →
`closeAnySamplesUsingFile()` deleting the node the outer loop is standing on.
**The field is never read or written anywhere in the tree** — the `.cpp` half of
that fix is missing. `processPlayingList()` (`OpenALAudioManager.cpp:2398-2586`)
does not set it and `closeAnySamplesUsingFile()`
(`OpenALAudioManager.cpp:2803-2842`) does not check it. The described
use-after-free path is therefore open, and it is reachable exactly when the cache
is full and a looping sound advances — a crash or corruption here would present
as audio stopping.

### Adjacent finding (not a dropout)

`playSample()` (`OpenALAudioManager.cpp:2957-2968`) and `playSample3D()`
(`OpenALAudioManager.cpp:2971-3027`) never set `AL_GAIN`. Gain is only ever
applied by `adjustPlayingVolume()` (`OpenALAudioManager.cpp:1322-1347`), which
runs only when `m_volumeHasChanged` is set — and that flag is set only by
`setVolume()` / `refreshCachedVariables()` (`GameAudio.cpp:741`, `:772`) and
cleared each frame (`OpenALAudioManager.cpp:2573-2575`). So a newly started
sample plays at OpenAL's default gain of 1.0 regardless of the user's sound
volume until the next volume change. This makes sounds too loud, not absent; it
is listed here because it is the same "the port dropped a step Miles did" shape.

---

## C. Streaming and buffer underruns

**This is the part that is structurally exposed to a CPU-bound device.**

### Everything refills on the game thread

There is no audio thread in this port. The chain is:

`GameEngine::update()` → `TheAudio->UPDATE()` (`GameEngine.cpp:1117`, once per
game loop iteration) → `OpenALAudioManager::update()`
(`OpenALAudioManager.cpp:544-552`) → `processPlayingList()` →
`playing->m_stream->update()` (`OpenALAudioManager.cpp:2539`) →
`m_requireDataCallback()` → `ffmpegFile->decodePacket()`
(`OpenALAudioManager.cpp:806-813`) → `av_read_frame` + `avcodec_send_packet` +
`avcodec_receive_frame` (`FFmpegFile.cpp:205-259`) → interleave + `alBufferData`
+ `alSourceQueueBuffers` (`OpenALAudioManager.cpp:816-844`,
`OpenALAudioStream.cpp:38-73`).

**Music and streamed speech are demuxed, decoded, interleaved and queued
synchronously on the game thread, once per rendered frame.** A frame hitch is
directly a refill stall. (OpenAL Soft's own mixing is *not* on this thread — the
OpenSL backend runs a dedicated mixer thread, `alc/backends/opensl.cpp:539`,
`:258-261` — so already-queued audio survives a hitch; only the refill does not.)

The same thread also does the *sample* decoding: `getBufferForFile()` decodes an
entire uncached sound file to PCM in a blocking loop
(`OpenALAudioCache.cpp:56-57`) before returning, and opening a stream does
`TheFileSystem->openFile()` + `avformat_open_input()` +
`avformat_find_stream_info()` inline (`OpenALAudioManager.cpp:786-797`,
`FFmpegFile.cpp:51-108`). So *starting a new sound* can itself cause the hitch
that starves the music stream.

### Queue geometry

- `AL_STREAM_BUFFER_COUNT` is **32** (`OpenALAudioStream.h:29`); every stream
  allocates 32 AL buffers up front (`OpenALAudioStream.cpp:8`).
- The refill low-water mark and the refill target are **both** `32 / 2 = 16`
  (`OpenALAudioStream.cpp:158`, `:163`). So the queue is topped back up to 16
  buffers each frame and 16 buffers is the entire headroom.
- One buffer = one decoded FFmpeg frame (`OpenALAudioManager.cpp:816-844`), so
  the headroom in milliseconds depends entirely on the codec's frame size —
  not determinable from source.
- `bufferData()` refuses to queue when already at 32 (`OpenALAudioStream.cpp:41-46`).

### The refill loop gives up on the first unproductive packet

```
while (num_queued < AL_STREAM_BUFFER_COUNT / 2) {
    if (!m_requireDataCallback()) { m_endOfData = true; break; }   // :168-171
    ALint refreshedQueued = 0;
    alGetSourcei(m_source, AL_BUFFERS_QUEUED, &refreshedQueued);
    if (refreshedQueued <= num_queued) { break; }                  // :175-177
    ...
}
```
(`OpenALAudioStream.cpp:158-181`)

`decodePacket()` legitimately returns `true` without producing a frame when the
decoder needs more input — `avcodec_send_packet` or `avcodec_receive_frame`
returning `AVERROR(EAGAIN)` (`FFmpegFile.cpp:225-227`, `:241-243`). One such
packet ends the whole refill for that frame.

### What happens when the queue reaches zero

`processPlayingList()` calls `update()` and then, **in the same frame**, releases
the stream if its source is still stopped:

```
playing->m_stream->update();                                       // :2539
if (playing->m_stream && sourceIsStopped(playing->m_stream->getSource()))
{
    ... releasePlayingAudio(playing); it = m_playingStreams.erase(it);
}
```
(`OpenALAudioManager.cpp:2539-2555`)

So a stream that fully drains and then fails to re-queue *even one* buffer that
frame is destroyed on the spot — the track or voice line ends permanently, mid-
sentence. Combining the two: a hitch long enough to drain 16 buffers, followed by
a single `EAGAIN` packet on the recovery frame, silently kills the stream.

There are three further ways `m_endOfData` latches and the stream is released:

1. Real EOF — `av_read_frame` returns `AVERROR_EOF`, `m_atEof` is set
   (`FFmpegFile.cpp:210-217`), the callback returns false
   (`OpenALAudioManager.cpp:806-813`), `m_endOfData = true`
   (`OpenALAudioStream.cpp:99-101`, `:168-170`). Correct.
2. **Stalled-probe latch** — if the source is stopped with everything played and
   the EOF probe's single `decodePacket()` yields no new buffer, `m_stalledProbes`
   is incremented; **three consecutive such frames latch EOF**
   (`OpenALAudioStream.cpp:102-113`). Three consecutive slow/unproductive frames
   are enough to end a stream that was not actually finished. The counter only
   resets on a successful refill (`OpenALAudioStream.cpp:114-116`, `:179`).
3. **Non-EOF read error** — `decodePacket()` checks only for `AVERROR_EOF`
   (`FFmpegFile.cpp:210-217`) and then uses `m_packet->stream_index`
   unconditionally (`FFmpegFile.cpp:219-222`). Any other negative return from
   `av_read_frame` (I/O error, invalid data) leaves the packet unset and indexes
   `m_streams` with whatever is in it; the two `DEBUG_ASSERTCRASH` guards compile
   out in this build. Behaviour past that point is undefined.

### The underrun recovery itself is audible

On finding the source stopped with buffers queued, `update()` calls `play()`
**before** unqueueing the processed buffers (`OpenALAudioStream.cpp:120-127`;
the unqueue is state-gated at `:133-139`). Restarting a stopped streaming source
whose queue is entirely processed makes OpenAL replay that queue from its start.
The buffers are unqueued a few instructions later, but the mixer thread runs
concurrently and can emit part of a period from the old data first. The in-file
comments describe exactly this artefact from real testing — "REPLAYING its
already-played buffers as a repeating 'chip'"
(`OpenALAudioStream.cpp:86-89`) and "the audible 'chirping' loop after a voice
line" (`OpenALAudioStream.cpp:103-109`). **Every underrun therefore costs a
short repeated blip plus a gap**, which is what "cuts out in places" sounds like.

### The uninterruptible-speech interlock

An `AT_Streaming` event with `getUninterruptible()` calls `stopAllSpeech()` —
killing every other streaming voice (`OpenALAudioManager.cpp:753-756`,
`1349-1371`) — and sets `setDisallowSpeech(TRUE)`
(`OpenALAudioManager.cpp:861-864`). While that flag is set, **every** subsequent
`AT_Streaming` event is rejected at `GameAudio.cpp:418-419`. It is cleared when
the stream is detected stopped (`OpenALAudioManager.cpp:2545-2551`) or by a
15-second backstop timer (`OpenALAudioManager.cpp:79-83`, `2566-2572`). The
file's own comments record that this flag has already been observed sticking and
silencing all subsequent speech for a match
(`OpenALAudioManager.cpp:70-79`). The backstop makes the failure bounded rather
than permanent — a 15-second window with no speech, which is a very good match
for "cuts out in places" if the reporter meant voice lines.

### Streams are also created and started with an empty queue

`playStream()` (`alSourcePlay`) is called immediately after the stream is
constructed, before anything is buffered (`OpenALAudioManager.cpp:866-868`,
`2943-2955`). The first real fill happens on the next `processPlayingList()`,
which then performs up to 16 `decodePacket()` calls in one frame — a
per-voice-line and per-track hitch on the game thread.

---

## D. Device and platform configuration

### What the port asks OpenAL for

```
m_alcDevice = alcOpenDevice(NULL);                                    // :1525
...
ALCint attributes[] = { ALC_FREQUENCY, audioSettings->m_outputRate, 0 };
m_alcContext = alcCreateContext(m_alcDevice, attributes);             // :1533-1534
```
(`OpenALAudioManager.cpp:1513-1567`)

That is the entire device configuration. Not requested: `ALC_MONO_SOURCES`,
`ALC_STEREO_SOURCES`, `ALC_REFRESH`, `ALC_SYNC`, HRTF attributes, output mode.
The only other global setup is `alDistanceModel(AL_INVERSE_DISTANCE_CLAMPED)`
(`OpenALAudioManager.cpp:517`). `initDelayFilter()` and `createListener()` are
empty (`OpenALAudioManager.cpp:3061-3070`), and `setSpeakerType()` only stores an
integer (`OpenALAudioManager.cpp:1882-1891`).

If `alcOpenDevice`, `alcCreateContext` or `alcMakeContextCurrent` fails, the port
calls `setOn(false, AudioAffect_All)` and returns — "fail silently" is the
comment (`OpenALAudioManager.cpp:1526-1546`). There is no user-visible signal.

### What OpenAL Soft does with that

In openal-soft 1.24.2 (fetched by `cmake/openal.cmake:28-53`):

- Default period is `DefaultUpdateSize = 960` frames with
  `DefaultNumUpdates = 3` — 20 ms × 3 = 60 ms at 48 kHz
  (`core/device.h:45-46`, `alc/alc.cpp:1102-1103`).
- A supplied `ALC_FREQUENCY` **scales the period proportionally**:
  `period_size = lround(period_size * freqAttr / oldrate)`
  (`alc/alc.cpp:1421-1428`). So requesting 22050 Hz yields a ~441-frame period —
  still 20 ms, but a period size Android will not match natively.
- `period_size` / `periods` can only be overridden from an `alsoft.conf` /
  `ALSOFT_*` config (`alc/alc.cpp:1119-1124`). **No `alsoft.conf` and no
  `ALSOFT_*` environment variable is set for Android anywhere in this repo** —
  see below.
- The OpenSL backend does not query Android's native output rate. It takes
  `mDevice->mSampleRate` as given and hands it to `CreateAudioPlayer`
  (`alc/backends/opensl.cpp:397-446`). A mismatch against the device's native
  rate is resolved by AudioFlinger's resampler, adding cost and latency.
- The OpenSL mixer thread requests real-time priority via `SetRTPriority()`
  (`alc/backends/opensl.cpp:260`), which on this build (no RTKit —
  `build/android-vulkan/_deps/openal_soft-build/config.h`, `HAVE_RTKIT 0`) means
  a bare `pthread_setschedparam(SCHED_RR)` (`core/helpers.cpp:372-389`). Whether
  Android grants it is a runtime fact, not determinable here.

`m_outputRate` is `AudioSettings.ini`'s `OutputRate` (`GameAudio.cpp:101`) — not
in this repo, so the actual requested rate is unknown. Nothing anywhere queries
`AudioManager.PROPERTY_OUTPUT_SAMPLE_RATE` or
`PROPERTY_OUTPUT_FRAMES_PER_BUFFER`.

### Which backend is actually in the APK

`android/app/src/main/jniLibs/arm64-v8a/libopenal.so` contains only three backend
names — `opensl`, `wave`, `null` — plus `libOpenSLES.so`,
`N12_GLOBAL__N_114OpenSLPlaybackE` and
`"Failed to initialize OpenSL device: {:#08x}"`. **There is no Oboe/AAudio
backend in the shipped library**, despite `ALSOFT_BACKEND_OBOE:BOOL=ON` in
`build/android-vulkan/CMakeCache.txt:33` (Oboe is not vendored, so the backend
was not built). Android therefore runs on OpenSL ES, the older and
higher-latency of the two paths.

### Environment and Android-side configuration

`FilterPipeWireOpenAL()` in `GeneralsMD/Code/Main/SDL3Main.cpp:250-276` sets
`ALSOFT_DISABLE_CPU_EXTS` and `ALSOFT_DRIVERS`, but is explicitly compiled out on
Android: the guard is `#if defined(__linux__) && !defined(__ANDROID__)`
(`SDL3Main.cpp:257`), with a comment explaining that forcing `pulse,alsa` would
mute the device. On Android the function only prints "keeping default driver
selection" (`SDL3Main.cpp:274`). **So on Android the port sets no OpenAL
environment variable and ships no `alsoft.conf` at all** (a repo-wide search for
`ALSOFT`, `alsoft.conf`, `ALC_HRTF`, `ALC_MONO_SOURCES`, `OPENSL`, `AAUDIO`
outside `build/` finds only the desktop-Linux paths above and the CMake options).

On the Java side there is **no audio configuration whatsoever**: a search for
`audio`/`openal` across `android/app/src/main/java/com/generalsx/zerohour/` hits
only a comment listing `libopenal` as a `DT_NEEDED` entry
(`GeneralsZHActivity.java:59`). There is no `AudioManager` use, **no audio-focus
request, no `AudioAttributes`, no `PROPERTY_OUTPUT_SAMPLE_RATE` /
`PROPERTY_OUTPUT_FRAMES_PER_BUFFER` query, and no low-latency
(`FEATURE_AUDIO_LOW_LATENCY`) check**. `android/app/src/main/java-sdl/org/libsdl/app/SDLAudioManager.java`
is SDL's own stock file and is unrelated to the OpenAL path.

Note that `SDL_InitSubSystem(SDL_INIT_VIDEO | SDL_INIT_AUDIO)`
(`SDL3Main.cpp:1099`) initialises SDL's audio subsystem alongside OpenAL's,
even though nothing in the game uses SDL for audio output.

### Lifecycle: pause, resume, focus

Two separate systems, neither of which touches OpenAL.

**(1) The mobile lifecycle watcher.** `mobileLifecycleWatcher()`
(`SDL3GameEngine.cpp:139-196`) records `SDL_EVENT_WILL_ENTER_BACKGROUND` /
`DID_ENTER_BACKGROUND` / `DID_ENTER_FOREGROUND` into `s_appBackgrounded` and
`WINDOW_FOCUS_LOST` / `WINDOW_FOCUS_GAINED` into `s_appInactive`
(`SDL3GameEngine.cpp:112-117`), logging each via `[GX-LIFECYCLE]`
(`SDL3GameEngine.cpp:134-137`). `SDL3GameEngine::update()` then skips the entire
engine update from the **second** consecutive paused frame onward:

```
if (pausedNow && s_wasPausedLastFrame) { SDL_Delay(50); return; }
```
(`SDL3GameEngine.cpp:1886-1890`)

**Nothing in that path calls `TheAudio->pauseAudio()`, `stopAudio()`, or
`alcDevicePauseSOFT`.** So while backgrounded or unfocused: already-queued audio
keeps being mixed by OpenAL Soft's own thread, but `TheAudio->UPDATE()` never
runs, so `processPlayingList()` never runs, so **no stream is ever refilled**.
Every music track and speech line drains its 16-buffer queue and stops. On
return to the foreground, `OpenALAudioStream::update()` runs again — and lands
in exactly the underrun path described in class C: a restart-then-unqueue blip,
and outright destruction of the stream if the recovery frame's first packet is
unproductive. Looping 3D sounds are likewise not advanced while paused (their
`notifyOfAudioCompletion()` never fires), so they go silent until the first
foreground frame. **A brief switch away and back is a strong candidate for a
reproducible "the music/voice stopped and never came back".**

**(2) The in-game pause.** `GameLogic::setGamePaused()` calls `pauseGameSound()`
and `pauseGameMusic()` (`GameLogic.cpp:4507-4524`), which call
`TheAudio->pauseAudio()` / `resumeAudio()` (`GameLogic.cpp:4545-4602`). This is
reached from the pause/quit menu and from scripted popup messages
(`QuitMenu.cpp:393`, `:412`; `InGameUI.cpp:5829`, `:5849`).

`OpenALAudioManager::pauseAudio()` uses **`alSourceStop()`, not
`alSourcePause()`**, for 2D and 3D samples (`OpenALAudioManager.cpp:616-628`);
only streams get a real `pause()` (`OpenALAudioManager.cpp:649-652`).
`resumeAudio()` then calls `alSourcePlay()` (`OpenALAudioManager.cpp:682-694`).
Two consequences:

- A resumed sample restarts **from the beginning**, not from where it was.
- `TheAudio->UPDATE()` keeps running while the game is paused
  (`GameEngine.cpp:1117` is unconditional). On the very next frame
  `processPlayingList()` sees those sources as `AL_STOPPED`, runs
  `notifyOfAudioCompletion()` and then `releasePlayingAudio()`
  (`OpenALAudioManager.cpp:2412-2427`, `:2451-2465`) — **the sounds are destroyed
  during the pause**, and `resumeAudio()` later calls `alSourcePlay()` on
  already-deleted source names. Opening and closing the pause menu (or any
  scripted popup) therefore silences every sound effect that was in flight.

`pauseAudio()` additionally deletes every queued `AR_Play` request
(`OpenALAudioManager.cpp:656-670`), so sounds requested in the frames around a
pause are lost outright.

`pauseAmbient()` is an empty function (`OpenALAudioManager.cpp:721-725`).

**(3) Audio focus.** Not found — nothing requests or responds to Android audio
focus, so a notification, a call, or another app ducking is not handled at all.

---

## Ranked hypotheses

Ordered by how well each explains "sound cuts out in places" on a CPU-bound
Android device.

### 1. Stream underrun on the game thread destroys or blips music and speech

**Claim.** Frame hitches drain the 16-buffer stream queue
(`OpenALAudioStream.cpp:158-163`); the recovery either replays already-played
buffers audibly (`OpenALAudioStream.cpp:120-127`) or, if the recovery frame's
first packet is unproductive (`OpenALAudioStream.cpp:175-177`) or three
consecutive frames are (`OpenALAudioStream.cpp:110-112`), latches EOF and lets
`processPlayingList()` destroy the stream mid-track
(`OpenALAudioManager.cpp:2539-2555`).

**Evidence for.** Refill is on the game thread, once per rendered frame
(`GameEngine.cpp:1117` → `OpenALAudioManager.cpp:544-552` → `:2539`), with no
audio thread anywhere. The same thread also does full synchronous file decodes
(`OpenALAudioCache.cpp:56-57`) and stream opens (`FFmpegFile.cpp:51-108`), so
starting a sound can starve the music. The file's own comments record the
resulting artefacts as observed on real hardware
(`OpenALAudioStream.cpp:86-89`, `:103-109`).

**What would confirm it.** A counter of frames where `update()` entered with
`num_queued == 0`, correlated in time with the existing `[GX-PERF]` frame-time
line (`GameEngine.cpp:1080-1088`); dropouts should cluster on frames above the
queue's depth in milliseconds. **What would kill it.** Dropouts that happen on
*sound effects* (which are single fully-buffered `alBufferData` sources and
cannot underrun) while music and speech stay clean, or dropouts with no
correlated frame spike.

### 2. Lifecycle: backgrounding/focus loss stops refilling and nothing restores it

**Claim.** From the second consecutive paused frame, `SDL3GameEngine::update()`
returns before `GameEngine::update()` (`SDL3GameEngine.cpp:1886-1890`), so
`TheAudio->UPDATE()` never runs, every stream drains, and on resume the streams
hit hypothesis 1's failure path. Nothing calls `pauseAudio()`/`resumeAudio()` or
any ALC suspend on this path.

**Evidence for.** No audio call of any kind exists in the lifecycle watcher
(`SDL3GameEngine.cpp:139-196`) or anywhere else keyed on those events; a repo-wide
search for `pauseAudio`/`resumeAudio` callers finds only `GameLogic` and
`ScriptActions`. No audio-focus handling exists on the Java side either.

**What would confirm it.** `[GX-LIFECYCLE]` lines
(`SDL3GameEngine.cpp:134-137`) immediately preceding each reported dropout in a
device log. **What would kill it.** Dropouts in a log with no `[GX-LIFECYCLE]`
event anywhere near them.

### 3. In-game pause destroys in-flight samples, because pause is implemented as stop

**Claim.** `pauseAudio()` calls `alSourceStop()` on samples
(`OpenALAudioManager.cpp:616-628`) while `TheAudio->UPDATE()` keeps running
(`GameEngine.cpp:1117`), so `processPlayingList()` releases them as finished
(`OpenALAudioManager.cpp:2412-2427`, `:2451-2465`) and `resumeAudio()`'s
`alSourcePlay()` (`OpenALAudioManager.cpp:682-694`) acts on deleted sources.
`pauseAudio()` also discards every pending `AR_Play` request
(`OpenALAudioManager.cpp:656-670`).

**Evidence for.** The code paths are unambiguous and this fires on every
pause-menu open and every scripted popup (`InGameUI.cpp:5829`,
`QuitMenu.cpp:393`) — which is a very literal reading of "cuts out **in
places**", i.e. at specific points in a mission.

**What would confirm it.** Reproduce: start a long looping sound, open the pause
menu, close it, and check whether the loop resumes. **What would kill it.** The
reporter says dropouts happen during continuous uninterrupted play with no pause
menu and no scripted popups.

Lower-ranked but worth ruling out: the **`disallowSpeech` interlock**
(`GameAudio.cpp:418-419`, `OpenALAudioManager.cpp:861-864`) producing up to
15-second speech blackouts (`OpenALAudioManager.cpp:79-83`, `2566-2572`) — the
in-file comments say this has already been seen; the **cache eviction of
in-use lower-priority sounds** (`OpenALAudioCache.cpp:276-292` →
`OpenALAudioManager.cpp:2803-2842`) against an unintentionally hardcoded 14 MiB
budget (`OpenALAudioCache.cpp:18`, `:212-219`); and the **class A voice/limit/
priority culling**, which is real and deliberate but has been in the game since
2003 and is not obviously more aggressive here than on the original.

---

## What to measure on-device

Cheap instrumentation, in the order that best separates the hypotheses. Nothing
here needs a debugger.

1. **Stream underrun counter.** In `OpenALAudioStream::update()`, count frames
   where `num_queued == 0` on entry, and separately count each latch of
   `m_endOfData` at `OpenALAudioStream.cpp:110-112` and `:168-170`. Emit a
   one-line-per-second summary via the existing `GX_PERF_TRACE` mechanism
   (`GameEngine.cpp:1080-1088`). Distinguishes hypothesis 1 from everything else.
2. **Queue depth in milliseconds.** Log `frame->nb_samples` and
   `frame->sample_rate` once per stream open (`OpenALAudioManager.cpp:816-844`).
   16 × frame duration is the entire underrun budget; this is the single number
   most needed and it cannot be read from source.
3. **Frame-time correlation.** The `[GX-PERF]` line already reports per-subsystem
   times including `audio=` (`GameEngine.cpp:1080-1092`). Add a per-frame max, and
   check whether underruns land on the spike frames.
4. **Device parameters after `alcCreateContext`.** Log `ALC_FREQUENCY`,
   `ALC_REFRESH`, `ALC_MONO_SOURCES`, `ALC_STEREO_SOURCES` via `alcGetIntegerv`,
   and `alcGetString(device, ALC_DEVICE_SPECIFIER)`, right after
   `OpenALAudioManager.cpp:1539`. Compare the frequency against
   `AudioManager.PROPERTY_OUTPUT_SAMPLE_RATE` read in
   `GeneralsZHActivity.java`. Settles the class D questions in one run.
5. **`alGetError()` after `alGenSources`** at `OpenALAudioManager.cpp:908` and
   `:967`, and after the `alGenSources`/`alGenBuffers` pair in
   `OpenALAudioStream.cpp:6-8`. If these ever fail today they fail invisibly.
6. **Channel-slot census.** Log `m_playingSounds.size()`, `m_playing3DSounds.size()`,
   `m_num2DSamples`, `m_num3DSamples` once a second from
   `OpenALAudioManager.cpp:2573`. If either count sits pinned at the cap, class A
   culling is the story; if `m_num2DSamples` is **0**, the provider-selection
   hard-fail in class A is the story.
7. **Cull tally.** `canPlayNow()` already has the log points, behind
   `INTENSIVE_AUDIO_DEBUG` (`GameSounds.cpp:180-269`). Replace them with cheap
   counters (distance / shroud / voice / limit / no-channel / priority) and dump
   once a second — this tells you directly whether sounds are being *refused* or
   being *stopped after starting*, which is the top-level fork in this whole
   audit.
8. **Lifecycle correlation.** `[GX-LIFECYCLE]` already logs every transition
   (`SDL3GameEngine.cpp:134-137`). Just capture a full logcat during a session
   with reported dropouts and look for those lines near each one.

---

## Cheap fixes worth trying

### Low-risk and clearly right regardless of which hypothesis wins

- **Request the device's native sample rate.** Read
  `AudioManager.PROPERTY_OUTPUT_SAMPLE_RATE` in the Java activity, pass it
  through, and use it for `ALC_FREQUENCY` at `OpenALAudioManager.cpp:1533`
  instead of `m_outputRate`. OpenSL does not query it itself
  (`alc/backends/opensl.cpp:397-446`), so today a mismatch forces AudioFlinger to
  resample every buffer.
- **Check `alGetError()` after every `alGenSources`/`alGenBuffers`** and
  initialise `ALuint source = 0` at `OpenALAudioManager.cpp:905` and `:964`.
  Today a failure produces a stack-garbage source name that silently occupies a
  channel slot forever.
- **Wire up the reentrancy guard that is already declared.** `m_reentrantProcessingAudio`
  (`OpenALAudioManager.h:236`) is documented in detail and never used; the
  `.cpp` half in `processPlayingList()` and `closeAnySamplesUsingFile()` is
  missing. This is a latent use-after-free, not a performance question.
- **Honour `setMaxSize()`** (`OpenALAudioCache.cpp:212-219`) or, at minimum,
  charge the cache the decoded PCM size rather than overwriting `m_fileSize` with
  the on-disk size at `OpenALAudioCache.cpp:163`. The current accounting is
  simply wrong in both directions.
- **Use `alSourcePause()` instead of `alSourceStop()` in `pauseAudio()`**
  (`OpenALAudioManager.cpp:616-628`) so `resumeAudio()`'s `alSourcePlay()`
  actually resumes, and so `processPlayingList()` does not reap paused samples as
  finished.
- **Do not treat "stream is stopped this frame" as "stream is finished".**
  `processPlayingList()` should require `m_stream->isAtEnd()` (the flag already
  exists, `OpenALAudioStream.h:50`) before releasing at
  `OpenALAudioManager.cpp:2542`, rather than releasing on a bare `AL_STOPPED`.
  This alone converts "underrun kills the track" into "underrun stutters".
- **Do not give up the refill on the first unproductive packet.**
  `OpenALAudioStream.cpp:175-177` breaks on a single `AVERROR(EAGAIN)`, which is
  a normal decoder state (`FFmpegFile.cpp:225-227`, `:241-243`). Allow a small
  number of unproductive `decodePacket()` calls per frame before giving up.
- **Handle non-EOF `av_read_frame` failures** at `FFmpegFile.cpp:210-222` instead
  of falling through into `m_packet->stream_index` with an unset packet.
- **Refill audio across a lifecycle pause.** Either call `TheAudio->UPDATE()`
  (or just the stream-refill part) in the paused branch of
  `SDL3GameEngine::update()` (`SDL3GameEngine.cpp:1886-1890`), or explicitly
  `pauseAudio(AudioAffect_All)` on `WILL_ENTER_BACKGROUND` / `FOCUS_LOST` and
  `resumeAudio()` on `DID_ENTER_FOREGROUND` / `FOCUS_GAINED`
  (`SDL3GameEngine.cpp:142-165`). Doing neither — the current state — is the one
  option that guarantees a broken stream on resume.
- **Log the provider-selection outcome.** If `initSamplePools()` did not run,
  `m_num2DSamples` is 0 and the game is silent except for music, with no
  diagnostic at all (`OpenALAudioManager.cpp:1759-1766`, `3078-3090`).

### Speculative — needs the measurements first

- **Enlarge the stream queue headroom.** Raising `AL_STREAM_BUFFER_COUNT`
  (`OpenALAudioStream.h:29`) and/or the refill target
  (`OpenALAudioStream.cpp:158`, `:163`) buys proportionally more hitch tolerance,
  at the cost of latency and memory. Pointless until measurement 2 above says how
  many milliseconds a buffer is worth.
- **Move stream refill off the game thread.** The correct fix for hypothesis 1,
  and the largest change: the decode + `alBufferData` + `alSourceQueueBuffers`
  path (`OpenALAudioManager.cpp:806-844`, `OpenALAudioStream.cpp:38-73`) would
  need its own thread with its own OpenAL context handling and locking around
  `m_playingStreams`. Worth doing only if the underrun counter confirms the
  diagnosis.
- **Build openal-soft with the Oboe backend** so Android gets AAudio instead of
  OpenSL ES. `ALSOFT_BACKEND_OBOE` is already ON in the cache
  (`build/android-vulkan/CMakeCache.txt:33`) but Oboe is not vendored, so the
  shipped `libopenal.so` has only `opensl`/`wave`/`null`. This is a latency and
  robustness improvement, not a fix for anything identified above.
- **Raise the concurrent-sample counts.** `SampleCount2D`/`SampleCount3D` were
  tuned for 2003 hardware and are enforced only as a bookkeeping subtraction
  (`OpenALAudioManager.cpp:1920-1931`), not a real resource limit — OpenAL Soft
  allows 256 sources (`alc/alc.cpp:3004`). Raising them is low-risk *if*
  measurement 6 shows the lists pinned at the cap, and pointless otherwise.
- **Pre-warm the sample cache.** The synchronous full-file decode at
  `OpenALAudioCache.cpp:56-57` on first play of each sound is a guaranteed hitch;
  decoding common sounds during load would remove one of the hitch sources
  feeding hypothesis 1.
