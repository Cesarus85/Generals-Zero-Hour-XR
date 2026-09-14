/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

// GeneralsX @build Android port 07/07/2026
//
// A crash during dlopen()'s ELF constructors or early in SDL_main runs
// before SDL3Main.cpp's own stderr-redirect logic ever executes — on
// stock Android that class of crash is invisible without adb (only a
// system tombstone under /data/tombstones, unreadable without root, and
// a logcat FATAL SIGNAL line that scrolls away in seconds). Users
// testing this port sideloaded, without a PC handy, had no way to tell
// us what broke.
//
// Fix: install a signal handler as an ELF constructor — it runs the
// instant the dynamic linker loads this library, before any engine code,
// before JNI_OnLoad, before main(). On a fatal signal it appends a
// one-line summary to a fixed path using only raw POSIX syscalls (no
// libc buffered I/O, no malloc) so it stays usable even with a corrupted
// heap, then chains to whatever handler was previously installed so the
// OS's own tombstone/logcat capture still happens too.
//
// The log path is the app's internal storage (getFilesDir() on the Java
// side) — always private + writable to this app's UID on every Android
// version, no permission needed, unlike external/shared storage. Android's
// SDL3/LogViewerActivity reads the same file to show it in-app without adb.

#if defined(__ANDROID__)

#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <stdint.h>
#if defined(__aarch64__)
#include <ucontext.h>
#endif

