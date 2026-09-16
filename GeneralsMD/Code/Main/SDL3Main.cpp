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

/*
** SDL3Main.cpp
**
** Entry point for Linux builds using SDL3 windowing and DXVK graphics.
**
** TheSuperHackers @feature CnC_Generals_Linux 07/02/2026
** Entry point replaces WinMain() for Linux builds.
** Instantiates SDL3GameEngine and calls GameMain().
*/

#ifndef _WIN32

// SYSTEM INCLUDES
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif
// GeneralsX @build Android port 06/07/2026 Shared guard for the touch-first
// mobile platforms (matches SDL3GameEngine.cpp). Both need SDL's entry-point
// wrapper, both synthesize mouse events from touch, both pick the internal
// resolution from the real display size.
#if (defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE) || defined(__ANDROID__)
#define SAGE_MOBILE_PLATFORM 1
#endif
#if defined(SAGE_MOBILE_PLATFORM)
// On iOS, SDL renames main() to SDL_main and provides its own UIApplicationMain
// bootstrap; the app lifecycle (suspend/resume, window) is owned by SDL.
// On Android the same include renames main() to SDL_main, which the SDLActivity
// Java shell (android/) invokes inside the app process after loading libmain.so.
#include <SDL3/SDL_main.h>
#include <cerrno>
#include <sys/stat.h>
#include <fcntl.h>
#include <filesystem>
#include <string>
#endif
#if defined(__ANDROID__)
// GeneralsX @feature Android port 10/07/2026 Optional custom Vulkan driver
// loading (Adreno/Turnip), see TryLoadCustomVulkanDriver() below.
#include <jni.h>
#include <dlfcn.h>
#include <adrenotools/driver.h>
#include <android/api-level.h>
// GeneralsX @build Android port render-backend picker 07/09/2026 -
// d3d8gles_ShouldUseVulkanBackend()/d3d8gles_ShouldUseANGLE(), see UseVulkanBackend()/UseANGLE() below.
#include "d3d8gles.h"
#endif
#include <cstdlib>
#include <cctype>
#include <cstring>
#include <cstdio>
#include <unistd.h>   // _exit()
#include <glob.h>     // glob() for Vulkan ICD discovery

// USER INCLUDES (match WinMain.cpp pattern)
#include "Lib/BaseType.h"
#include "Common/CommandLine.h"
#include "Common/CriticalSection.h"
#include "Common/GlobalData.h"
#include "Common/GameEngine.h"
#include "Common/GameMemory.h"
#include "Common/Debug.h"
#include "Common/version.h"  // GeneralsX @bugfix BenderAI 14/02/2026 Version class + TheVersion extern
#include "SDL3GameEngine.h"
#include "GeneratedVersion.h"  // GeneralsX @feature Android port 12/07/2026 ANDROID_CI_BUILD_NUMBER

// DXVK WSI
#define DXVK_WSI_SDL3 1
#include <wsi/native_wsi.h>

// CRITICAL SECTIONS (Linux needs these too)
static CriticalSection critSec1;
static CriticalSection critSec2;
static CriticalSection critSec3;
static CriticalSection critSec4;
static CriticalSection critSec5;

// GLOBAL COMMAND LINE ARGUMENTS
// TheSuperHackers @build felipebraz 13/02/2026
// Store argc/argv from main() for use by CommandLine.cpp parseCommandLine() on Linux
// Windows provides these automatically; Linux needs explicit globals
int __argc = 0;          ///< global argument count
char** __argv = nullptr; ///< global argument vector

// GLOBAL WINDOW HANDLE
// TheSuperHackers @build felipebraz 13/02/2026
// ApplicationHWnd is declared extern in GeneralsMD/Code/Main/WinMain.h
// On Linux, we cast SDL_Window* to HWND type for compatibility
HWND ApplicationHWnd = nullptr;  ///< our application window handle

// GLOBAL SDL3 WINDOW
// GeneralsX @feature felipebraz 16/02/2026
// SDL3 window created in main() before GameMain(), stored globally for engine access
SDL_Window* TheSDL3Window = nullptr;

// GeneralsX @build Codex 16/09/2026 Android XR boot defines this itself;
// non-Android SDL builds have no offscreen XR boot and use normal windowing.
#if !defined(__ANDROID__)
bool GX_XR_OffscreenBoot = false;
#endif

// GAME TEXT FILE PATHS
// TheSuperHackers @build felipebraz 13/02/2026
// GameText.cpp uses these paths to load CSF and STR files (game localization)
// Format %s is replaced with language code in GameTextManager::init()
// GeneralsX @bugfix BenderAI 13/02/2026 - Fix case-sensitivity on Linux (generals.csf vs Generals.csf)
const Char *g_csfFile = "data/%s/generals.csf";  ///< CSF file path (lowercase for Linux compatibility)
// GeneralsX @feature Android port 09/09/2026 Per-language, like g_csfFile above it.
// GameTextManager::init() fills the %s in with the current language, and prefers this
// plain-text file over the compiled .csf when it exists -- which is what makes a language
// pack a text file someone can translate and send as a pull request, rather than a binary
// nobody can review. See languages/README.md.
const Char *g_strFile = "data/%s/generals.str";  ///< STR file path, per language

// Extern declarations (from GameMain.cpp)
extern Int GameMain();

/**
 * FilterSoftwareVulkanICDs
 *
 * Sets VK_DRIVER_FILES to only hardware Vulkan ICDs, excluding LLVMpipe/lavapipe.
 *
 * Workaround for Mesa/LLVM 20.x bug: libvulkan_lvp.so (LLVMpipe Vulkan ICD) crashes
 * during dlopen() static initialization with a null-ptr deref in llvm::Regex::Regex().
 * The Vulkan loader loads ALL ICDs found in the ICD directories when
 * vkEnumerateInstanceExtensionProperties() is called, which triggers the crash.
 * Filtering hardware-only ICDs via VK_DRIVER_FILES prevents loading libvulkan_lvp.so.
 *
 * Only applied when neither VK_DRIVER_FILES nor VK_ICD_FILENAMES is already set,
 * so the user can always override by setting those variables externally.
 *
 * GeneralsX @bugfix BenderAI 06/03/2026
 */
/**
 * GeneralsX @build Android port render-backend picker 07/09/2026 - the
 * Vulkan/GLES/GLES+ANGLE decision (render_backend.cfg / env var fallback) is
 * made once, in d3d8gles_ShouldUseVulkanBackend()/d3d8gles_ShouldUseANGLE()
 * (Core/Libraries/Source/d3d8gles/src/d3d8gles.cpp -- see d3d8gles.h's
 * comment on them for why). This used to be its own separate copy of the
 * same check; it drifted out of sync with dx8wrapper.cpp's copy the moment
 * the Setup app's picker was added to only one of them, producing a Vulkan
 * SDL window paired with a GLES D3D8 backend underneath it and a black
 * screen. Thin wrappers here just adapt the shared bool to this file's
 * existing UseVulkanBackend()/UseANGLE() call sites.
 */
static bool UseVulkanBackend()
{
#if defined(__ANDROID__)
	return d3d8gles_ShouldUseVulkanBackend();
#else
	return true;
#endif
}

static bool UseANGLE()
{
#if defined(__ANDROID__)
	return d3d8gles_ShouldUseANGLE();
#else
	return false;
#endif
}

#if !defined(__ANDROID__)
static void FilterSoftwareVulkanICDs()
{
	if (getenv("VK_DRIVER_FILES") || getenv("VK_ICD_FILENAMES")) {
		return;
	}

	auto icd_is_software = [](const char *name) -> bool {
		char low[256] = "";
		for (int i = 0; name[i] && i < 255; ++i) {
			low[i] = (char)tolower((unsigned char)name[i]);
		}
		return strstr(low, "lvp") || strstr(low, "lavapipe") || strstr(low, "softpipe") || strstr(low, "llvmpipe");
	};

	static char hw_icds[4096] = "";
	const char *patterns[] = {
		"/usr/share/vulkan/icd.d/*.json",
		"/etc/vulkan/icd.d/*.json",
		nullptr
	};

	glob_t gl = {};
	int gflags = 0;
	for (int i = 0; patterns[i]; ++i) {
		if (glob(patterns[i], gflags, nullptr, &gl) == 0) {
			gflags = GLOB_APPEND;
		}
	}

	bool found_hw = false;
	for (size_t i = 0; i < gl.gl_pathc; ++i) {
		const char *path = gl.gl_pathv[i];
		const char *base = strrchr(path, '/');
		base = base ? base + 1 : path;
		if (icd_is_software(base)) {
			fprintf(stderr, "INFO: Vulkan ICD filter: skipping software ICD '%s'\n", base);
			continue;
		}
		if (found_hw) {
			strncat(hw_icds, ":", sizeof(hw_icds) - strlen(hw_icds) - 1);
		}
		strncat(hw_icds, path, sizeof(hw_icds) - strlen(hw_icds) - 1);
		found_hw = true;
	}
	globfree(&gl);

	if (found_hw) {
		setenv("VK_DRIVER_FILES", hw_icds, 1);
		fprintf(stderr, "INFO: Vulkan ICD filter: VK_DRIVER_FILES=%s\n", hw_icds);
	} else {
		fprintf(stderr, "WARNING: Vulkan ICD filter: no hardware ICDs found, LLVMpipe exclusion skipped\n");
		fprintf(stderr, "WARNING: If startup crashes in libvulkan_lvp.so, set VK_DRIVER_FILES manually\n");
	}
}
#endif // !__ANDROID__

