/*
**	GeneralsX @build Android port 30/07/2026
**
**	Opt-in diagnostic tracing.
**
**	The [GX-TRACE] instrumentation in the font/text/GUI paths is invaluable
**	when chasing a hang, but several of its sites sit inside per-frame code
**	(Render2DSentenceClass::Render alone emits 4-12 lines every frame, each
**	followed by an fflush). Left permanently on, it produced tester logs
**	that were 100% trace spam: the engine log grows unbounded on disk, and
**	the in-app log export keeps only a bounded slice, so session startup --
**	device/GPU/driver banner, INI loading, DXVK init, i.e. the part that
**	actually explains a failure -- was always scrolled away by the time a
**	report was filed. A real case: issue #2's 2026-07-26 log from a Galaxy
**	S24+ contained 2,681 lines, every one of them trace output, and not a
**	single line about what went wrong.
**
**	So tracing is now off unless explicitly requested. Two ways to turn it
**	on, checked once on first use:
**
**	  - GX_TRACE environment variable, set to anything but empty or "0"
**	    (for developers: adb shell setprop / a wrapper script).
**	  - a file named "gx_trace.txt" in the game data folder (the process
**	    working directory by the time anything traces). This is the one to
**	    tell a tester about: no adb, no rebuild, they already manage that
**	    folder. Delete the file to turn tracing back off.
**
**	When disabled the macro costs one already-resolved bool load, so it is
**	safe to leave in per-frame paths.
**
**	GeneralsX @feature Android port 31/07/2026 GX_PERF is a second, separate
**	opt-in for just the low-volume [GX-PERF] per-subsystem frame-timing
**	summary (one line per second, see GameEngine::update()) -- deliberately
**	independent of GX_TRACE/gx_trace.txt. A real multi-minute session with
**	gx_trace.txt on produced a >70 MB stderr log; the in-app log viewer's
**	head+tail export (LogViewerActivity, 200 KB cap) then elided the entire
**	middle of the session -- exactly where sustained gameplay, and every
**	[GX-PERF] sample from it, lived. A tester who only needs the frame-time
**	breakdown, not full font/UI tracing, can now get it without drowning the
**	exported log: a "gx_perf.txt" marker file or GX_PERF env var enables
**	[GX-PERF] on its own, at native cost even over a long session. Enabling
**	GX_TRACE also implies this (a full trace session naturally wants the
**	perf summary too).
*/

#pragma once

#include <cstdio>
#include <cstdlib>

namespace GXTrace
{

	inline bool computeEnabled()
	{
		const char *env = getenv("GX_TRACE");
		if (env != nullptr && env[0] != '\0' && env[0] != '0') {
			return true;
		}

		// Relative on purpose: the engine chdir()s into the selected game
		// data folder during startup, long before any traced code runs.
		FILE *marker = fopen("gx_trace.txt", "r");
		if (marker != nullptr) {
			fclose(marker);
			return true;
		}

		return false;
	}

	inline bool isEnabled()
	{
		// Function-local static: resolved once, thread-safe since C++11, and
		// shared across every translation unit that includes this header.
		static const bool enabled = computeEnabled();
		return enabled;
	}

	inline bool computePerfEnabled()
	{
		const char *env = getenv("GX_PERF");
		if (env != nullptr && env[0] != '\0' && env[0] != '0') {
			return true;
		}

		FILE *marker = fopen("gx_perf.txt", "r");
		if (marker != nullptr) {
			fclose(marker);
			return true;
		}

		return false;
	}

	inline bool isPerfEnabled()
	{
		// GX_TRACE implies GX_PERF: a full trace session wants the summary too.
		static const bool enabled = isEnabled() || computePerfEnabled();
		return enabled;
	}

	// GeneralsX @feature Android port 09/09/2026 A third, separate opt-in, for the
	// [GX-AUDIO] instrumentation. Chasing the silent intro movies and the fragmenting
	// loading music took several device sessions, and every one of them needed a log
	// with the audio path talking -- queue depth, source state, what the movie decoder
	// handed over. That instrumentation is worth keeping: audio faults are invisible in
	// a normal log and impossible to reason about without it. But it is per-audio-frame
	// output, so leaving it permanently on buries the rest of the log.
	//
	// Deliberately NOT implied by GX_TRACE: a font/GUI trace session already produces
	// enormous logs, and someone chasing a hang does not want the audio path in there
	// too. The launcher's Diagnostics section creates the marker file, so a tester who
	// is asked for an audio log just flips a switch.
	inline bool computeAudioEnabled()
	{
		const char *env = getenv("GX_AUDIO_TRACE");
		if (env != nullptr && env[0] != '\0' && env[0] != '0') {
			return true;
		}

		FILE *marker = fopen("gx_audio_trace.txt", "r");
		if (marker != nullptr) {
			fclose(marker);
			return true;
		}

		return false;
	}

	inline bool isAudioEnabled()
	{
		static const bool enabled = computeAudioEnabled();
		return enabled;
	}

}  // namespace GXTrace

// Usage: GX_TRACE("Some_Function: about to do the thing x=%d\n", x);
// The "[GX-TRACE] " prefix and the flush are supplied here.
#define GX_TRACE(...)                                     \
	do {                                                  \
		if (GXTrace::isEnabled()) {                        \
			fprintf(stderr, "[GX-TRACE] " __VA_ARGS__);    \
			fflush(stderr);                                \
		}                                                 \
	} while (0)

// Usage: GX_PERF_TRACE("[GX-PERF] frames=%d ...\n", n); -- caller supplies
// its own tag/prefix (unlike GX_TRACE), since callers of this one already
// have a specific line format. Gated on isPerfEnabled(), not isEnabled(),
// so it works with just gx_perf.txt/GX_PERF -- see the header comment above.
#define GX_PERF_TRACE(...)                                \
	do {                                                  \
		if (GXTrace::isPerfEnabled()) {                     \
			fprintf(stderr, __VA_ARGS__);                  \
			fflush(stderr);                                \
		}                                                 \
	} while (0)

// Usage: GX_AUDIO_TRACE("stream stopped: src=%u\n", src);
// The "[GX-AUDIO] " prefix and the flush are supplied here, as for GX_TRACE.
// Gated on isAudioEnabled() alone -- see the note on computeAudioEnabled().
#define GX_AUDIO_TRACE(...)                               \
	do {                                                  \
		if (GXTrace::isAudioEnabled()) {                     \
			fprintf(stderr, "[GX-AUDIO] " __VA_ARGS__);        \
			fflush(stderr);                                    \
		}                                                 \
	} while (0)