namespace {

// GeneralsX @bugfix Android port 07/07/2026 A bare signal+fault_addr line
// wasn't enough to diagnose a crash reported as happening "on pressing a
// button in-game": two separate sessions hit the exact same fault_addr
// (0x2000000001, a fixed non-null garbage-looking value — not a plain
// null-deref), which pointed at a deterministic bug but gave no way to
// find WHERE. Resolve the crashing PC to "library+file-offset" by walking
// /proc/self/maps with raw syscalls only (open/read/close are POSIX
// async-signal-safe; no malloc, no strtok/libc line-buffering). The
// resulting offset is directly usable with `addr2line -e libmain.so
// <offset>` against the same CI build's unstripped .so.
int hexDigitValue(char c) {
	if (c >= '0' && c <= '9') return c - '0';
	if (c >= 'a' && c <= 'f') return c - 'a' + 10;
	if (c >= 'A' && c <= 'F') return c - 'A' + 10;
	return 0;
}

// GeneralsX @bugfix Android port 12/07/2026 - 64KB was too small: a build
// this size (DXVK, GNS + its ICE/signaling worker threads, curl, freetype,
// SDL3, etc.) routinely produces a /proc/self/maps well past 64KB, so the
// read loop below silently truncated before reaching the entries needed to
// resolve the crash PC/LR/early backtrace frames -- confirmed by comparing
// a real device crash log (PC, LR and backtrace[0..3] all "no matching
// entry") against the same build's unstripped libmain.so, where backtrace[4]
// onward (present in the captured portion) resolved cleanly to ordinary,
// uncorrupted call frames (SDL_main -> GameMain -> SDL3GameEngine::execute),
// i.e. the process wasn't corrupted, the maps read just ran out of buffer.
static char s_mapsBuf[1 << 20];

bool findLibraryForAddress(uintptr_t pc, char *outName, size_t outNameLen, uintptr_t *outOffset) {
	int fd = open("/proc/self/maps", O_RDONLY);
	if (fd < 0) {
		return false;
	}
	ssize_t total = 0;
	ssize_t n;
	while (total < (ssize_t)sizeof(s_mapsBuf) - 1 &&
	       (n = read(fd, s_mapsBuf + total, sizeof(s_mapsBuf) - 1 - (size_t)total)) > 0) {
		total += n;
	}
	close(fd);
	if (total <= 0) {
		return false;
	}
	s_mapsBuf[total] = '\0';

	// GeneralsX @bugfix Android port 06/08/2026 (pc - start) + fileOffset is
	// only the correct file-relative offset for a segment where
	// p_vaddr == p_offset -- true of a library's FIRST PT_LOAD segment (both
	// are 0 there) but NOT for later segments: NDK/lld inserts a page of
	// vaddr-only padding at every permission-boundary crossing (R-X -> RW)
	// that isn't mirrored in file-offset space, so start-fileOffset grows by
	// one page per segment (confirmed via readelf -l on our own shipped
	// libdxvk_d3d9.so: +0x1000, +0x2000, +0x3000 across its three LOAD
	// segments). Every "library+0x..." offset this handler has ever logged
	// for a PC/LR/backtrace frame landing outside a library's first segment
	// was wrong by that page-scale constant, silently pointing addr2line at
	// the wrong function -- root-caused after two agent-assisted symbol
	// lookups on a real device crash kept landing on implausible
	// instructions. The correct, ASLR-stable quantity is "pc - load bias",
	// where the load bias is the runtime start address of the library's
	// first mapped segment (whose fileOffset is 0, so its vaddr is 0 too);
	// /proc/self/maps always lists one library's segments as a contiguous
	// run, so track the bias of the run currently being scanned and reset it
	// whenever the path changes.
	uintptr_t libBase = 0;
	char libBasePath[192];
	libBasePath[0] = '\0';

	char *line = s_mapsBuf;
	char *bufEnd = s_mapsBuf + total;
	while (line < bufEnd) {
		char *lineEnd = line;
		while (lineEnd < bufEnd && *lineEnd != '\n') {
			lineEnd++;
		}

		uintptr_t start = 0, end = 0, fileOffset = 0;
		char *p = line;
		while (p < lineEnd && *p != '-') { start = (start << 4) | (uintptr_t)hexDigitValue(*p); p++; }
		if (p < lineEnd) p++; // skip '-'
		while (p < lineEnd && *p != ' ') { end = (end << 4) | (uintptr_t)hexDigitValue(*p); p++; }
		while (p < lineEnd && *p == ' ') p++;
		while (p < lineEnd && *p != ' ') p++; // skip perms field
		while (p < lineEnd && *p == ' ') p++;
		while (p < lineEnd && *p != ' ') { fileOffset = (fileOffset << 4) | (uintptr_t)hexDigitValue(*p); p++; }

		char *pathStart = lineEnd;
		while (pathStart > line && *(pathStart - 1) != ' ') {
			pathStart--;
		}
		size_t pathLen = (size_t)(lineEnd - pathStart);
		bool samePath = pathLen > 0 && pathLen == strlen(libBasePath) &&
			memcmp(pathStart, libBasePath, pathLen) == 0;
		if (!samePath && pathLen > 0) {
			// New library run starting: its first segment's fileOffset is 0
			// and start IS the load bias. If this run's very first line
			// doesn't have fileOffset 0 (seen for some non-ELF mappings),
			// libBase stays a best-effort fallback equal to that start.
			libBase = start - fileOffset;
			size_t copyLen = pathLen < sizeof(libBasePath) - 1 ? pathLen : sizeof(libBasePath) - 1;
			memcpy(libBasePath, pathStart, copyLen);
			libBasePath[copyLen] = '\0';
		}

		if (pc >= start && pc < end) {
			if (pathLen > 0 && pathLen < outNameLen) {
				memcpy(outName, pathStart, pathLen);
				outName[pathLen] = '\0';
			} else {
				outName[0] = '\0';
			}
			*outOffset = pc - libBase;
			return true;
		}

		line = lineEnd + 1;
	}
	return false;
}

// GeneralsX @bugfix Android port 07/07/2026 Context.getFilesDir() (what
// SDL_GetAndroidInternalStoragePath() and this path both need to agree with)
// resolves under /data/user/<userId>/<pkg>/files on modern Android; the
// /data/data/<pkg> shortcut is only a symlink to user 0's data and silently
// resolves to a WRONG, nonexistent-for-this-run location on a work profile,
// a secondary user, or guest mode. Android multi-user UIDs are always
// userId*100000 + appId (a stable, documented convention), so derive the
// real user id from getuid() instead of assuming user 0.
char s_crashLogPath[256];

void computeCrashLogPath() {
	int userId = (int)(getuid() / 100000);
	snprintf(s_crashLogPath, sizeof(s_crashLogPath),
		"/data/user/%d/com.generalsx.zerohour/files/crash.log", userId);
}

// GeneralsX @bugfix Android port 30/07/2026 crash.log is append-only by
// design -- a crash mid-write must never truncate what's already on disk --
// but that also means it never shrinks on its own. A tester who reinstalls
// across dozens of test builds without ever tapping "Clear Logs" ends up
// with every launch stamp and every crash block from every prior session
// concatenated into one growing file, with the build-compiled stamp being
// the only (and, per the ccache note above, not fully reliable) way to tell
// them apart. Rotate it the same way SDL3Main.cpp already rotates
// generals-stderr.log: move whatever is there into crash-prev.log before
// this session writes its own first byte, so crash.log holds at most this
// session's own content, and the previous session's record is still one
// tap away in Settings -> View Logs instead of accumulating forever.
void rotatePrevCrashLog() {
	if (s_crashLogPath[0] == '\0') {
		return;
	}
	int userId = (int)(getuid() / 100000);
	char prevPath[256];
	snprintf(prevPath, sizeof(prevPath),
		"/data/user/%d/com.generalsx.zerohour/files/crash-prev.log", userId);
	rename(s_crashLogPath, prevPath);
}

struct sigaction s_prevHandlers[NSIG];
bool s_haveHandlerFor[NSIG] = {};

// Appends a message using only async-signal-safe primitives (open/write/close
// are POSIX async-signal-safe; snprintf into a stack buffer with no malloc
// path is the one pragmatic exception here — bionic's implementation does
// not lock or allocate for a plain format string like this one, and every
// widely-shipped Android crash handler makes the same trade-off).
void appendCrashLog(const char *message, size_t length) {
	if (s_crashLogPath[0] == '\0') {
		return;
	}
	int fd = open(s_crashLogPath, O_WRONLY | O_CREAT | O_APPEND, 0644);
	if (fd < 0) {
		return;
	}
	ssize_t written = 0;
	while (written < (ssize_t)length) {
		ssize_t n = write(fd, message + written, length - (size_t)written);
		if (n <= 0) {
			break;
		}
		written += n;
	}
	close(fd);
}

// GeneralsX @bugfix Android port 11/07/2026 Two real-device crashes in a row
// resolved to "no matching /proc/self/maps entry" for PC alone -- a wild
// jump (corrupted vtable/function pointer or a call through freed memory),
// not something a single address can diagnose. Log LR (x30, the return
// address at the moment of the bad call/branch) and walk the AArch64
// frame-pointer chain (x29 -> [saved x29, saved x30]) a few frames up: even
// when the jump target itself is garbage, the caller that made the bad call
// is usually still a valid, resolvable address, since the CPU sets LR
// before branching. Frame-pointer chains are kept by default by the NDK
// clang toolchain for arm64, so this is a plain, no-libunwind backtrace.
void logResolvedAddress(const char *label, uintptr_t addr) {
	char buf[256];
	if (addr == 0) {
		// GeneralsX @bugfix Android port 30/07/2026 This used to silently
		// return here, on the assumption that 0 only ever meant "no value
		// available" (e.g. the backtrace walker's own savedLR==0 loop
		// terminator, still handled separately below). That hid the single
		// most important fact in a real crash: PC==0 IS the finding, not a
		// missing value -- it means execution branched through a null (or
		// zeroed/corrupted) function pointer or GOT/PLT entry, as opposed to
		// a normal fault while executing real code at LR. A run of Mali-G76
		// crashes all had fault_addr=0x0 and a perfectly resolvable "crash
		// LR" (the call site), while "crash PC" never appeared in any log
		// at all -- this line is why, and it was mistaken for "logging
		// never ran" instead of "PC really is null, silently dropped".
		int zlen = snprintf(buf, sizeof(buf), "%s=0x0 (NULL -- branch/call through a "
			"null or corrupted function pointer, not a fault while executing real code)\n", label);
		if (zlen > 0) {
			appendCrashLog(buf, (size_t)zlen < sizeof(buf) ? (size_t)zlen : sizeof(buf) - 1);
		}
		return;
	}
	char libName[192];
	uintptr_t libOffset = 0;
	int len;
	if (findLibraryForAddress(addr, libName, sizeof(libName), &libOffset)) {
		len = snprintf(buf, sizeof(buf), "%s=%p is in %s+0x%lx\n",
			label, (void *)addr, libName, (unsigned long)libOffset);
	} else {
		len = snprintf(buf, sizeof(buf), "%s=%p (no matching /proc/self/maps entry)\n", label, (void *)addr);
	}
	if (len > 0) {
		appendCrashLog(buf, (size_t)len < sizeof(buf) ? (size_t)len : sizeof(buf) - 1);
	}
}

void androidCrashHandler(int sig, siginfo_t *info, void *ucontext) {
	char buf[512];
	const void *faultAddr = (info != nullptr) ? info->si_addr : nullptr;
	int len = snprintf(buf, sizeof(buf),
		"\n=== NATIVE CRASH === signal=%d (%s) fault_addr=%p pid=%d tid=%d ===\n"
		"If you're reading this: open the GeneralsZH Setup app -> View Logs,\n"
		"and share this file's contents.\n",
		sig, strsignal(sig), faultAddr, (int)getpid(), (int)gettid());
	if (len > 0) {
		appendCrashLog(buf, (size_t)len < sizeof(buf) ? (size_t)len : sizeof(buf) - 1);
	}

#if defined(__aarch64__)
	uintptr_t pc = (ucontext != nullptr) ? (uintptr_t)((ucontext_t *)ucontext)->uc_mcontext.pc : 0;
	uintptr_t lr = (ucontext != nullptr) ? (uintptr_t)((ucontext_t *)ucontext)->uc_mcontext.regs[30] : 0;
	uintptr_t fp = (ucontext != nullptr) ? (uintptr_t)((ucontext_t *)ucontext)->uc_mcontext.regs[29] : 0;
	logResolvedAddress("crash PC", pc);
	logResolvedAddress("crash LR", lr);

	if (fp != 0) {
		uintptr_t frame = fp;
		for (int i = 0; i < 16; ++i) {
			if (frame == 0 || (frame & 0xF) != 0) {
				break;
			}
			// A corrupted fp could point anywhere; this read can itself fault.
			// That's acceptable here -- every frame resolved before that point
			// has already been flushed to disk by appendCrashLog(), so a second
			// fault just ends the backtrace early instead of losing everything.
			uintptr_t *pFrame = (uintptr_t *)frame;
			uintptr_t savedFP = pFrame[0];
			uintptr_t savedLR = pFrame[1];
			if (savedLR == 0) {
				break;
			}
			char label[24];
			snprintf(label, sizeof(label), "backtrace[%d]", i);
			logResolvedAddress(label, savedLR);
			if (savedFP <= frame || savedFP - frame > (1u << 20)) {
				break;
			}
			frame = savedFP;
		}
	}
#endif

	// Chain to whatever handler was previously installed (Android's own
	// debuggerd hook in the common case) so the system tombstone and the
	// logcat "FATAL SIGNAL" block still get generated too — this handler
	// only ADDS a no-adb-needed diagnostic, it doesn't replace the OS one.
	if (sig >= 0 && sig < NSIG && s_haveHandlerFor[sig]) {
		struct sigaction &prev = s_prevHandlers[sig];
		if ((prev.sa_flags & SA_SIGINFO) != 0 && prev.sa_sigaction != nullptr) {
			prev.sa_sigaction(sig, info, ucontext);
			return;
		}
		if (prev.sa_handler != SIG_DFL && prev.sa_handler != SIG_IGN && prev.sa_handler != nullptr) {
			prev.sa_handler(sig);
			return;
		}
	}
	signal(sig, SIG_DFL);
	raise(sig);
}

// GeneralsX @bugfix Android port 13/07/2026 GitHub issue #2: several reports
// show the engine log stopping mid-line, deep inside recursive/nested INI
// block parsing (e.g. an object's ParticleSysBone sub-blocks), with VmRSS
// still low (~247MB on a report from a 12-24GB device -- nowhere near an
// OOM-kill threshold) and zero output from this very handler. A SIGSEGV
// caused by exhausting the thread's own stack (deep recursion) delivers the
// signal onto that SAME already-exhausted stack when no alternate signal
// stack is registered -- the handler then immediately faults again trying
// to push its own locals, and the process dies silently before a single
// byte reaches appendCrashLog(). sigaltstack() + SA_ONSTACK gives the
// handler a separate, always-valid stack to run on regardless of what state
// the crashing thread's own stack is in, so a stack-overflow SIGSEGV
// produces the same PC/LR/backtrace crash.log entry as any other fault
// instead of just going silent.
char s_altStack[64 * 1024];

__attribute__((constructor))
void installAndroidCrashHandler() {
	computeCrashLogPath();
	rotatePrevCrashLog();

	stack_t ss;
	ss.ss_sp = s_altStack;
	ss.ss_size = sizeof(s_altStack);
	ss.ss_flags = 0;
	sigaltstack(&ss, nullptr);

	struct sigaction sa;
	memset(&sa, 0, sizeof(sa));
	sa.sa_sigaction = androidCrashHandler;
	sa.sa_flags = SA_SIGINFO | SA_RESTART | SA_ONSTACK;
	sigemptyset(&sa.sa_mask);

	const int signals[] = { SIGSEGV, SIGABRT, SIGBUS, SIGILL, SIGFPE };
	for (int s : signals) {
		if (sigaction(s, &sa, &s_prevHandlers[s]) == 0) {
			s_haveHandlerFor[s] = true;
		}
	}

	// A signpost written unconditionally at load time: if crash.log exists
	// but ends right after this line, the crash happened somewhere between
	// here and the next log line the engine itself writes (SDL3Main.cpp) —
	// i.e. during the rest of dlopen's constructors or very early SDL_main,
	// still before the regular stderr log redirect is set up.
	time_t now = time(nullptr);
	char stamp[256];
	// GeneralsX @feature Android port 16/07/2026 Stamp the exact build (compile
	// date/time) into both crash.log and stderr at load. Reporters can't tell
	// which APK a given log came from otherwise (issue #2, requested by a tester)
	// -- and that ambiguity made it impossible to trust whether a diagnostic
	// change was actually in the tested binary. __DATE__/__TIME__ uniquely
	// identify each CI build.
	int len = snprintf(stamp, sizeof(stamp),
		"\n=== libmain.so loaded, crash handler installed (t=%ld) [build compiled %s %s] ===\n",
		(long)now, __DATE__, __TIME__);
	if (len > 0) {
		appendCrashLog(stamp, (size_t)len < sizeof(stamp) ? (size_t)len : sizeof(stamp) - 1);
	}
	fprintf(stderr, "[GX-BUILD] libmain.so compiled %s %s\n", __DATE__, __TIME__);
	fflush(stderr);
}

} // namespace