/**
 * FilterPipeWireOpenAL
 *
 * Sets ALSOFT_DRIVERS to skip PipeWire, falling back to pulse/alsa.
 *
 * Workaround for openal-soft PipeWire backend crash: alcOpenDevice() segfaults
 * inside the PipeWire backend while opening the default playback device.
 * The crash occurs in PipeWire's stream/context internals and is unrecoverable
 * from userspace. Excluding PipeWire via ALSOFT_DRIVERS causes openal-soft to
 * fall back to the PulseAudio backend, which works correctly on PipeWire systems
 * via the PulseAudio compatibility layer.
 *
 * NOTE: openal-soft reads ALSOFT_DRIVERS from a static global constructor when
 * libopenal.so is loaded by the dynamic linker, which is before main() runs.
 * This function is therefore only effective for builds that use lazy
 * initialization. The authoritative fix is in the launch scripts (run-linux-zh.sh
 * etc.), which set ALSOFT_DRIVERS before the binary starts.
 *
 * Only applied when ALSOFT_DRIVERS is not already set by the user.
 *
 * GeneralsX @bugfix 09/03/2026
 */
static void FilterPipeWireOpenAL()
{
	// GeneralsX @bugfix Copilot 24/03/2026 PipeWire/OpenAL workaround is Linux-only; keep macOS CoreAudio backend selection untouched.
	// GeneralsX @build Android port 06/07/2026 Android defines __linux__ too, but has neither
	// PipeWire nor PulseAudio — forcing ALSOFT_DRIVERS=pulse,alsa here would silently mute the
	// device (openal-soft must pick its OpenSL/AAudio backends). Desktop Linux only.
	#if defined(__linux__) && !defined(__ANDROID__)
	// Crash: alcOpenDevice() hits 'movaps %xmm1,0x26260(%rbx)' — SSE movaps requires
	// 16-byte alignment; a misaligned ALCdevice struct faults regardless of backend.
	// Disabling CPU extensions forces openal-soft to use scalar code that has no
	// alignment requirements. Also exclude pipewire which has its own crash at
	// device-open time on PipeWire 1.4.x.
	// NOTE: these env vars are authoritative only when set before the binary loads
	// (openal-soft reads them from a static constructor). The launch scripts set them
	// first; this is a best-effort fallback for lazy-init builds.
	if (!getenv("ALSOFT_DISABLE_CPU_EXTS")) {
		setenv("ALSOFT_DISABLE_CPU_EXTS", "all", 1);
		fprintf(stderr, "INFO: OpenAL: ALSOFT_DISABLE_CPU_EXTS=all (movaps alignment crash workaround)\n");
	}
	if (!getenv("ALSOFT_DRIVERS")) {
		setenv("ALSOFT_DRIVERS", "pulse,alsa,oss,jack,null,wave", 1);
		fprintf(stderr, "INFO: OpenAL: ALSOFT_DRIVERS=pulse,alsa,oss,jack,null,wave (pipewire excluded)\n");
	}
	#else
	fprintf(stderr, "INFO: OpenAL: keeping default driver selection on non-Linux platform\n");
	#endif
}

#if defined(__ANDROID__)
/**
 * TryLoadCustomVulkanDriver
 *
 * GeneralsX @feature Android port 10/07/2026 Optional user-supplied Vulkan
 * driver (most commonly a Mesa Turnip build for Adreno GPUs), loaded via
 * libadrenotools (https://github.com/bylaws/libadrenotools, BSD-2-Clause).
 * adrenotools installs a linker-namespace-bypass hook that transparently
 * redirects EVERY later dlopen("libvulkan.so") call in this process to the
 * custom driver -- including DXVK's own internal load in
 * references/fbraz3-dxvk/src/vulkan/vulkan_loader.cpp, which is a separate
 * meson-built .so and is never patched or made aware of this. Must run
 * before that first happens, i.e. before SDL_Vulkan_LoadLibrary() below.
 *
 * The Setup app (SetupActivity.java) writes <internalPath>/custom_driver.cfg
 * (one line: the driver .so's soname) and unpacks the driver itself into
 * <internalPath>/custom_driver/ -- either because the user imported one via
 * its "Custom Vulkan Driver" section, or because
 * applyRecommendedDriverIfNeeded() auto-selected the bundled Turnip build
 * for an Adreno phone whose stock driver reports less than Vulkan 1.3. Both
 * files are absent when neither applies, in which case this is a no-op and
 * the stock vendor driver loads exactly as before.
 *
 * Adreno-only: Turnip has no Mali backend, so this cannot help phones whose
 * GPU is Mali (see the Mali-G76 case documented further down this file) --
 * those remain capped at whatever Vulkan version their proprietary driver
 * reports. What it DOES help: Adreno phones whose stock driver reports
 * Vulkan 1.1/1.2 while DXVK 2.6 needs 1.3.
 */
