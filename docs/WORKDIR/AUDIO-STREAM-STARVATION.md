# Audio stream starvation: streamed music/briefings play in fragments

Scope: why the map-loading music and campaign briefing audio play a moment of
sound, go silent, and repeat that for the whole load, even though the stream
is not being destroyed and the pump-cadence fix (`TheAudio->UPDATE()` inside
`LoadScreen::update()`) is already in place. This is about why the *refill
itself* produces nothing, dozens of times in a row, within calls that already
retry up to `MAX_BARREN_CALLS` times.

All line numbers are against the files actually compiled into the game.
`SAGE_USE_OPENAL` makes `Core/GameEngineDevice/Source/OpenALAudioDevice/*`
and `Core/GameEngineDevice/Source/VideoDevice/FFmpeg/FFmpegFile.cpp` the
active implementation for both `Generals` and `GeneralsMD`
(`Core/GameEngineDevice/CMakeLists.txt:248-288`); the `GeneralsMD`-local
copies of `OpenALAudioManager.cpp` / `FFmpegFile.cpp` are excluded from the
build in that configuration (`GeneralsMD/Code/GameEngineDevice/CMakeLists.txt:213-219`,
`:271-290`). Everything below is the `Core/` copy.

## 1. The callback path, end to end

1. `OpenALAudioManager::playAudioEvent()` opens the file, opens an
   `FFmpegFile`, and wires two callbacks
   (`Core/GameEngineDevice/Source/OpenALAudioDevice/OpenALAudioManager.cpp:825-881`):
   - `stream->setRequireDataCallback(...)` — `OpenALAudioManager.cpp:837-844`:
     ```cpp
     stream->setRequireDataCallback([ffmpegFile, stream]() -> bool {
         ffmpegFile->decodePacket();
         return !ffmpegFile->isAtEof();
     });
     ```
   - `ffmpegFile->setFrameCallback(...)` — `OpenALAudioManager.cpp:847-881`:
     called synchronously from inside `decodePacket()` whenever a full audio
     frame comes out of the decoder; it interleaves planar samples if needed
     and calls `stream->bufferData(...)`.
2. `OpenALAudioStream::update()` is what invokes the data callback, in two
   places: a single-shot EOF probe
   (`Core/GameEngineDevice/Source/OpenALAudioDevice/OpenALAudioStream.cpp:99-154`)
   and the bulk refill loop
   (`OpenALAudioStream.cpp:238-282`), which calls it up to
   `MAX_BARREN_CALLS` (16, `OpenALAudioStream.cpp:243`) times per `update()`
   while `AL_BUFFERS_QUEUED < AL_STREAM_BUFFER_COUNT/2` (16 of 32,
   `Core/GameEngineDevice/Include/OpenALAudioDevice/OpenALAudioStream.h:29`).
3. `update()` itself only runs when something calls
   `playing->m_stream->update()` inside
   `OpenALAudioManager::processPlayingList()`
   (`OpenALAudioManager.cpp:2630`), which runs once per
   `OpenALAudioManager::update()` (`OpenALAudioManager.cpp:558-566`), which
   runs once per `TheAudio->UPDATE()`. During loading that is pumped
   explicitly from `LoadScreen::update()`
   (`Core/GameEngine/Source/GameClient/GUI/LoadScreen.cpp:159-192`) — this is
   the fix already in place for "audio only advances once per game frame,
   and loading never reaches that frame." **Decoding is entirely
   synchronous on the thread that calls `TheAudio->UPDATE()`** — the game/UI
   thread, both during normal play (`GeneralsMD/Code/GameEngine/Source/Common/GameEngine.cpp:1117`)
   and during loading (`LoadScreen.cpp:186`, `:1140`, `:1194`, `:1367`). No
   other thread advances the decoder; OpenAL Soft's mixer thread only
   consumes buffers already queued, it never calls back into game code. So
   "does the decoder need something else to run" reduces entirely to "does
   `OpenALAudioStream::update()` get called often enough, and does each call
   make forward progress" — the first half is already fixed, this document
   is about the second half.
4. `decodePacket()`
   (`Core/GameEngineDevice/Source/VideoDevice/FFmpeg/FFmpegFile.cpp:205-260`)
   does the actual work: reads one packet (`av_read_frame`), sends it to the
   relevant stream's codec context, and drains every frame currently
   available (`avcodec_receive_frame` in a loop) before returning.