// GeneralsX @bugfix Android port 18/07/2026 Interpose __cxa_throw to log the
// call stack of every C++ throw. Issue #2's crash is an exception reaching
// GameEngine::update()'s catch(...) whose typeinfo demangles to '$_0' --
// Clang's name for an ANONYMOUS type. The engine's INI status codes
// (INI_INVALID_DATA et al., Common/INI.h) are exactly that: an unnamed enum,
// so its typeinfo has internal linkage and a per-translation-unit identity,
// which is why every typed catch clause we added (including
// catch(decltype(INI_INVALID_DATA))) failed to match a throw coming from a
// different .cpp file -- the Itanium ABI compares typeinfo identity, and each
// TU has its own for an unnamed type. There are 134 `throw INI_INVALID_DATA;`
// sites, so instead of instrumenting them all, log the throw itself: since
// libmain.so is the root of its dlopen group and links libc++_shared, this
// definition preempts libc++abi's for every throw made by engine code (and
// the bundled libs in our group), records the unwind stack with dladdr
// symbol names (the engine builds with default visibility, so .dynsym keeps
// real function names even in the stripped APK), then forwards to the real
// __cxa_throw from libc++_shared so exception semantics stay untouched.
// Capped so a throw-happy code path cannot flood the log.

#if defined(__ANDROID__)