static void TryLoadCustomVulkanDriver(const char *internalPath)
{
	char cfgPath[1024];
	snprintf(cfgPath, sizeof(cfgPath), "%s/custom_driver.cfg", internalPath);
	FILE *cfg = fopen(cfgPath, "r");
	if (cfg == nullptr) {
		return;  // no custom driver configured -- stock driver loads as usual
	}
	char driverName[256] = {0};
	bool haveDriverName = (fgets(driverName, sizeof(driverName), cfg) != nullptr);
	fclose(cfg);
	if (!haveDriverName) {
		return;
	}
	size_t len = strlen(driverName);
	while (len > 0 && (driverName[len - 1] == '\n' || driverName[len - 1] == '\r')) {
		driverName[--len] = '\0';
	}
	if (len == 0) {
		return;
	}

	char driverDir[1024];
	// GeneralsX @bugfix Android port 01/08/2026 adrenotools_open_libvulkan()
	// internally does `std::string(customDriverDir) + customDriverName` with
	// NO separator inserted (driver.cpp, right before the pre-flight stat()
	// check) -- customDriverDir must already carry its own trailing slash, or
	// the concatenated path is garbage (".../custom_driverlibvulkan_x.so"
	// instead of ".../custom_driver/libvulkan_x.so") and stat() fails before
	// dlopen() is ever reached, which is exactly why adding a dlerror() log
	// to the failure path earlier showed "(none)" -- the failure was a plain
	// ENOENT from stat(), never a dlopen() error at all. This has silently
	// broken every custom-driver (Turnip) import on Android since the
	// feature was added.
	snprintf(driverDir, sizeof(driverDir), "%s/custom_driver/", internalPath);
	if (access(driverDir, R_OK) != 0) {
		fprintf(stderr, "WARNING: custom_driver.cfg names '%s' but %s doesn't exist -- using stock Vulkan driver\n",
		        driverName, driverDir);
		return;
	}

	// hookLibDir MUST be exactly what ApplicationInfo.nativeLibraryDir
	// returns -- it contains a random per-install path component on modern
	// Android and cannot be hardcoded, so fetch it via JNI from the JVM side
	// the app is already running in (SDLActivity).
	JNIEnv *jni = static_cast<JNIEnv *>(SDL_GetAndroidJNIEnv());
	jobject activity = static_cast<jobject>(SDL_GetAndroidActivity());
	if (jni == nullptr || activity == nullptr) {
		fprintf(stderr, "WARNING: custom Vulkan driver requested but no JNI environment -- using stock driver\n");
		return;
	}

	std::string hookLibDir;
	{
		jclass activityClass = jni->GetObjectClass(activity);
		jmethodID getApplicationInfo = jni->GetMethodID(
			activityClass, "getApplicationInfo", "()Landroid/content/pm/ApplicationInfo;");
		jobject appInfo = jni->CallObjectMethod(activity, getApplicationInfo);
		jclass appInfoClass = jni->GetObjectClass(appInfo);
		jfieldID nativeLibraryDirField = jni->GetFieldID(appInfoClass, "nativeLibraryDir", "Ljava/lang/String;");
		jstring nativeLibraryDirStr = static_cast<jstring>(jni->GetObjectField(appInfo, nativeLibraryDirField));
		if (nativeLibraryDirStr != nullptr) {
			const char *chars = jni->GetStringUTFChars(nativeLibraryDirStr, nullptr);
			if (chars != nullptr) {
				hookLibDir = chars;
				jni->ReleaseStringUTFChars(nativeLibraryDirStr, chars);
			}
			jni->DeleteLocalRef(nativeLibraryDirStr);
		}
		jni->DeleteLocalRef(appInfo);
		jni->DeleteLocalRef(appInfoClass);
		jni->DeleteLocalRef(activityClass);
	}
	if (hookLibDir.empty()) {
		fprintf(stderr, "WARNING: could not resolve nativeLibraryDir -- using stock Vulkan driver\n");
		return;
	}

	void *mappingHandle = nullptr;
	// GeneralsX @bugfix Android port 01/08/2026 The trailing-slash fix above
	// (commit 453bb4ec6) didn't resolve the real-device failure -- still
	// "(none)" from dlerror() after that fix too, meaning either the stat()
	// pre-check still isn't finding the file for some OTHER reason, or the
	// failure is actually happening at one of adrenotools' EARLIER,
	// dlopen()-independent bail-outs (driver.cpp: linkernsbypass_load_status()
	// failing "probably means we're on api < 28", or the isolated-namespace
	// creation/linking step) -- both return nullptr just as silently as the
	// stat() check does. Replicate adrenotools' own concatenation and stat()
	// here, and log the device API level, to see exactly which one it is.
	{
		std::string probePath = std::string(driverDir) + driverName;
		struct stat probeBuf{};
		int probeResult = stat(probePath.c_str(), &probeBuf);
		fprintf(stderr, "INFO: TryLoadCustomVulkanDriver probe: path='%s' stat=%d (errno=%d: %s) apiLevel=%d\n",
		        probePath.c_str(), probeResult, probeResult == 0 ? 0 : errno,
		        probeResult == 0 ? "ok" : strerror(errno),
		        android_get_device_api_level());
	}
	void *lib = adrenotools_open_libvulkan(
		RTLD_NOW,
		ADRENOTOOLS_DRIVER_CUSTOM,
		/* tmpLibDir */ internalPath,
		hookLibDir.c_str(),
		driverDir,
		driverName,
		/* fileRedirectDir */ nullptr,
		&mappingHandle);

	if (lib == nullptr) {
		// GeneralsX @bugfix Android port 01/08/2026 adrenotools_open_libvulkan()
		// wraps dlopen() internally for the actual driver .so; dlerror() often
		// carries the real reason (missing dependency symbol, wrong ELF class,
		// etc.) that the plain nullptr return on its own doesn't. Real-device
		// testing (Turnip import on a Snapdragon 8 Elite / Adreno 830) hit this
		// exact failure with no further detail previously logged.
		const char *dlErr = dlerror();
		fprintf(stderr, "WARNING: adrenotools_open_libvulkan('%s') failed -- falling back to stock Vulkan driver (dlerror: %s)\n",
		        driverName, dlErr ? dlErr : "(none)");
		return;
	}
	fprintf(stderr, "INFO: Loaded custom Vulkan driver '%s' via libadrenotools (hookLibDir=%s)\n",
	        driverName, hookLibDir.c_str());
}
#endif // __ANDROID__

/**
 * CreateGameEngine
 *
 * Factory function for SDL3GameEngine on Linux.
 * Called by GameMain() to instantiate platform-specific engine.
 *
 * @return SDL3GameEngine instance
 */
GameEngine *CreateGameEngine(void)
{
	fprintf(stderr, "INFO: CreateGameEngine() - Creating SDL3GameEngine for Linux\n");
	SDL3GameEngine *engine = NEW SDL3GameEngine();
	return engine;
}

/**
 * main
 *
 * Linux entry point (replaces WinMain on Windows).
 * Initializes subsystems and calls GameMain().
 *
 * @param argc Command line argument count
 * @param argv Command line arguments
 * @return Exit code (0 = success)
 */