5. What the callback's return value means vs. what it does: the comment at
   `OpenALAudioStream.h:37-41` documents the *intended* contract — "returns
   TRUE on a normal refill or transient error, FALSE only at true EOF." The
   *actual* implementation at `OpenALAudioManager.cpp:837-844` computes this
   solely from `ffmpegFile->isAtEof()` and **discards `decodePacket()`'s own
   boolean return value entirely** (`ffmpegFile->decodePacket();` — the
   result isn't even assigned to a variable). `isAtEof()` is `true` only when
   `av_read_frame` returned `AVERROR_EOF` (`FFmpegFile.cpp:210-217`,
   `FFmpegFile.h:60-62`). Every other way `decodePacket()` can fail internally
   — `avcodec_send_packet` returning a hard error (`FFmpegFile.cpp:229-235`),
   `avcodec_receive_frame` returning a hard error (`:246-252`), or a packet
   from a non-audio stream that just doesn't produce an audio frame — makes
   `decodePacket()` return `false` or run to completion without ever calling
   the frame callback, and in every one of those cases **the outer lambda
   still reports `true` ("more data is coming")** because `isAtEof()` was
   never set. This is confirmed directly from the source, not inferred: the
   callback can and does return `TRUE` without ever queueing a buffer, any
   time `decodePacket()` does anything other than hit real end-of-file or
   successfully decode a frame.

## 2. What the log's numbers prove

```
[GX-AUDIO] unqueue src=1 asked=1 failed=0 queuedAfter=0
[GX-AUDIO] refilled src=1 queued=0 processed=0 state=STOPPED
```

- `unqueue ... asked=1 failed=0 queuedAfter=0`: exactly one buffer was
  attached and fully processed (`OpenALAudioStream.cpp:184-219`), it was
  unqueued with no `AL` error, and the queue is verifiably empty right
  afterward. Nothing here indicates any deeper problem — this is the
  intended, working half of the update.
- `refilled ... queued=0 processed=0 state=STOPPED`: this line is printed
  after the entire refill loop
  (`OpenALAudioStream.cpp:238-282`, which is `gxStarved`-gated at
  `:292-303`) has run — meaning up to 16 calls to the data callback
  happened between the two log lines — and the queue is *still* zero. Per
  §1.5, each of those 16 calls can perfectly legally have returned `true`
  (not the documented "true only means real EOF") while adding nothing:
  the refill loop's own comment block at `OpenALAudioStream.cpp:258-277`
  already documents that a `true`-but-no-growth call is treated as "keep
  trying," which is exactly why it takes all 16 attempts to give up rather
  than 1.
- Therefore the log proves: (a) the unqueue side is completely healthy —
  this is not a buffer-leak or stuck-processed-count problem; (b) sixteen
  consecutive calls into `FFmpegFile::decodePacket()` (via the callback) in
  the same `update()` produced zero frames while still reporting "more data
  coming"; (c) since the trace pair recurs with the same `src=` across many
  updates rather than appearing once and then falling silent (the
  `gxStarved` gate at `:177` requires `num_queued > 0` at *entry* to log at
  all — see the walk-through in §3.1), the stream is not permanently dead
  either: it is intermittently managing to get exactly one buffer queued
  between failures, then failing again. That rules a *permanently wedged,
  never-recovers* decoder in or out only partially — see §3.1.
- What it does **not** prove by itself: which of `decodePacket()`'s several
  internal failure paths is firing, or whether the packets being read are
  even audio packets. The log as given does not include which branch inside
  `decodePacket()` returned, because none of `decodePacket()`'s own error
  paths log anything that survives to a release build (see §3, Rank 3).

## 3. Ranked causes

### Rank 1 — the callback throws away `decodePacket()`'s own failure signal (confirmed in source, matches the log unconditionally)

**File/line:** `Core/GameEngineDevice/Source/OpenALAudioDevice/OpenALAudioManager.cpp:837-844`.

For: this is not speculative — the code plainly ignores
`ffmpegFile->decodePacket()`'s return value and answers `!isAtEof()`
instead. Whatever the deeper reason `decodePacket()` fails to produce a
frame (Rank 2's two candidates below, or something else entirely), this is
the reason `update()`'s refill loop cannot tell "transient, keep trying" (a
real "need one more packet" `EAGAIN`) apart from "this call flatly failed"
(a hard decode error). It converts every non-EOF failure into an
indistinguishable "moreData=1, nothing queued" result, which is exactly the
log's shape and exactly why the existing `MAX_BARREN_CALLS` retry — which
was written on the assumption that a `true`-with-no-growth result only ever
means "decoder needs one more packet" — cannot out-retry a call that is
actually failing outright.

Against: none found — this is a straightforward reading of the code; the
only question is whether it's the *proximate* cause or whether it's masking
something else, which is what Rank 2 is about.

### Rank 2a — `avcodec_send_packet(EAGAIN)` is never drained (confirmed in source; explains a hard, self-inflicted stall once triggered)

**File/line:** `Core/GameEngineDevice/Source/VideoDevice/FFmpeg/FFmpegFile.cpp:224-227`:
```cpp
result = avcodec_send_packet(codec_ctx, m_packet);
// Check if we need more data
if (result == AVERROR(EAGAIN))
    return true;
```
Per FFmpeg's own decode contract, `avcodec_send_packet` returns
`AVERROR(EAGAIN)` to mean "the decoder's output is full — call
`avcodec_receive_frame` (possibly repeatedly) until it too returns `EAGAIN`,
*then* resend." This branch does not call `avcodec_receive_frame` at all —
it returns immediately, and the packet that was just read is never resent
either (the next call reads a brand-new packet via `av_read_frame` at
`:210`, so the old one is simply dropped). Nothing else in this file, or
anywhere else in the audio path, ever calls `avcodec_receive_frame()` except
the drain loop that this early return skips
(`FFmpegFile.cpp:239-257`). Once a codec context reaches this state there is
no mechanism left in this codebase to clear it: every subsequent
`decodePacket()` call reads and discards one more packet from the file and
gets `EAGAIN` on send again, forever, for that stream instance — matching
"a moment of sound, then silence, repeated for the whole load" if this
triggers early and the stream is never destroyed and re-opened.

For: matches the "16 calls, all report true, zero growth" shape exactly and
requires no assumption about file contents or codec specifics — it's a
plain API-contract violation, verifiable from source alone.

Against: it predicts a stream that, once wedged, *never* produces another
buffer again — but the log shows the `unqueue`/`refilled` pair recurring
with the same `src=` many times, and the `gxStarved` gate that produces
those two lines requires `num_queued > 0` *at entry to `update()`*
(`OpenALAudioStream.cpp:177`, `:212`). If the decoder were permanently
wedged after the first hit, `num_queued` would settle at 0 and stay there
(nothing left to unqueue, nothing added), and the `gxStarved`-gated trace
pair would stop appearing on later calls, printing only once — not "dozens
of times in a row" with matching numbers each time. So a *permanent* wedge
from this branch alone does not by itself explain the repetition; something
must be intermittently getting exactly one buffer queued again between
occurrences. It is still a real bug and still worth fixing (see §4), but on
its own it under-explains the log's recurrence, unless it is combined with
Rank 2b (something occasionally interrupts/refreshes the decoder state, or
each occurrence corresponds to a fresh burst that happens to succeed once
before wedging again).

### Rank 2b — the streamed file's read/seek is not exclusive to this decoder (plausible, not confirmed — flagged per the task's own candidate list)

**File/line:** `Core/GameEngine/Source/Common/System/StreamingArchiveFile.cpp:219-237`
(`read()`) and `:169-196` (`openFromArchive()`), together with
`Core/GameEngineDevice/Source/StdDevice/Common/StdBIGFile.cpp:77`.

`StreamingArchiveFile::read()` does an **absolute seek before every single
read** (`m_file->seek(m_startingPos + m_curPos, File::START)` at `:227`,
then `m_file->read(...)` at `:232`) against `m_file`, which
`openFromArchive()` sets to the `archiveFile` handle it was given
(`:180`) — and `StdBIGFile.cpp:77` passes its **one shared archive file
handle** (`m_file`, the whole `.big` opened once) into every
`StreamingArchiveFile` it hands out for any logical file inside that
archive. If music/briefing audio is packed in the same `.big` as other
assets the loader is pulling in at the same time — which is exactly the
condition that is unique to *loading*, and would explain why the symptom is
scoped to the loading screen and briefings and not general gameplay audio —
then two consumers sharing one seek-then-read handle without any locking
visible in this file can interleave a seek from one consumer between
another's seek and read, handing back bytes from the wrong offset. Fed into
`av_read_frame`/`avcodec_send_packet`, that reads as a genuine decode
failure (Rank 1's masked case) or a garbled packet that never yields a
frame, and because it is a *race*, it is exactly the kind of failure that
would recover on some calls and not others — which is what the recurring,
not-permanently-dead log pattern in §3.1 needs.

For: this is the only candidate found in source that naturally produces
*intermittent* rather than *permanent* failure, matching the log's
recurrence better than 2a alone; the shared, unguarded handle is real and
verifiable at the lines cited.

Against: **not found** — no evidence in the files reviewed of a second
thread actually reading through this same archive handle concurrently with
`TheAudio->UPDATE()`. No background/loader thread was found in this port's
loading path (`GameEngine::update()`/`LoadScreen::update()` appear
single-threaded); the file loading during a level load, as far as this
review went, also runs on the same game thread. If loading is in fact
single-threaded end to end, this candidate reduces to a non-issue for
*this* bug (a single thread doing seek-then-read on its own handle in
sequence is safe by construction) and Rank 2a plus Rank 1 remain the
explanation. This needs a device-side check to settle either way (see §5).

### Rank 3 — `bufferData()` silently failing on the OpenAL calls (cannot be ruled out from source; release-build logging gap)

**File/line:** `OpenALAudioStream.cpp:47-82` (the `alGetError()` checks
around `alBufferData`/`alSourceQueueBuffers`), `Core/GameEngine/Include/Common/Debug.h:159-170`
(`DEBUG_LOG` compiles to nothing unless `DEBUG_LOGGING` is defined).

For: if every frame decodePacket() *does* successfully produce were failing
to queue (e.g. a persistent `AL_INVALID_VALUE`/`AL_INVALID_OPERATION` on
this source), the observable effect — frames decoded, nothing queued —
would look identical from the `[GX-AUDIO]` traces, which only report queue
counts, not `bufferData()`'s own outcome. And critically, `bufferData()`'s
own failure messages go through `DEBUG_LOG`
(`OpenALAudioStream.cpp:63-64`, `:71-72`), which is compiled to `((void)0)`
outside a logging-enabled build (`Debug.h:167-170`) — so if the device log
that was captured came from a build without `DEBUG_LOGGING`, this failure
mode would produce **no evidence at all**, for or against, in the log
given.

Against: `getALFormat()` falls back to a *valid* enum
(`AL_FORMAT_MONO8`, `OpenALAudioManager.cpp:499-516`) on an unrecognized
channel/bit-depth combination rather than an invalid one, so a format
mismatch specifically would not itself produce an `AL` error — it would
queue and play, just wrong (pitch/speed), not silently drop the buffer. No
other structural reason was found in source for `alSourceQueueBuffers` to
fail on every call for one source while every other sound on the same
device works. Kept as a live possibility only because the logging gap makes
it formally undecidable from source.

### Rank 4 — a genuine (but misclassified) short read (checked, does not match this log)

**File/line:** `FFmpegFile.cpp:161-171`:
```cpp
int FFmpegFile::readPacket(void *opaque, uint8_t *buf, int buf_size)
{
    File *file = static_cast<File *>(opaque);
    const int read = file->read(buf, buf_size);
    if (read <= 0)
        return AVERROR_EOF;
    return read;
}
```
Any call to the underlying `File::read()` that returns `0` — even a
legitimate transient "nothing available yet, try again" on Android's
storage rather than a true end-of-stream — is reported to `libavformat` as
a hard, permanent `AVERROR_EOF`. That would make `av_read_frame` return
`AVERROR_EOF` (`FFmpegFile.cpp:210-217`), set `m_atEof = true`, and
`decodePacket()` returns `false`. Unlike Rank 1's masked failures, this one
*is* correctly surfaced: the callback lambda computes `!isAtEof()` which is
now `false`, and `OpenALAudioStream::update()`'s refill loop latches
`m_endOfData = true` and *breaks immediately*
(`OpenALAudioStream.cpp:250-253`) rather than retrying up to 16 times.

Against, for *this* log: the given trace shows `refilled ... queued=0`
after a full retry cycle with no accompanying "EOF latched" line
(`OpenALAudioStream.cpp:250-253` has no log statement of its own, but the
probe path's analogous EOF does log — `:112-116` — and none of that
appears in the evidence quoted). More decisively, the pattern repeats
"dozens of times in a row" for the same stream across many `update()`
calls, which a latched `m_endOfData` would prevent (every later call would
skip the refill loop entirely, `:238`). So a real end-of-file
misclassification is not what produced the quoted log, though it remains a
latent correctness bug worth fixing on its own merits (a transient
zero-byte read on real Android storage should not be treated as permanent
EOF) and would matter more once Rank 1/2 are fixed and this becomes the
last inaccurate signal left in the pipe.

## 4. Recommended fix and its risks

Fix, in priority order:

1. **Stop the callback from lying about `decodePacket()`'s outcome.**
   Change `FFmpegFile::decodePacket()` to distinguish three outcomes
   instead of two: "produced a frame / needs more input" (keep going),
   "hard decode error" (should NOT be silently reported as `true`), and
   "true EOF" (`isAtEof()`, unchanged). The simplest change that preserves
   every existing caller's contract is to have the lambda in
   `OpenALAudioManager.cpp:837-844` use `decodePacket()`'s own return value
   together with `isAtEof()`, rather than only `isAtEof()` — e.g. only
   report "more data" as true when `decodePacket()` itself returned `true`
   (already includes the `EAGAIN`-needs-more-input case, which returns
   `true`) and treat an outright `decodePacket()==false && !isAtEof()` as a
   distinct "hard failure" the caller can log and count separately from a
   normal barren retry, instead of it silently consuming one of the 16
   `MAX_BARREN_CALLS` slots indistinguishably from "decoder just needs one
   more packet."
2. **Fix the `EAGAIN`-on-send drain.** In `FFmpegFile::decodePacket()`
   (`:224-227`), do not return immediately on `EAGAIN` from
   `avcodec_send_packet`. Drain with `avcodec_receive_frame` in a loop
   first (same loop already present at `:239-257`), *then* retry sending
   the same still-owned packet (do not `av_read_frame` a new one until the
   current packet has actually been accepted). This matches the documented
   FFmpeg contract and removes the only self-inflicted, unrecoverable stall
   found in source.
3. Once both of the above are in, re-instrument with real error visibility
   in a release-buildable form (fprintf, like the existing `[GX-AUDIO]`
   traces, not `DEBUG_LOG`) so that if Rank 2b or Rank 3 are the actual
   proximate cause, the next capture will show it directly instead of
   requiring more source archaeology (see §5).
4. Only after those are proven insufficient by a fresh trace, investigate
   the shared-archive-handle question (Rank 2b): either confirm loading is
   in fact single-threaded (in which case this is a non-issue) or add
   per-`StreamingArchiveFile` synchronization / a private seek position
   check around the shared handle.

Risks — this file has been patched repeatedly for speech/briefing bugs, and
a change here needs to not re-break any of them:

- **The `EAGAIN`-on-receive "needs more input" path must still return
  `true` and still count toward `MAX_BARREN_CALLS`.** Do not conflate it
  with the new "hard failure" case, or the fbraz3/BenderAI narrator-restart
  fix (`OpenALAudioStream.cpp:284-291`, comment attributes it to a
  generic-speech stream that "began the frame with an empty queue") and the
  14/06/2026 EOF-latch logic (`OpenALAudioStream.cpp:99-154`, `:221-233`,
  `FFmpegFile.h:60-62`) both depend on `true` meaning "still might have more,
  don't give up" for a codec that legitimately needs several packets.
- **A "hard failure" must not be treated as EOF.** The whole point of
  `isAtEof()` (14/06/2026 fix) was to distinguish "the taunt/briefing line
  is genuinely finished, let it reach a stable `AL_STOPPED` so
  `disallowSpeech` clears" from "transient error, recover and keep going"
  (`OpenALAudioStream.h:37-41`, `OpenALAudioManager.cpp:839-843`). If a
  hard-failure signal is wired into the same latch as real EOF, a corrupted
  read mid-briefing would silently truncate the line and clear
  `disallowSpeech` early — reintroducing exactly the bug the 14/06/2026 fix
  and the 04/07/2026 "3 stalled probes" heuristic
  (`OpenALAudioStream.cpp:118-148`) were written against. It must be its
  own third state, not folded into either existing boolean.
- **The 04/07/2026 stalled-probe counter and its Android 08/09/2026 timing
  gate** (`OpenALAudioStream.cpp:136-153`) assume "no growth" is rare and
  meaningful. If the `EAGAIN`-drain fix (item 2) changes how often
  `decodePacket()` legitimately returns "no growth yet," re-check that a
  healthy, merely-slow stream still doesn't accumulate 3 stalled *probes*
  close together and get EOF-latched by mistake — that call site
  (`:99-154`) is separate from the bulk refill loop and was not touched by
  this analysis's proposed fix, but consumes the same `decodePacket()`.
- **The unqueue-in-every-state fix and the "only restart on an unprocessed
  buffer" fix** (both `Android port 08/09/2026`, `OpenALAudioStream.cpp:156-233`)
  are unrelated to the refill's data supply and should be undisturbed by
  any of the above — they are about buffer bookkeeping, not about whether
  `decodePacket()` produces frames. Nothing in the recommended fix touches
  them, but a fix implemented carelessly inside the same function could
  easily start "gxStarved"-gating differently and break their tracing.
- If Rank 2b (shared archive handle) turns out to be real, synchronizing it
  has its own risk: any lock taken on the game thread while it already owns
  other resources needs care not to introduce a stall of its own during
  loading — the exact class of problem this whole investigation started
  from.

## 5. What would settle this from a device trace, since source does not

The source review narrows this to "the callback lies about failure" (Rank 1,
certain) sitting on top of one or both of two proximate causes (Rank 2a,
certain-if-triggered; Rank 2b, plausible-but-unconfirmed), with Rank 3 left
formally undecidable by a logging gap. To decide between the remaining
candidates without more source spelunking, add, temporarily, release-safe
(`fprintf`, not `DEBUG_LOG`) traces at:

- Inside `FFmpegFile::decodePacket()` itself: log the actual branch taken —
  distinguish "`av_read_frame` EOF", "`send_packet` EAGAIN (and whether
  receive was attempted)", "`send_packet` hard error + the FFmpeg error
  string", "`receive_frame` EAGAIN", "`receive_frame` hard error + string",
  and "frame produced, stream_type=X" (so a non-audio `stream_type` — e.g.
  an attached-picture/video stream — showing up repeatedly would confirm or
  refute packets being consumed by a stream nothing is reading). This alone
  would immediately confirm or rule out Rank 2a (does `EAGAIN` on *send*
  ever actually occur for these files' codec) versus a hard error (which
  would point at Rank 2b or corrupt source data) versus normal `EAGAIN` on
  *receive* (which would mean the existing retry budget is simply too small
  for this codec/container, a different and much simpler fix).
- Inside `OpenALAudioManager.cpp:837-844`'s callback lambda: log
  `decodePacket()`'s own return value alongside `isAtEof()`, to directly
  confirm Rank 1 is firing in practice (a `false`/`false` pair proves it).
- Inside `StreamingArchiveFile::read()` (`StreamingArchiveFile.cpp:219-237`):
  log the calling thread id and whether the seek's return value matched the
  requested offset, to confirm or rule out Rank 2b without needing to trace
  every loading call site by hand.

With those three additions, one capture during a loading-screen music
dropout would be enough to pick definitively between "fix the EAGAIN drain
and the masked callback and stop there" and "there is also a real handle
race to fix."

## Answering the task's direct questions

- **Is the decode on the game thread, and does the decoder need anything
  else to run?** Yes, entirely on the thread that calls `TheAudio->UPDATE()`
  (the game/UI thread in both normal play and loading — §1.3), and no other
  thread advances it. The pump-cadence half of that problem (loading not
  calling `TheAudio->UPDATE()` at all) is already fixed
  (`LoadScreen.cpp:165-192`). This document is about a *different* problem:
  once `update()` is called, its own bounded retry loop
  (`OpenALAudioStream.cpp:238-282`) can still exhaust all 16 attempts and
  add nothing, because the callback it calls can report success without
  having produced anything (§1.5, §3 Rank 1) — so "retry harder" cannot fix
  this; the callback's contract with its caller has to be corrected first.
- **Is this fixable without moving the decode off the game thread?** Yes.
  Nothing in the ranked causes requires threading to fix — Rank 1 and Rank
  2a are both single-threaded, in-process logic bugs with fixes that stay
  entirely inside `FFmpegFile::decodePacket()` and the lambda that wraps it.
  Only Rank 2b, if it turns out to be real, would call for a synchronization
  fix (a lock or a private read path), still not a threading *move* of the
  decode itself.