#include <cxxabi.h>
#include <dlfcn.h>
#include <unwind.h>
#include <typeinfo>
#include <stdlib.h>

namespace {

struct GxUnwindState {
	void** current;
	void** end;
};

_Unwind_Reason_Code gxUnwindCallback(struct _Unwind_Context* context, void* arg) {
	GxUnwindState* state = static_cast<GxUnwindState*>(arg);
	uintptr_t pc = _Unwind_GetIP(context);
	if (pc) {
		if (state->current == state->end)
			return _URC_END_OF_STACK;
		*state->current++ = reinterpret_cast<void*>(pc);
	}
	return _URC_NO_REASON;
}

} // namespace

extern "C" void __cxa_throw(void* thrown_exception, std::type_info* tinfo, void (*dest)(void*)) {
	using CxaThrowFn = void (*)(void*, std::type_info*, void (*)(void*));
	static CxaThrowFn realCxaThrow =
		reinterpret_cast<CxaThrowFn>(dlsym(RTLD_NEXT, "__cxa_throw"));

	static int logged = 0;
	if (logged < 64) {
		++logged;
		const char* mangled = tinfo ? tinfo->name() : "<null>";
		int status = 0;
		char* demangled = abi::__cxa_demangle(mangled, nullptr, nullptr, &status);
		// The first 4 bytes cover every enum/int payload the engine throws
		// (e.g. INI_* == ERROR_BAD_INI); garbage for class-type payloads but
		// harmless to print.
		unsigned payload = 0;
		if (thrown_exception)
			memcpy(&payload, thrown_exception, sizeof(payload));
		fprintf(stderr, "[GX-THROW] type='%s' (mangled='%s') first4bytes=0x%08x\n",
			demangled ? demangled : mangled, mangled, payload);
		if (demangled)
			free(demangled);

		void* frames[24];
		GxUnwindState state{ frames, frames + 24 };
		_Unwind_Backtrace(gxUnwindCallback, &state);
		int frameCount = (int)(state.current - frames);
		for (int i = 0; i < frameCount; ++i) {
			Dl_info info;
			const char* lib = "?";
			const char* sym = nullptr;
			uintptr_t off = reinterpret_cast<uintptr_t>(frames[i]);
			if (dladdr(frames[i], &info)) {
				if (info.dli_fname) {
					const char* slash = strrchr(info.dli_fname, '/');
					lib = slash ? slash + 1 : info.dli_fname;
				}
				if (info.dli_fbase)
					off = reinterpret_cast<uintptr_t>(frames[i]) - reinterpret_cast<uintptr_t>(info.dli_fbase);
				sym = info.dli_sname;
			}
			char* symDemangled = nullptr;
			if (sym) {
				int symStatus = 0;
				symDemangled = abi::__cxa_demangle(sym, nullptr, nullptr, &symStatus);
			}
			fprintf(stderr, "[GX-THROW]   #%02d %s+0x%zx %s\n",
				i, lib, (size_t)off, symDemangled ? symDemangled : (sym ? sym : ""));
			if (symDemangled)
				free(symDemangled);
		}
		fflush(stderr);
	}

	if (realCxaThrow)
		realCxaThrow(thrown_exception, tinfo, dest);

	// Only reachable if dlsym failed entirely -- there is no way to continue
	// a throw without the runtime, so make the failure loud instead of UB.
	fprintf(stderr, "[GX-THROW] FATAL: real __cxa_throw not found via RTLD_NEXT\n");
	fflush(stderr);
	abort();
}

#endif // __ANDROID__

#endif // __ANDROID__