int main(int argc, char* argv[])
{
	int exitcode = 1;

	// TheSuperHackers @build felipebraz 13/02/2026
	// Store command line arguments in globals for CommandLine.cpp parser
	__argc = argc;
	__argv = argv;

#if defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE
	// Diagnostic capture: an icon-launched app's stderr goes nowhere we can read,
	// so mirror it to a file in Library/Caches (purgeable, not user-visible). This
	// lets us pull a full engine log after an on-device session — essential for
	// debugging mode-specific issues (e.g. Generals Challenge radar/scripts) that
	// only the user can reproduce. Pull with: devicectl ... copy from
	// Library/Caches/generals-stderr.log. Remove once the relevant bugs are fixed.
	{
		// Quiet DXVK at the source: the d3d8 layer's per-call warns (e.g. an
		// unimplemented render state set every frame) wrote hundreds of MB per
		// long session. The shipped dxvk.conf also sets logLevel=none; the env
		// covers modules that read it before the config.
		setenv("DXVK_LOG_LEVEL", "none", 0);
		const char *diagHome = getenv("HOME");
		if (diagHome != nullptr) {
			char diagPath[1024];
			char prevPath[1024];
			// Documents, not Library/Caches: Caches is purgeable (a device restart or
			// storage pressure can empty it), and Documents is user-reachable via the
			// Files app since the bundle enables UIFileSharingEnabled.
			snprintf(diagPath, sizeof(diagPath), "%s/Documents/generals-stderr.log", diagHome);
			// Keep the previous session's log: a session that ends in a memory kill
			// leaves no OS crash report, so the prior log is often the only evidence.
			snprintf(prevPath, sizeof(prevPath), "%s/Documents/generals-stderr-prev.log", diagHome);
			rename(diagPath, prevPath);
			// Filtered + capped sink instead of a raw freopen: per-frame debug spam
			// (upstream [GX-ISSUE144] font traces, [INI] loader traces, residual DXVK
			// warns) is dropped, and the file stops growing at 8 MB so a marathon
			// session cannot eat device storage. funopen() is fine here: this is
			// Darwin-only code.
			static int s_logFd = -1;
			s_logFd = open(diagPath, O_WRONLY | O_CREAT | O_TRUNC, 0644);
			if (s_logFd >= 0) {
				static size_t s_logWritten = 0;
				FILE *sink = funopen(nullptr,
					nullptr,
					[](void *, const char *buf, int len) -> int {
						static const size_t kLogCap = 8u * 1024u * 1024u;
						if (s_logFd < 0) return len;
						if (len > 13 &&
						    (memcmp(buf, "[GX-ISSUE144]", 13) == 0 ||
						     memcmp(buf, "[INI] ", 6) == 0 ||
						     memcmp(buf, "warn:  D3D8De", 13) == 0)) {
							return len;  // drop known per-frame spam, report consumed
						}
						if (s_logWritten >= kLogCap) {
							// past the cap, still record errors — the tail of a dying
							// session is this log's whole reason to exist
							static bool s_capMarked = false;
							if (!s_capMarked) {
								s_capMarked = true;
								const char *mark = "[log capped: non-error lines dropped from here]\n";
								write(s_logFd, mark, strlen(mark));
							}
							if (len > 4 && (memcmp(buf, "err:", 4) == 0 ||
							                memcmp(buf, "ERROR", 5) == 0 ||
							                memcmp(buf, "FATAL", 5) == 0)) {
								write(s_logFd, buf, (size_t)len);
							}
							return len;
						}
						ssize_t w = write(s_logFd, buf, (size_t)len);
						if (w > 0) s_logWritten += (size_t)w;
						return len;
					},
					nullptr, nullptr);
				if (sink != nullptr) {
					*stderr = *sink;  // classic Darwin stderr swap; stderr is a FILE, not a macro here
					setvbuf(stderr, nullptr, _IOLBF, 0);  // line-buffered so a crash still flushes recent lines
				}
			}
		}
	}

	// The engine resolves all game data relative to the working directory.
	// Preferred layout: assets ship read-only INSIDE the signed app bundle
	// (<bundle>/GameData), the iOS-sanctioned home for app resources — the
	// install is then fully self-contained. Dev builds packaged without
	// assets fall back to the Documents folder (Files-app accessible).
	// User data (saves, Options.ini) always lives in Library/Application
	// Support via the engine's user-data path; never in the bundle.
	{
		const char *home = getenv("HOME");

		// <bundle>/GameData, derived from the executable path (argv[0])
		char bundleData[1024] = {0};
		if (argc > 0 && argv[0] != nullptr) {
			const char *slash = strrchr(argv[0], '/');
			if (slash != nullptr) {
				const size_t dirLen = (size_t)(slash - argv[0]);
				if (dirLen < sizeof(bundleData) - 16) {
					memcpy(bundleData, argv[0], dirLen);
					snprintf(bundleData + dirLen, sizeof(bundleData) - dirLen, "/GameData");
				}
			}
		}

		bool usingBundleData = false;
		if (bundleData[0] != '\0' && access(bundleData, R_OK) == 0) {
			if (chdir(bundleData) == 0) {
				usingBundleData = true;
				fprintf(stderr, "INFO: iOS working directory (bundle): %s\n", bundleData);
			}
		}
		if (!usingBundleData && home != nullptr) {
			char docs[1024];
			snprintf(docs, sizeof(docs), "%s/Documents", home);
			if (chdir(docs) != 0) {
				fprintf(stderr, "WARNING: chdir(%s) failed: %s\n", docs, strerror(errno));
			} else {
				fprintf(stderr, "INFO: iOS working directory (Documents): %s\n", docs);
			}
		}

		if (home != nullptr) {
			// Keep DXVK's shader cache in Library/Caches: purgeable under
			// storage pressure, excluded from iCloud backup, invisible in the
			// Files app. Must be set before the d3d8 dylib loads.
			char cacheDir[1024];
			snprintf(cacheDir, sizeof(cacheDir), "%s/Library/Caches", home);
			mkdir(cacheDir, 0755);
			setenv("DXVK_STATE_CACHE_PATH", cacheDir, 0);

			if (usingBundleData) {
				// Seed default settings on first run (full detail instead of the
				// 2003 auto-detect, which drops unknown GPUs to Low).
				char userDataDir[1024], optionsPath[1024];
				snprintf(userDataDir, sizeof(userDataDir),
				         "%s/Library/Application Support/GeneralsX/GeneralsZH", home);
				snprintf(optionsPath, sizeof(optionsPath), "%s/Options.ini", userDataDir);
				if (access(optionsPath, F_OK) != 0 && access("DefaultOptions.ini", R_OK) == 0) {
					std::error_code fsError;
					std::filesystem::create_directories(userDataDir, fsError);
					std::filesystem::copy_file("DefaultOptions.ini", optionsPath, fsError);
					if (!fsError) {
						fprintf(stderr, "INFO: Seeded default Options.ini\n");
					}
				}

				// One-time tidy-up: remove asset copies from Documents now that
				// the bundle carries them. Guarded by a sentinel so it truly runs
				// once — Documents is exposed via the Files app, and anything the
				// user places there later (mods, custom maps) must never be touched.
				// "Maps" is deliberately NOT in the list: it is where user maps live.
				char docs[1024];
				snprintf(docs, sizeof(docs), "%s/Documents", home);
				char sentinel[1024];
				snprintf(sentinel, sizeof(sentinel), "%s/.bundle-assets-tidied", docs);
				if (access(sentinel, F_OK) != 0) {
					std::error_code fsError;
					for (const auto &entry : std::filesystem::directory_iterator(docs, fsError)) {
						const std::string name = entry.path().filename().string();
						const bool isShippedAsset =
							(name.size() > 4 && name.compare(name.size() - 4, 4, ".big") == 0) ||
							name == "Data" || name == "Window" || name == "ZH_Generals" ||
							name == "fonts" || name == "_CommonRedist" ||
							name == "dxvk.conf" || name == "GeneralsXZH.dxvk-cache" ||
							name == "GeneralsXZH_d3d9.log";
						if (isShippedAsset) {
							fprintf(stderr, "INFO: tidy-up removing shipped asset copy: %s\n", name.c_str());
							std::error_code removeError;
							std::filesystem::remove_all(entry.path(), removeError);
						}
					}
					if (!fsError) {  // a failed scan must retry next launch, not fail closed forever
						FILE *s = fopen(sentinel, "w");
						if (s) fclose(s);
					}
				}
			}
		}
	}
#endif

#if defined(__ANDROID__)
	// GeneralsX @build Android port 06/07/2026 Android process environment.
	//
	// An Android app starts with cwd "/" and no HOME. The engine resolves game
	// data relative to the working directory and user data via $HOME (XDG), so
	// both are pointed into the app's own storage before anything else runs:
	//
	//   game data  -> <external files dir>            (user drops the .big files
	//                  /storage/emulated/0/Android/data/<pkg>/files — reachable
	//                  over USB/MTP and adb; the Java shell extracts bundled
	//                  fonts/, dxvk.conf and DefaultOptions.ini there on launch)
	//   user data  -> <internal files dir>/.local/share/GeneralsX/GeneralsZH
	//                  (via HOME; survives reinstalls of external storage wipes,
	//                  never user-visible, holds Options.ini + saves + maps cache)
	//   DXVK cache -> <cache dir> (purgeable by the OS under storage pressure)
	{
		// GeneralsX @bugfix Android port 07/07/2026 "error", not "none": a
		// real-device run reached Direct3DCreate8() and threw with no
		// information beyond "unknown exception" — DXVK's own error log is
		// exactly what would explain a failure that deep (unsupported
		// instance/device extension, missing Vulkan feature, etc.) and
		// "none" was silencing it. "error" still excludes the per-frame
		// WARN-level render-state spam the iOS port's "none" setting was
		// actually chasing (hundreds of MB/session); only genuine failures
		// are rare enough not to reproduce that problem.
		setenv("DXVK_LOG_LEVEL", "error", 0);

		// GeneralsX @bugfix Android port 08/07/2026 Disable DXVK's background
		// memory defragmentation. Two real-device crash reports (screen
		// rotation in the lobby; unit creation mid-game) symbolized to
		// DxvkMemoryAllocator::createAllocation popping a corrupted
		// allocation-pool free-list head (fault_addr 0x2000000001 = a live
		// allocation's counters written over the recycled node's next
		// pointer) — a use-after-free consistent with the relocation/defrag
		// path moving allocations behind the app's back. Upstream DXVK itself
		// disables defrag on Intel ANV for "unknown reasons" breakage
		// (dxvk_memory.cpp, issue #4395); this Adreno driver appears to be
		// another such case. DXVK_CONFIG is parsed on top of dxvk.conf, so
		// this reaches existing installs whose game folder already carries an
		// older dxvk.conf. setenv(..., 0) keeps any user override in the
		// actual environment intact.
		setenv("DXVK_CONFIG", "dxvk.enableMemoryDefrag = False", 0);

		const char *internalPath = SDL_GetAndroidInternalStoragePath();
		const char *externalPath = SDL_GetAndroidExternalStoragePath();
		const char *cachePath    = SDL_GetAndroidCachePath();

		if (internalPath != nullptr) {
			// HOME drives registry.ini's location (registryini.cpp) -- a small
			// settings blob, not user content -- and, prior to 18/07/2026, the
			// engine's whole user-data dir (GlobalData.cpp XDG branch: saves,
			// Options.ini, AND custom maps). Internal storage was chosen so it
			// survives the user reshuffling/wiping the external GameData
			// folder. That's fine for registry.ini, but it silently made
			// custom maps impossible to add on Android: on the reference
			// platform (Windows) user-data lives in Documents, which is both
			// durable AND reachable with a plain file manager -- Android has
			// no folder with both properties, so something had to give.
			// GENERALSX_USERDATA_DIR (below) now carries the actual
			// user-data dir on a reachable path instead; HOME keeps pointing
			// at internal storage for registry.ini only.
			setenv("HOME", internalPath, 1);

			// GeneralsX @feature Android port 18/07/2026 issue #9 follow-up:
			// give the engine's user-data dir (GlobalData.cpp,
			// BuildUserDataPathFromRegistry) a plain, top-level, always-visible
			// home instead of the internal-storage path HOME points at above.
			// This is where custom maps (Maps/), save games, and Options.ini
			// now live -- laid out exactly like the reference (Windows)
			// platform's Documents folder: a shared "Generals" directory
			// holding one leaf per game variant ("Command and Conquer
			// Generals Zero Hour Data" for this port; "Command and Conquer
			// Generals Data" alongside it, matching vanilla Generals' own
			// Documents leaf name, reserved for if/when that variant is
			// also built for Android -- RTS_BUILD_GENERALS is OFF today).
			// This is exactly why players have been unable to add custom
			// maps at all until now (issue #9 comments). Needs
			// MANAGE_EXTERNAL_STORAGE (already requested and granted via
			// SetupActivity for the GameData folder picker) to write outside
			// the app's own sandboxed directories. Multi-user-safe the same
			// way AndroidCrashHandler.cpp derives its path: Android's shared
			// storage mount is always /storage/emulated/<userId>, and
			// per-user UIDs are always userId*100000 + appId.
			int userId = (int)(getuid() / 100000);
			char generalsRoot[280];
			int rootLen = snprintf(generalsRoot, sizeof(generalsRoot),
				"/storage/emulated/%d/Generals", userId);
			if (rootLen > 0 && (size_t)rootLen < sizeof(generalsRoot)) {
				char zhUserDataDir[400];
				int zhLen = snprintf(zhUserDataDir, sizeof(zhUserDataDir),
					"%s/Command and Conquer Generals Zero Hour Data", generalsRoot);
				if (zhLen > 0 && (size_t)zhLen < sizeof(zhUserDataDir)) {
					setenv("GENERALSX_USERDATA_DIR", zhUserDataDir, 1);
				}

				// Reserved sibling for vanilla Generals, not used by this
				// (Zero Hour) build -- created so the "Generals" folder's
				// layout matches Windows Documents from the start rather
				// than growing a second leaf only whenever that variant
				// eventually ships.
				char baseUserDataDir[400];
				int baseLen = snprintf(baseUserDataDir, sizeof(baseUserDataDir),
					"%s/Command and Conquer Generals Data", generalsRoot);
				if (baseLen > 0 && (size_t)baseLen < sizeof(baseUserDataDir)) {
					mkdir(generalsRoot, 0755);
					mkdir(baseUserDataDir, 0755);
				}
			}
		}
		if (cachePath != nullptr) {
			// DXVK shader cache: regenerable, belongs in the OS-purgeable dir.
			// Must be set before the d3d8 .so loads.
			setenv("DXVK_STATE_CACHE_PATH", cachePath, 0);
		}

		// Mirror stderr into a file the user can pull without adb root: Android
		// sends native stderr to /dev/null (only SDL_Log reaches logcat), and an
		// engine this chatty is undebuggable blind. Keep the previous session's
		// log — a session that ends in a low-memory kill leaves no crash report,
		// so the prior log is often the only evidence.
		if (externalPath != nullptr) {
			char logPath[1024], prevPath[1024];
			snprintf(logPath, sizeof(logPath), "%s/generals-stderr.log", externalPath);
			snprintf(prevPath, sizeof(prevPath), "%s/generals-stderr-prev.log", externalPath);
			rename(logPath, prevPath);
			if (freopen(logPath, "w", stderr) != nullptr) {
				setvbuf(stderr, nullptr, _IOLBF, 0);  // line-buffered: a crash still flushes recent lines
			}
		}

		// Game data: highest priority is a folder the user picked in-app via
		// the GeneralsZH Setup app's folder browser (no adb needed) — it
		// writes the chosen absolute path as plain text into
		// <internal>/gamedata_path.txt before this activity ever starts.
		// Falls back to <external>/GameData (the adb-push convention this
		// port originally documented), then the external files dir itself.
		bool didChdir = false;
		if (internalPath != nullptr) {
			char markerPath[1024];
			snprintf(markerPath, sizeof(markerPath), "%s/gamedata_path.txt", internalPath);
			FILE *marker = fopen(markerPath, "r");
			if (marker != nullptr) {
				char customPath[900] = {0};
				if (fgets(customPath, sizeof(customPath), marker) != nullptr) {
					size_t len = strlen(customPath);
					while (len > 0 && (customPath[len - 1] == '\n' || customPath[len - 1] == '\r')) {
						customPath[--len] = '\0';
					}
					if (len > 0) {
						if (chdir(customPath) == 0) {
							didChdir = true;
							fprintf(stderr, "INFO: Android working directory (Setup-selected): %s\n", customPath);
						} else {
							fprintf(stderr, "WARNING: Setup-selected game folder '%s' set but chdir failed: %s\n",
							        customPath, strerror(errno));
						}
					}
				}
				fclose(marker);
			}
		}
		if (!didChdir && externalPath != nullptr) {
			char gameData[1024];
			snprintf(gameData, sizeof(gameData), "%s/GameData", externalPath);
			if (access(gameData, R_OK) == 0 && chdir(gameData) == 0) {
				didChdir = true;
				fprintf(stderr, "INFO: Android working directory (GameData): %s\n", gameData);
			} else if (chdir(externalPath) == 0) {
				didChdir = true;
				fprintf(stderr, "INFO: Android working directory (external files): %s\n", externalPath);
			}
		}
		if (!didChdir) {
			fprintf(stderr, "WARNING: could not enter game data directory (external storage unavailable?)\n");
		}

		// GeneralsX @feature Android port 06/09/2026 Optional second folder:
		// where the BASE Generals archives live. Zero Hour is an expansion and
		// only ships what it added, so it needs the original game's Terrain.big,
		// Textures.big, W3D.big and friends alongside its own.
		//
		// The engine already knows how to find them -- loadBaseGeneralsAssetsForZH()
		// in StdBIGFileSystem.cpp -- but its automatic guesses are a sibling
		// "../Generals" directory and a nested "ZH_Generals" one, in that order,
		// and it stops at the first that yields anything. A repack that leaves a
		// sibling Generals folder full of duplicate *ZH.big archives therefore
		// swallows the search before the real base game is ever reached.
		//
		// Dropping the base archives in beside the ZH ones is NOT a fix: archive
		// precedence is load order, the list is alphabetical, and INI.big sorts
		// before INIZH.big -- so the base game wins every shared file and Zero
		// Hour turns into original Generals wearing the expansion's menus. That
		// first-one-wins rule is also what mods rely on, so it must not change.
		//
		// So point the engine straight at the folder instead. CNC_GENERALS_PATH
		// is the very first thing loadBaseGeneralsAssetsForZH() consults, ahead of
		// every guess, and setting it here costs the engine nothing. Same plumbing
		// as the game folder above: the Setup app writes a plain-text marker,
		// native code reads it before the engine starts.
		if (internalPath != nullptr) {
			char basePathMarker[1024];
			snprintf(basePathMarker, sizeof(basePathMarker), "%s/generals_base_path.txt", internalPath);
			FILE *baseMarker = fopen(basePathMarker, "r");
			if (baseMarker != nullptr) {
				char basePath[900] = {0};
				if (fgets(basePath, sizeof(basePath), baseMarker) != nullptr) {
					size_t len = strlen(basePath);
					while (len > 0 && (basePath[len - 1] == '\n' || basePath[len - 1] == '\r')) {
						basePath[--len] = '\0';
					}
					if (len > 0) {
						setenv("CNC_GENERALS_PATH", basePath, 1);
						fprintf(stderr, "INFO: Android base Generals folder (Setup-selected): %s\n", basePath);
					}
				}
				fclose(baseMarker);
			}
		}

		// GeneralsX @feature Android port 30/07/2026 Opt-in Vulkan validation
		// layer, same UX as gx_trace.txt: a tester drops a file named
		// dxvk_validation.txt into the game data folder (no adb, no rebuild)
		// and the next launch runs with VK_LAYER_KHRONOS_validation attached.
		// DXVK already supports this natively via DXVK_DEBUG=validation
		// (dxvk_instance.cpp) and its debug-callback output already goes to
		// stderr on non-Windows (log.cpp) -- i.e. straight into the
		// generals-stderr.log this file just set up above -- so the only
		// missing piece was getting the loader to find the layer at all:
		// libVkLayer_khronos_validation.so is bundled in jniLibs/arm64-v8a/,
		// which Android's Vulkan loader searches automatically for a
		// debuggable app. Checked relative to CWD, same as gx_trace.txt,
		// because that's only valid after the chdir above. Off by default:
		// the layer adds real per-call overhead and DXVK itself warns about
		// it, so this is for a specific repro, not every session.
		if (didChdir) {
			FILE *vvlMarker = fopen("dxvk_validation.txt", "r");
			if (vvlMarker != nullptr) {
				fclose(vvlMarker);
				setenv("DXVK_DEBUG", "validation", 1);
				fprintf(stderr, "INFO: dxvk_validation.txt found -- Vulkan validation layer requested (DXVK_DEBUG=validation)\n");
			}

			// GeneralsX @feature Android port 01/08/2026 Same opt-in UX again:
			// dxvk_verbose_log.txt bumps DXVK_LOG_LEVEL from this build's
			// default of "error" (set above) up to "info", which is what makes
			// DXVK's "Presenter: Actual swapchain properties" line (format,
			// present mode, image count, composite alpha) show up in
			// generals-stderr.log. That line is normally silent, and it's the
			// only way to see what compositeAlpha/present mode a specific
			// device's driver actually negotiated (e.g. tracking down a
			// device-specific screenshot/task-switcher-only rendering
			// artifact) without an adb-attached logcat. Overwrite=1 so this
			// marker always wins over the "error" default above, and the
			// overhead (a handful of one-line-per-swapchain-recreation logs)
			// is negligible compared to dxvk_validation.txt's, so it's safe
			// to leave enabled for a whole repro session.
			FILE *verboseMarker = fopen("dxvk_verbose_log.txt", "r");
			if (verboseMarker != nullptr) {
				fclose(verboseMarker);
				setenv("DXVK_LOG_LEVEL", "info", 1);
				fprintf(stderr, "INFO: dxvk_verbose_log.txt found -- DXVK_LOG_LEVEL=info\n");
			}

			// GeneralsX @feature Android port 30/07/2026 DXVK's HUD, same
			// opt-in UX as the validation marker above: drop dxvk_hud.txt into
			// the game data folder, no rebuild and no adb. There is no other
			// way to get frame timings off a retail device, and asset
			// extraction deliberately never overwrites a user's dxvk.conf, so
			// editing the shipped one would not reach existing installs.
			//
			// An empty file gets a safe default. Anything written inside is
			// passed through verbatim as the HUD element list, EXCEPT that
			// "frametimes" and "memory" are refused: those two draw through
			// their own graph pipelines (HudFrameTimeItem/HudMemoryDetailsItem),
			// which still chain VkPipelineRenderingCreateInfo unconditionally
			// and therefore crash on a device without VK_KHR_dynamic_rendering.
			// The text elements go through HudRenderer, which this port already
			// routes via the legacy render pass.
			FILE *hudMarker = fopen("dxvk_hud.txt", "r");
			if (hudMarker != nullptr) {
				char hudSpec[256] = { 0 };
				if (fgets(hudSpec, sizeof(hudSpec), hudMarker) == nullptr)
					hudSpec[0] = '\0';
				fclose(hudMarker);

				// Trim trailing newline/whitespace left by a text editor.
				for (size_t i = strlen(hudSpec); i > 0 && (unsigned char)hudSpec[i - 1] <= ' '; --i)
					hudSpec[i - 1] = '\0';

				if (hudSpec[0] == '\0')
					strcpy(hudSpec, "fps,drawcalls,submissions,pipelines");

				if (strstr(hudSpec, "frametimes") != nullptr || strstr(hudSpec, "memory") != nullptr) {
					fprintf(stderr, "WARNING: dxvk_hud.txt requests 'frametimes'/'memory', which crash on this "
						"device's Vulkan 1.1 driver -- falling back to the safe element list\n");
					strcpy(hudSpec, "fps,drawcalls,submissions,pipelines");
				}

				setenv("DXVK_HUD", hudSpec, 1);
				fprintf(stderr, "INFO: dxvk_hud.txt found -- DXVK HUD enabled (DXVK_HUD=%s)\n", hudSpec);
			}
		}

		// GeneralsX @feature Android port 13/07/2026 Game DATA language
		// override (separate from the launcher's own UI language, see
		// LocaleHelper.java): SetupActivity.applyGameLanguageOverride()
		// writes <internal>/game_language.cfg (one line: the engine's
		// lowercase language token, e.g. "german") only after confirming
		// data/<token>/generals.csf actually exists in the selected game
		// folder -- so this never forces a language the user doesn't
		// actually own retail/licensed data for. GetRegistryLanguage()
		// (registry.cpp) checks CNC_ZH_LANGUAGE before the registry.ini
		// file or BIG-file auto-detect, so this must be exported before
		// any engine subsystem reads it.
		if (internalPath != nullptr) {
			char langMarkerPath[1024];
			snprintf(langMarkerPath, sizeof(langMarkerPath), "%s/game_language.cfg", internalPath);
			FILE *langMarker = fopen(langMarkerPath, "r");
			if (langMarker != nullptr) {
				char lang[64] = {0};
				if (fgets(lang, sizeof(lang), langMarker) != nullptr) {
					size_t len = strlen(lang);
					while (len > 0 && (lang[len - 1] == '\n' || lang[len - 1] == '\r')) {
						lang[--len] = '\0';
					}
					if (len > 0) {
						// GeneralsX @bugfix Android port 09/09/2026 Set the TEXT language, not
						// the game's language.
						//
						// This used to export CNC_ZH_LANGUAGE, which is what GetRegistryLanguage()
						// answers -- and that answer is read by far more than the string table.
						// It picks the header templates, the font configuration, the command map,
						// and through them the menu ARTWORK. So selecting Russian did translate
						// the text, and also swapped Zero Hour's branding for the base Generals
						// one, because those are the assets the engine could still resolve.
						//
						// A language pack is text. It is not a different SKU, and it has no
						// business deciding which game's logo is on the menu. So it now sets a
						// variable only the string table reads, and everything else stays on the
						// language the installed game data actually is.
						setenv("GENERALSX_TEXT_LANGUAGE", lang, 1);
						fprintf(stderr, "INFO: Game TEXT language override: %s\n", lang);
					}
				}
				fclose(langMarker);
			}
		}

		// Seed default settings on first run (full detail instead of the 2003
		// GPU auto-detect, which drops unknown GPUs — "Adreno 830" included —
		// to Low LOD with quarter-res textures).
		if (internalPath != nullptr && access("DefaultOptions.ini", R_OK) == 0) {
			char userDataDir[1024], optionsPath[1024];
			snprintf(userDataDir, sizeof(userDataDir),
			         "%s/.local/share/GeneralsX/GeneralsZH", internalPath);
			snprintf(optionsPath, sizeof(optionsPath), "%s/Options.ini", userDataDir);
			if (access(optionsPath, F_OK) != 0) {
				std::error_code fsError;
				std::filesystem::create_directories(userDataDir, fsError);
				std::filesystem::copy_file("DefaultOptions.ini", optionsPath, fsError);
				if (!fsError) {
					fprintf(stderr, "INFO: Seeded default Options.ini\n");
				}
			}
		}
	}
#endif

	fprintf(stderr, "=================================================\n");
	fprintf(stderr, " Command & Conquer Generals: Zero Hour (Linux)\n");
	fprintf(stderr, " SDL3 + DXVK Build\n");
	// GeneralsX @feature Android port 12/07/2026 Print the CI run number
	// (matches the "runNNN" in the release APK's filename) so a crash log
	// or bug report unambiguously proves which build produced it -- we've
	// already misattributed a tester's log to the wrong build once because
	// there was no way to tell them apart.
	fprintf(stderr, " CI build number: %d\n", ANDROID_CI_BUILD_NUMBER);
	fprintf(stderr, "=================================================\n\n");

	try {
		// Initialize critical sections (required by game engine)
		TheAsciiStringCriticalSection = &critSec1;
		TheUnicodeStringCriticalSection = &critSec2;
		TheDmaCriticalSection = &critSec3;
		TheMemoryPoolCriticalSection = &critSec4;
		TheDebugLogCriticalSection = &critSec5;

		// Initialize memory manager early (required by NEW operator)
		initMemoryManager();

		// GeneralsX @bugfix BenderAI 14/02/2026 Initialize Version singleton
		// GameEngine::init() calls updateWindowTitle() which uses TheVersion
		// Must be created before GameMain() to avoid nullptr dereference
		TheVersion = NEW Version;

		// Parse command line (CommandLine class handles argc/argv internally)
		// TheSuperHackers @build felipebraz 10/02/2026 Phase 1.5
		// Store argc/argv for CommandLine parser to access via _NSGetArgc/_NSGetArgv or /proc/self/cmdline
		// For now, let CommandLine::parseCommandLineForStartup() handle this
		CommandLine::parseCommandLineForStartup();

		// GeneralsX @bugfix Copilot 17/05/2026 Skip SDL3 window bootstrap for CLI/headless replay execution.
		const bool isHeadlessMode = (TheGlobalData != nullptr && TheGlobalData->m_headless);
		if (isHeadlessMode) {
			fprintf(stderr, "INFO: Headless mode detected, skipping SDL3 video/Vulkan window initialization\n");
		} else {

		// GeneralsX @bugfix felipebraz 16/02/2026
		// Initialize SDL3 and Vulkan BEFORE creating GameEngine (fighter19 pattern)
		// This prevents LLVM SIGSEGV crash during Vulkan driver enumeration
		// Must be done here, not in SDL3GameEngine::init() which is too late
		fprintf(stderr, "INFO: Initializing SDL3 video subsystem...\n");
#if defined(SAGE_MOBILE_PLATFORM)
		// All mouse events are synthesized by the gesture translator in
		// SDL3GameEngine.cpp; SDL's automatic touch->mouse synthesis would
		// double-deliver finger 1 and fight the two-finger pan logic.
		SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
#endif
#if defined(__ANDROID__)
		// Keep running (paused by our own lifecycle gate) instead of blocking
		// inside SDL when the app loses focus; the render gate in
		// SDL3GameEngine::update() owns the pause so state stays consistent.
		SDL_SetHint(SDL_HINT_ANDROID_BLOCK_ON_PAUSE, "0");
#endif
		// GeneralsX @build Android port ANGLE experiment - useVulkan/useANGLE
		// only read an env var each, so it's safe to decide before SDL's video
		// subsystem (and therefore EGL) is initialized. SDL_HINT_EGL_LIBRARY
		// must be set before SDL_InitSubSystem(SDL_INIT_VIDEO) -- SDL loads EGL
		// while bringing up the video driver, not lazily at context-creation
		// time. libEGL_angle.so is bundled in jniLibs alongside libmain.so and
		// resolves via the app's normal dlopen search path -- no absolute path
		// needed, and it's named distinctly from libEGL.so so Android's linker
		// namespace (which reserves that name for the system library) never
		// gets a say. The matching gl* dispatch is loaded separately, in
		// WebGLPipeline::initContext(), once the context is current.
		const bool useVulkan = UseVulkanBackend();
#if defined(__ANDROID__)
		if (!useVulkan && UseANGLE()) {
			SDL_SetHint(SDL_HINT_EGL_LIBRARY, "libEGL_angle.so");

			// GeneralsX @bugfix Android port 08/30/2026 A Redmi Note 8 Pro
			// (Mali-G76) device log showed the whole-frame vertical-flip bug
			// that bd2a2379 (gles_pipeline.cpp) already fixed and verified
			// against the SYSTEM GLES driver reappearing specifically once
			// this device's GLES backend routed through ANGLE
			// ("[d3d8gles] GLES backend: libGLESv2_angle.so" in the log).
			// The same device's Vulkan/DXVK path has never shown this --
			// DXVK's swapchain creation hardcodes
			// VkSwapchainCreateInfoKHR::preTransform = IDENTITY
			// unconditionally (dxvk_presenter.cpp) and just lets
			// SurfaceFlinger do a compositor-side rotation blit if the
			// surface's actual currentTransform isn't identity -- always
			// correct, if marginally less efficient. ANGLE instead reads
			// the surface's real currentTransform and internally
			// "pre-rotates" its own rendering (vertex positions, viewport,
			// scissor, render-pass area -- see ANGLE's SurfaceVk.cpp,
			// enablePreRotateSurfaces, enabled by default on Android) to
			// avoid that compositor blit. 90/270-degree pre-rotation is the
			// common case on portrait-primary phones and well exercised;
			// 180 degrees (this device apparently reports a landscape
			// window rotated 180 from what most phones consider forward
			// landscape -- see the reverted 106cd23 for the same finding
			// via Android's orientation API) is a much rarer path through
			// the same ANGLE code and a very plausible place for a
			// device-specific bug to hide. Disabling this ANGLE feature
			// makes it fall back to declaring IDENTITY and relying on the
			// same compositor-blit behavior already confirmed correct via
			// DXVK on this exact device, instead of ANGLE's own internal
			// compensation math.
			setenv("ANGLE_FEATURE_OVERRIDES_DISABLED", "enablePreRotateSurfaces", 1);
		}
#endif
		// Quest port note: SDL_INIT_GAMEPAD was tried for per-button mapping, but
		// Quest controllers deliver zero button/axis events through SDL -- every
		// button arrives as SDL_SCANCODE_AC_BACK instead (see the KEY_DOWN case
		// in SDL3GameEngine.cpp). So no gamepad subsystem needed.
		if (!SDL_InitSubSystem(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
			fprintf(stderr, "FATAL: Failed to initialize SDL3: %s\n", SDL_GetError());
			return 1;
		}

		if (useVulkan) {
			// Set DXVK WSI driver before loading Vulkan
			setenv("DXVK_WSI_DRIVER", "SDL3", 1);

			// GeneralsX @bugfix BenderAI 06/03/2026 - Exclude LLVMpipe Vulkan ICD before loading Vulkan.
			// libvulkan_lvp.so crashes during static initialization with LLVM 20.x when the Vulkan
			// loader enumerates all ICDs. Restrict to hardware ICDs first.
#if !defined(__ANDROID__)
			// Desktop-Mesa workaround; Android has no ICD JSON directories — the
			// system Vulkan loader picks the vendor driver (Adreno/Mali) itself.
			FilterSoftwareVulkanICDs();
#endif
		}
		FilterPipeWireOpenAL();

#if defined(__ANDROID__)
		if (useVulkan) {
			// Must run before SDL_Vulkan_LoadLibrary()/DXVK's own internal
			// dlopen("libvulkan.so") below -- see TryLoadCustomVulkanDriver().
			const char *internalPath = SDL_GetAndroidInternalStoragePath();
			if (internalPath != nullptr) {
				TryLoadCustomVulkanDriver(internalPath);
			}
		}
#endif

		if (useVulkan) {
			// Load Vulkan library for DXVK DirectX8→Vulkan translation
			fprintf(stderr, "INFO: Loading Vulkan library...\n");
			if (!SDL_Vulkan_LoadLibrary(nullptr)) {
				fprintf(stderr, "WARNING: Failed to load Vulkan: %s\n", SDL_GetError());
				fprintf(stderr, "WARNING: Continuing without Vulkan (may use software rendering)\n");
			}
		} else {
			// GeneralsX @build Android port GLES experiment - GLES3 context
			// attributes must be set BEFORE SDL_CreateWindow (SDL only applies
			// them to windows created after this call); the actual GL context
			// itself is created later, once DX8Wrapper::Init() -> the d3d8gles
			// backend has a window handle (see WebGLPipeline::initContext).
			fprintf(stderr, "INFO: Using native GLES3 backend (no Vulkan)\n");
			SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
			SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
			SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
			SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
			SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
			SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
			// GeneralsX @bugfix Android port 09/04/2026 EXPERIMENT (low
			// confidence): was 0 (opaque, no alpha channel). Real-device
			// diagnostics ruled out every application-level cause (viewport
			// application, EGL surface size at creation time -- both
			// confirmed correct) for a persistent edge strip seen only on
			// GLES/ANGLE, never Vulkan, at any resolution including an exact
			// 1:1 match with no pillarboxing involved at all. Alpha=0 is a
			// comparatively rare, less-traveled EGL config on Android (almost
			// every GL app requests the standard RGBA8888 config), so it's
			// worth testing whether SurfaceFlinger/the GPU driver's edge/
			// display-cutout/rounded-corner composition handles that more
			// common configuration differently. Vulkan/DXVK's swapchain
			// likely declares VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR explicitly
			// (telling the compositor unambiguously to ignore alpha), which
			// EGL has no equivalent explicit knob for -- its blend behavior
			// is inferred from the chosen config's alpha bits instead.
			SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
			SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
			SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
			SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
		}

		// Create SDL3 window (Vulkan or OpenGL/GLES surface depending on the backend switch)
		fprintf(stderr, "INFO: Creating SDL3 %s window...\n", useVulkan ? "Vulkan" : "OpenGL ES");
		Uint32 windowFlags = (useVulkan ? SDL_WINDOW_VULKAN : SDL_WINDOW_OPENGL) | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN;  // Start hidden, show after D3D init
#if defined(SAGE_MOBILE_PLATFORM)
		// Request a native-resolution drawable (e.g. 2868x1320 instead of the
		// 956x440 point size). Without this the swapchain renders at point size and
		// the display upscales 3x, visibly blurring textures and terrain.
		windowFlags |= SDL_WINDOW_HIGH_PIXEL_DENSITY;
#endif
#if defined(__ANDROID__)
		// Fullscreen on Android == immersive mode: hides the status and
		// navigation bars so the RTS UI owns the whole panel.
		windowFlags |= SDL_WINDOW_FULLSCREEN;
#endif
		TheSDL3Window = SDL_CreateWindow(
			"Command & Conquer Generals: Zero Hour",
			1024, 768,  // Default resolution
			windowFlags
		);

		if (!TheSDL3Window) {
			fprintf(stderr, "FATAL: Failed to create SDL3 window: %s\n", SDL_GetError());
			SDL_Quit();
			return 1;
		}

		// Store window handle globally (cast SDL_Window* to HWND for compatibility)
		ApplicationHWnd = (HWND)TheSDL3Window;
		fprintf(stderr, "INFO: SDL3 window created successfully\n");

#if defined(SAGE_MOBILE_PLATFORM)
		// Match the game's internal resolution to the phone screen's aspect ratio.
		// Without this the engine runs its 4:3 default inside the 19.5:9 display:
		// pillarboxed picture and a skewed window->game coordinate mapping. Height
		// stays at the engine's 600px design baseline (UI layouts assume >= 600);
		// width follows the real aspect. Injected as -xres/-yres argv entries so
		// the normal command-line path applies them (user-passed flags still win
		// because the parser lets later arguments override earlier ones... ours go
		// last, so only add them if the user didn't pass explicit -xres/-yres).
		{
			bool userSetRes = false;
			for (int i = 1; i < __argc; ++i) {
				if (strcmp(__argv[i], "-xres") == 0 || strcmp(__argv[i], "-yres") == 0) {
					userSetRes = true;
					break;
				}
			}
#if defined(__ANDROID__)
			// GeneralsX @bugfix Android port 07/07/2026 Even with a plain (non-sensor)
			// "landscape" manifest lock, WindowManager can take a handful of frames to
			// actually apply it to a freshly created Activity's window — especially
			// across the singleInstance task switch from SetupActivity. A single
			// snapshot right after SDL_CreateWindow can still catch a stale portrait
			// size, which then bakes a wrong -xres/-yres for the whole session
			// (confirmed on a real device: the very first screen after Setup rendered
			// pillarboxed into a portrait window while a later screen in the same
			// session was already correctly landscape). Poll briefly for the window
			// to actually report landscape before trusting its size.
			//
			// GeneralsX @bugfix Android port 08/31/2026 The original version of this
			// loop broke as soon as the FIRST landscape-shaped (w>h) reading appeared,
			// which is not the same as the window having actually finished settling --
			// on a real device (Redmi Note 8 Pro) this baked xres/yres as 2264x1080,
			// while the window's true final size (once edge-to-edge/display-cutout
			// layout finished applying, a few frames later) was 2340x1080. That 76px
			// gap then became a permanent, if minor, pillarbox letterbox margin for
			// the whole session.
			//
			// GeneralsX @bugfix Android port 08/31/2026, take 2 A first attempt required
			// only TWO consecutive identical landscape readings (100ms of stability)
			// before trusting the size -- confirmed via a real-device log
			// (Pillarbox: game=2264x1080...) that this still wasn't long enough: the
			// window can apparently report a genuinely STABLE intermediate width for
			// well over 100ms before a later, asynchronous inset/cutout adjustment
			// (status/navigation bar animation, WindowInsetsAnimation, etc.) changes
			// it again. Widened to require FOUR consecutive matches (200ms stable) and
			// extended the budget to 60 attempts (up to 3s total, vs. 1s before) to
			// give that later adjustment more room to happen before this loop gives up
			// and trusts whatever it has. Still fundamentally a best-effort heuristic,
			// not a guarantee -- the robust fix would be reacting to a real
			// SDL_EVENT_WINDOW_RESIZED after startup instead of polling once here, but
			// that's a bigger change than this loop.
			int prevW = -1, prevH = -1;
			int stableCount = 0;
			for (int attempt = 0; attempt < 60; ++attempt) {
				int w = 0, h = 0;
				SDL_GetWindowSizeInPixels(TheSDL3Window, &w, &h);
				if (w > h && w == prevW && h == prevH) {
					if (++stableCount >= 4) break;
				} else {
					stableCount = 0;
				}
				prevW = w;
				prevH = h;
				SDL_PumpEvents();
				SDL_Delay(50);
			}
#endif
			// Use the pixel size of the high-density drawable: the game renders
			// 1:1 into the native-resolution swapchain, and fonts/UI rescale via
			// the engine's resolution-aware font scaling (GlobalLanguage).
			int winW = 0, winH = 0;
			SDL_GetWindowSizeInPixels(TheSDL3Window, &winW, &winH);
			if (!userSetRes && winW > 0 && winH > 0 && winW > winH) {
				static char xresVal[16], yresVal[16];
				static char xresFlag[] = "-xres";
				static char yresFlag[] = "-yres";
				// GeneralsX @bugfix Android port 08/07/2026 A prior version of this
				// code rendered at a REDUCED internal resolution and let the
				// pillarbox blit upscale to the full panel, expecting that to grow
				// buttons/controlbar geometry along with text. On real devices,
				// only text actually got bigger (GlobalLanguage's font-size scaling
				// is resolution-aware on its own); widget geometry did not change
				// size at all — the .wnd layout's own resolution-ratio math and the
				// pillarbox stretch cancel out, so shrinking the internal
				// resolution has no net visual effect on non-text UI. Reverted to
				// plain native-resolution rendering; only "Menu Text Size"
				// (ResolutionFontAdjustment, in the Settings app) actually changes
				// anything on screen.
				int yres = winH;
				int xres = winW;
				xres &= ~1;  // keep it even

				// GeneralsX @bugfix Android port 09/04/2026 This block injects
				// -xres/-yres as if the user passed them on the command line, and
				// CommandLine::parseCommandLineForEngineInit() (which runs AFTER
				// GameData.ini has already applied any saved Options.ini
				// "Resolution" preference into TheGlobalData) unconditionally
				// overwrites m_xResolution/m_yResolution with whatever -xres/-yres
				// say -- confirmed via real-device logs (Poco F8 Pro) that a
				// resolution picked in Options, confirmed written to Options.ini
				// with no I/O error, was silently discarded on the very next
				// launch because THIS code always re-injects the current window
				// size regardless. Options.ini itself is a plain "key = value"
				// per line format (UserPreferences::write()), so read it directly
				// here (the engine's own preference-loading machinery isn't up
				// yet at this point in startup) and prefer its saved Resolution
				// over the window-derived one when present and parseable --
				// falling back to the window size exactly as before otherwise
				// (first launch, or a corrupt/missing file).
				{
					const char *userDataDir = getenv("GENERALSX_USERDATA_DIR");
					if (userDataDir) {
						char optionsPath[512];
						snprintf(optionsPath, sizeof(optionsPath), "%s/Options.ini", userDataDir);
						FILE *fp = fopen(optionsPath, "r");
						if (fp) {
							char line[256];
							while (fgets(line, sizeof(line), fp)) {
								int savedX = 0, savedY = 0;
								if (sscanf(line, " Resolution = %d %d", &savedX, &savedY) == 2 &&
								    savedX > 0 && savedY > 0) {
									xres = savedX & ~1;
									yres = savedY;
									fprintf(stderr, "INFO: using saved Resolution %dx%d from Options.ini instead of window size %dx%d\n",
									        xres, yres, winW, winH);
									break;
								}
							}
							fclose(fp);
						}
					}
				}

				snprintf(xresVal, sizeof(xresVal), "%d", xres);
				snprintf(yresVal, sizeof(yresVal), "%d", yres);

				static char* newArgv[64];
				int n = 0;
				for (int i = 0; i < __argc && n < 59; ++i) {
					newArgv[n++] = __argv[i];
				}
				newArgv[n++] = xresFlag;
				newArgv[n++] = xresVal;
				newArgv[n++] = yresFlag;
				newArgv[n++] = yresVal;
				newArgv[n] = nullptr;
				__argv = newArgv;
				__argc = n;
				fprintf(stderr, "INFO: mobile internal resolution set to %sx%s (window %dx%d)\n",
				        xresVal, yresVal, winW, winH);
			}
		}
#endif
		}

		// Call cross-platform game entry point
		exitcode = GameMain();

		fprintf(stderr, "INFO: GameMain() returned with code %d\n", exitcode);

	} catch (const std::exception& e) {
		fprintf(stderr, "FATAL: Unhandled exception in main(): %s\n", e.what());
		exitcode = 1;
	} catch (...) {
		fprintf(stderr, "FATAL: Unknown exception in main()\n");
		exitcode = 1;
	}

	// Cleanup SDL3 resources
	if (TheSDL3Window) {
		SDL_DestroyWindow(TheSDL3Window);
		TheSDL3Window = nullptr;
		ApplicationHWnd = nullptr;
	}
	SDL_Quit();

	// GeneralsX @bugfix BenderAI 14/02/2026 Cleanup Version singleton
	if (TheVersion) {
		delete TheVersion;
		TheVersion = nullptr;
	}

	// GeneralsX @bugfix BenderAI 19/02/2026 Shutdown memory manager BEFORE nulling critical
	// sections. Without this, global pool destructors (ObjectPoolClass) crash during atexit()
	// because they call ::operator delete after the memory manager is already gone (SIGSEGV).
	// Matches WinMain.cpp cleanup order: TheVersion -> shutdownMemoryManager -> null critSecs.
	shutdownMemoryManager();

	// Cleanup critical sections (after memory manager, which may use them during shutdown)
	TheAsciiStringCriticalSection = nullptr;
	TheUnicodeStringCriticalSection = nullptr;
	TheDmaCriticalSection = nullptr;
	TheMemoryPoolCriticalSection = nullptr;
	TheDebugLogCriticalSection = nullptr;

	fprintf(stderr, "\nExiting with code %d\n", exitcode);

	// GeneralsX @bugfix BenderAI 25/02/2026 — use _exit() to skip C++ global destructors.
	// On macOS, __cxa_finalize_ranges runs ObjectPoolClass<X,256> global dtors after main() returns.
	// Those dtors crash with a corrupted BlockListHead (SIGSEGV at 0x4ade32ec4ade0018) because
	// pool block memory was already reused/overwritten during game shutdown.
	// Windows never had this problem — ExitProcess() terminates without running C++ global dtors.
	// _exit() matches that behavior. Explicit cleanup already done above (SDL_Quit, shutdownMemoryManager).
	_exit(exitcode);
}

#endif // !_WIN32
