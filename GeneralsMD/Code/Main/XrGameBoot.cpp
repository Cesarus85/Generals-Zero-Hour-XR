// GeneralsX @feature XR port Phase 1.0 - offscreen game boot, see
// XrGameBoot.h. Deliberately mirrors SDL3Main.cpp's Android boot sequence
// (env, working directory, Options seeding, critsecs, memory manager,
// Version, command line, engine init) minus everything window-shaped: no
// SDL video, no window, no resolution probing -- the render size is fixed
// (kXrGameWidth/Height) and GL comes from the XR loop's context.
#ifdef __ANDROID__

#include "XrGameBoot.h"
#include "XrCameraProfile.h"
#include "XrTactics.h"
#include "XrBoardMesh.h"

#include <android/log.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <filesystem>
#include <string>
#include <vector>

#include <SDL3/SDL.h>
#define SDL_MAIN_HANDLED
#include <SDL3/SDL_main.h>

#include "Lib/BaseType.h"
#include "Common/CommandLine.h"
#include "Common/CriticalSection.h"
#include "Common/FramePacer.h"
#include "Common/GameEngine.h"
#include "Common/GameMemory.h"
#include "Common/version.h"
#include "GameClient/Display.h"
#include "GameClient/Shell.h"
#include "GameClient/WindowLayout.h"
#include "GameClient/GameWindowManager.h"
#include "GameClient/GameWindow.h"
#include "Common/NameKeyGenerator.h"
#include "Common/GlobalData.h"
#include "GameClient/View.h"
#include "GameClient/InGameUI.h"
#include "SDL3Device/GameClient/TouchInput.h"
#include "W3DDevice/GameClient/W3DDisplay.h"
#include "W3DDevice/GameClient/W3DScene.h"
#include "W3DDevice/GameClient/BaseHeightMap.h"
#include "W3DDevice/GameClient/WorldHeightMap.h"
#include "W3DDevice/GameClient/W3DView.h"
#include "W3DDevice/GameClient/W3DInGameUI.h"
#include "camera.h"
#include "Common/Override.h"
#include "GameClient/ControlBar.h"
#include "GameClient/GadgetPushButton.h"
#include "GameClient/GameText.h"
#include "GameClient/GUICallbacks.h"
#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/FileSystem.h"
#include "GameLogic/Squad.h"
#include "GameClient/GameClient.h"
#include "GameClient/Drawable.h"
#include "GameClient/SelectionXlat.h"
#include "GameClient/SelectionInfo.h"
#include "Common/ThingTemplate.h"
#include "Common/Energy.h"
#include "GameClient/CommandXlat.h"
#include "GameLogic/Object.h"
#include "GameLogic/TerrainLogic.h"
#include "GameLogic/Module/BodyModule.h"
#include "coltest.h"
#include "GameClient/LookAtXlat.h"
#include "SDL3Device/GameClient/SDL3Mouse.h"
#include "SDL3Device/GameClient/SDL3Keyboard.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/FPUControl.h"
#include "SDL3GameEngine.h"
#include "GeneratedVersion.h"
#include "d3d8gles.h"

#define GX_BOOT_TAG "gx-xr-boot"

// Logging goes to BOTH logcat and stderr: stderr lands in the XR log file
// (once redirected below), logcat is visible live via adb during the
// ~minute-long boot with no other signs of life.
#define GXLOG(...) do { \
	__android_log_print(ANDROID_LOG_INFO, GX_BOOT_TAG, __VA_ARGS__); \
	fprintf(stderr, "[xr-boot] " __VA_ARGS__); \
	fprintf(stderr, "\n"); \
} while (0)
#define GXLOGE(...) do { \
	__android_log_print(ANDROID_LOG_ERROR, GX_BOOT_TAG, __VA_ARGS__); \
	fprintf(stderr, "[xr-boot] ERROR: " __VA_ARGS__); \
	fprintf(stderr, "\n"); \
} while (0)

// Defined here (declared in SDL3GameEngine.h): the XR boot sets it before
// CreateGameEngine() so init() skips the SDL window binding. The 2D port
// never sets it, so its path is untouched.
bool GX_XR_OffscreenBoot = false;

extern int __argc;
extern char **__argv;

static CriticalSection s_xrCritSec1;
static CriticalSection s_xrCritSec2;
static CriticalSection s_xrCritSec3;
static CriticalSection s_xrCritSec4;
static CriticalSection s_xrCritSec5;

static D3D8GLES_XRConfig s_xrConfig;
static bool s_booted = false;
static std::string s_layoutPath;
static std::string s_textLanguagePath;
static const char *s_languageNotice="XR sofort; Spieltexte nach Neustart (Sprachdateien nötig)";
static const char *s_groupNotice="";
static std::string s_legacyLayoutPath;
static bool s_splitEnabled=true;
static unsigned s_xrPresentationFrame=0;
static XrWorldFrame s_worldFrame;
bool GX_XR_SplitUIAllowed();
static std::string s_cameraPath;
static XrCameraProfile s_cameraFavorite;
static bool s_hasCameraFavorite=false;

// ---------------------------------------------------------------------------
// Storage paths via JNI (no SDLActivity here, so SDL_GetAndroid*Path is
// unusable -- ask the activity object directly).
// ---------------------------------------------------------------------------
static std::string callFileGetter(JNIEnv *env, jobject activity, const char *method, bool takesStringArg)
{
	std::string out;
	jclass cls = env->GetObjectClass(activity);
	if (cls == nullptr) {
		return out;
	}
	const char *sig = takesStringArg ? "(Ljava/lang/String;)Ljava/io/File;" : "()Ljava/io/File;";
	jmethodID mid = env->GetMethodID(cls, method, sig);
	jobject fileObj = nullptr;
	if (mid != nullptr) {
		fileObj = takesStringArg ? env->CallObjectMethod(activity, mid, nullptr)
		                          : env->CallObjectMethod(activity, mid);
	}
	if (fileObj != nullptr) {
		jclass fileCls = env->GetObjectClass(fileObj);
		jmethodID pathMid = fileCls ? env->GetMethodID(fileCls, "getAbsolutePath", "()Ljava/lang/String;") : nullptr;
		jstring pathStr = (pathMid && fileCls) ? (jstring)env->CallObjectMethod(fileObj, pathMid) : nullptr;
		if (pathStr != nullptr) {
			const char *utf = env->GetStringUTFChars(pathStr, nullptr);
			if (utf != nullptr) {
				out = utf;
				env->ReleaseStringUTFChars(pathStr, utf);
			}
			env->DeleteLocalRef(pathStr);
		}
		if (fileCls != nullptr) {
			env->DeleteLocalRef(fileCls);
		}
		env->DeleteLocalRef(fileObj);
	}
	if (env->ExceptionCheck()) {
		env->ExceptionClear();
	}
	env->DeleteLocalRef(cls);
	return out;
}

// Mirror of SetupActivity.isValidGameFolder(): a folder counts when it
// holds the expansion or base INI archive.
static bool isValidGameFolder(const char *dir)
{
	if (dir == nullptr || dir[0] == '\0') {
		return false;
	}
	char path[1024];
	snprintf(path, sizeof(path), "%s/INIZH.big", dir);
	if (access(path, R_OK) == 0) {
		return true;
	}
	snprintf(path, sizeof(path), "%s/INI.big", dir);
	return access(path, R_OK) == 0;
}

static bool readMarkerPath(const char *markerFile, char *outPath, size_t outLen)
{
	FILE *marker = fopen(markerFile, "r");
	if (marker == nullptr) {
		return false;
	}
	char buf[900] = {0};
	bool ok = false;
	if (fgets(buf, sizeof(buf), marker) != nullptr) {
		size_t len = strlen(buf);
		while (len > 0 && (buf[len - 1] == '\n' || buf[len - 1] == '\r')) {
			buf[--len] = '\0';
		}
		if (len > 0 && len < outLen) {
			memcpy(outPath, buf, len + 1);
			ok = true;
		}
	}
	fclose(marker);
	return ok;
}

static void mkdirParents(const char *path)
{
	std::error_code ec;
	std::filesystem::create_directories(path, ec);
}

bool XrGameBoot_Init(JNIEnv *env, jobject activity, void *eglDisplay, void *eglContext)
{
	GXLOG("XrGameBoot_Init: enter (CI build %d)", ANDROID_CI_BUILD_NUMBER);

	// -- Storage paths (this package's own dirs) -------------------------
	const std::string internalPath = callFileGetter(env, activity, "getFilesDir", false);
	const std::string externalPath = callFileGetter(env, activity, "getExternalFilesDir", true);
	const std::string cachePath = callFileGetter(env, activity, "getCacheDir", false);
	GXLOG("storage: internal=%s external=%s cache=%s",
	      internalPath.empty() ? "<none>" : internalPath.c_str(),
	      externalPath.empty() ? "<none>" : externalPath.c_str(),
	      cachePath.empty() ? "<none>" : cachePath.c_str());
	if (internalPath.empty()) {
		GXLOGE("no internal storage path -- cannot continue");
		return false;
	}
	s_legacyLayoutPath = internalPath + "/xr-layout-v1.cfg";
	s_layoutPath = internalPath + "/xr-layout-v2.cfg";
	// GeneralsX @feature Codex 14/09/2026 Honor the existing Setup text-only
	// override in XR too. Never change SKU/artwork/audio language.
	s_textLanguagePath=internalPath+"/game_language.cfg";
	if(FILE *language=fopen(s_textLanguagePath.c_str(),"r")) {
		char token[64]={};
		if(fscanf(language,"%63[a-z]",token)==1)setenv("GENERALSX_TEXT_LANGUAGE",token,1);
		fclose(language);
	}
	s_cameraPath = internalPath + "/xr-camera-v1.cfg";
	s_hasCameraFavorite=s_cameraFavorite.load(s_cameraPath.c_str());

	// -- Environment (same layout as the 2D port) -------------------------
	// HOME: registry.ini only. User data (saves, Options.ini, maps) lives
	// in the SHARED Generals folder, same formula as SDL3Main, so the XR
	// flavor and the 2D flavor share one set of settings/saves. Needs
	// MANAGE_EXTERNAL_STORAGE granted for this package (Setup grants it).
	setenv("HOME", internalPath.c_str(), 1);
	const int userId = (int)(getuid() / 100000);
	char generalsRoot[280], zhUserDataDir[400];
	snprintf(generalsRoot, sizeof(generalsRoot), "/storage/emulated/%d/Generals", userId);
	snprintf(zhUserDataDir, sizeof(zhUserDataDir), "%s/Command and Conquer Generals Zero Hour Data", generalsRoot);
	setenv("GENERALSX_USERDATA_DIR", zhUserDataDir, 1);
	GXLOG("user data dir: %s", zhUserDataDir);
	if (!cachePath.empty()) {
		setenv("DXVK_STATE_CACHE_PATH", cachePath.c_str(), 0);
	}

	// -- stderr to a pullable file (native stderr is /dev/null) ----------
	if (!externalPath.empty()) {
		char logPath[1024], prevPath[1024];
		snprintf(logPath, sizeof(logPath), "%s/generals-xr-stderr.log", externalPath.c_str());
		snprintf(prevPath, sizeof(prevPath), "%s/generals-xr-stderr-prev.log", externalPath.c_str());
		rename(logPath, prevPath);
		if (freopen(logPath, "w", stderr) != nullptr) {
			setvbuf(stderr, nullptr, _IOLBF, 0);
		}
		GXLOG("stderr -> %s", logPath);
	}

	// -- Working directory: the game data folder --------------------------
	// Primary source is the Setup-selected marker (XR package's own
	// internal storage -- SetupActivity writes it when the folder is
	// picked, exactly like the 2D flavor). The probe list below only
	// matters when the marker is missing (shouldn't happen: the activity
	// redirects to Setup first, but a loud fallback beats a dead boot).
	char dataDir[900] = {0};
	bool haveDir = false;
	{
		char markerPath[1024];
		snprintf(markerPath, sizeof(markerPath), "%s/gamedata_path.txt", internalPath.c_str());
		char marked[900] = {0};
		if (readMarkerPath(markerPath, marked, sizeof(marked))) {
			GXLOG("marker %s -> %s", markerPath, marked);
			if (isValidGameFolder(marked)) {
				snprintf(dataDir, sizeof(dataDir), "%s", marked);
				haveDir = true;
			} else {
				GXLOGE("marker path is not a valid game folder (no INIZH.big/INI.big)");
			}
		} else {
			GXLOG("no marker at %s", markerPath);
		}
	}
	if (!haveDir) {
		// Fallback probes: shared-storage spots a manual install would
		// use, the legacy external convention, and the 2D flavor's own
		// external dir (expected to fail with EACCES under scoped
		// storage -- the attempt is still logged so the log tells the
		// full story instead of just saying "not found").
		char externalGameData[1024] = {0};
		if (!externalPath.empty()) {
			snprintf(externalGameData, sizeof(externalGameData), "%s/GameData", externalPath.c_str());
		}
		const char *candidates[] = {
			"/storage/emulated/0/Download/GeneralsZH",
			"/sdcard/Download/GeneralsZH",
			"/storage/emulated/0/GeneralsZH",
			externalGameData[0] ? externalGameData : nullptr,
			externalPath.empty() ? nullptr : externalPath.c_str(),
			"/storage/emulated/0/Android/data/com.generalsx.zerohour/files/GameData",
			"/storage/emulated/0/Android/data/com.generalsx.zerohour/files",
			nullptr,
		};
		for (int i = 0; candidates[i] != nullptr; i++) {
			if (isValidGameFolder(candidates[i])) {
				snprintf(dataDir, sizeof(dataDir), "%s", candidates[i]);
				haveDir = true;
				GXLOG("probe hit: %s", candidates[i]);
				break;
			}
			GXLOG("probe miss: %s (errno=%d)", candidates[i], errno);
		}
	}
	if (!haveDir) {
		GXLOGE("no game data folder found -- pick one in Setup and relaunch");
		return false;
	}
	if (chdir(dataDir) != 0) {
		GXLOGE("chdir(%s) failed: %s", dataDir, strerror(errno));
		return false;
	}
	GXLOG("working directory: %s", dataDir);

	// Optional base-Generals folder (same marker convention as 2D).
	{
		char baseMarker[1024];
		snprintf(baseMarker, sizeof(baseMarker), "%s/generals_base_path.txt", internalPath.c_str());
		char basePath[900] = {0};
		if (readMarkerPath(baseMarker, basePath, sizeof(basePath))) {
			setenv("CNC_GENERALS_PATH", basePath, 1);
			GXLOG("base Generals folder: %s", basePath);
		}
	}

	// Seed default settings on first run (shared user-data dir, so this
	// normally already exists from the 2D flavor).
	{
		char optionsPath[1024];
		snprintf(optionsPath, sizeof(optionsPath), "%s/Options.ini", zhUserDataDir);
		if (access(optionsPath, F_OK) != 0 && access("DefaultOptions.ini", R_OK) == 0) {
			mkdirParents(zhUserDataDir);
			std::error_code ec;
			std::filesystem::copy_file("DefaultOptions.ini", optionsPath, ec);
			GXLOG("seeded default Options.ini (%s)", ec ? ec.message().c_str() : "ok");
		}
	}

	// -- SDL: events only (no video -- no window; no audio -- OpenAL) -----
	// pollSDL3Events()/SDL_GetTicks() need the events subsystem to be a
	// well-defined no-op source; video/audio would both require the
	// SDLActivity this flavor deliberately doesn't have.
	SDL_SetMainReady(); // JNI entry intentionally bypasses SDL_main.
	if (!SDL_Init(SDL_INIT_EVENTS)) {
		GXLOGE("SDL_Init(EVENTS) failed: %s", SDL_GetError());
		return false;
	} else {
		GXLOG("SDL events initialized");
	}

	// -- Engine prerequisites (mirror SDL3Main) ---------------------------
	TheAsciiStringCriticalSection = &s_xrCritSec1;
	TheUnicodeStringCriticalSection = &s_xrCritSec2;
	TheDmaCriticalSection = &s_xrCritSec3;
	TheMemoryPoolCriticalSection = &s_xrCritSec4;
	TheDebugLogCriticalSection = &s_xrCritSec5;
	initMemoryManager();
	TheVersion = NEW Version;

	static char s_arg0[] = "generals-xr";
	static char s_xresFlag[] = "-xres";
	static char s_yresFlag[] = "-yres";
	static char s_xresVal[16], s_yresVal[16];
	snprintf(s_xresVal, sizeof(s_xresVal), "%d", kXrGameWidth);
	snprintf(s_yresVal, sizeof(s_yresVal), "%d", kXrGameHeight);
	static char *s_argv[] = { s_arg0, s_xresFlag, s_xresVal, s_yresFlag, s_yresVal, nullptr };
	__argc = 5;
	__argv = s_argv;
	CommandLine::parseCommandLineForStartup();
	GXLOG("command line: -xres %s -yres %s", s_xresVal, s_yresVal);

	// -- Offscreen mode on, then boot the engine --------------------------
	GX_XR_OffscreenBoot = true;
	s_xrConfig.eglDisplay = eglDisplay;
	s_xrConfig.eglContext = eglContext;
	d3d8gles_SetXRConfig(&s_xrConfig);

	// NOTE: no enableFramesPerSecondLimit(TRUE) here (GameMain does it for
	// the 2D loop): the XR compositor paces via xrWaitFrame, and a sleep
	// inside executeSingleFrame would stall the XR frame loop.
	TheFramePacer = new FramePacer();
	TheGameEngine = CreateGameEngine();
	GXLOG("engine init starting (this takes ~a minute)...");
	try {
		TheGameEngine->init();
	} catch (const std::exception &e) {
		GXLOGE("engine init threw std::exception: %s", e.what());
		return false;
	} catch (...) {
		GXLOGE("engine init threw (non-std exception, see xr log for [GX-RELEASECRASH] lines)");
		return false;
	}
	if (TheDisplay != nullptr) {
		GXLOG("engine init complete: display %dx%d", TheDisplay->getWidth(), TheDisplay->getHeight());
	} else {
		GXLOGE("engine init complete but TheDisplay is null");
		return false;
	}
	s_booted = true;
	return true;
}

// GeneralsX @bugfix Codex 14/09/2026 Device display yields presentation only.
static void (*s_loadingPresenter)(void *)=nullptr;
static void *s_loadingContext=nullptr;
void XrGameBoot_SetLoadingPresenter(void (*present)(void *),void *context) {
	s_loadingPresenter=present;s_loadingContext=context;
}
void GX_XR_PresentLoadingFrame() {
	if(GX_XR_OffscreenBoot && s_loadingPresenter)s_loadingPresenter(s_loadingContext);
}

Bool XrGameBoot_Frame()
{
	if (!s_booted || TheGameEngine == nullptr) {
		return FALSE;
	}
	try {
		d3d8gles_BeginXRFrame(GX_XR_SplitUIAllowed(),s_worldFrame.enabled && s_worldFrame.elideWorldCopy);
		// P8 preserve non-XR detail choices outside this frame.
		extern bool GX_XR_WorldRequested();
		struct ShadowScope {
			GlobalData *data;Bool volumes,decals,markers;
			~ShadowScope(){if(TheGlobalData==data) {data->m_useShadowVolumes=volumes;data->m_useShadowDecals=decals;data->m_enableBehindBuildingMarkers=markers;}}
		} shadows{TheWritableGlobalData,TheGlobalData->m_useShadowVolumes,TheGlobalData->m_useShadowDecals,TheGlobalData->m_enableBehindBuildingMarkers};
		// GeneralsX @performance Codex 14/09/2026 Keep resources for instant A/B,
		// but skip volume update/render in B. Existing decal shadows are retained;
		// this does not invent replacement shadows for volume-only templates.
		if(GX_XR_WorldRequested()) {TheWritableGlobalData->m_useShadowVolumes=s_worldFrame.volumeShadows;TheWritableGlobalData->m_useShadowDecals=TRUE;TheWritableGlobalData->m_enableBehindBuildingMarkers=FALSE;}
		const Bool running=TheGameEngine->executeSingleFrame();
		++s_xrPresentationFrame;
		return running;
	} catch (const std::exception &e) {
		GXLOGE("frame threw std::exception: %s", e.what());
		return FALSE;
	} catch (...) {
		GXLOGE("frame threw (non-std exception)");
		return FALSE;
	}
}

const char *XrGameBoot_LayoutPath() { return s_layoutPath.c_str(); }
const char *XrGameBoot_LegacyLayoutPath() { return s_legacyLayoutPath.c_str(); }

// GeneralsX @bugfix Codex 14/09/2026 Native quit/options are not necessarily
// modal windows. They need the entire UI canvas, not just the command band.
bool XrGameBoot_ExpandedUI() {
	if(!GX_XR_OffscreenBoot || !XrGameBoot_IsInteractiveGame())return false;
	if(TheControlBar && TheControlBar->isPurchaseScienceVisible())return true;
	if(TheInGameUI && TheInGameUI->isQuitMenuVisible())return true;
	if(TheWindowManager && TheWindowManager->hasModalWindow())return true;
	if(IsDiplomacyVisible())return true;
	auto *options=TheShell ? TheShell->getOptionsLayout(FALSE):nullptr;
	return options && !options->isHidden();
}
// GeneralsX @bugfix Codex 14/09/2026 The native user-camera API cancels
// scripted movement even when its frame-based lock has expired. Never apply
// the automatic tabletop favorite while a movie/letterbox/path is active.
const char *XrGameBoot_CinematicBlocker() {
	if(!TheGlobalData || !TheDisplay)return "Darstellung noch nicht bereit";
	if(TheGlobalData->m_loadScreenRender || TheGlobalData->m_disableRender)return "Ladevorgang";
	if(TheGlobalData->m_playIntro || TheGlobalData->m_afterIntro)return "Intro";
	if(TheDisplay->isMoviePlaying() || (TheInGameUI && TheInGameUI->videoBuffer()))return "Video";
	if(TheDisplay->isLetterBoxed() || (TheTacticalView && !TheTacticalView->isCameraMovementFinished()))return "Kamerasequenz";
	return "";
}
const char *XrGameBoot_PresentationBlocker() {
	if(!GX_XR_OffscreenBoot || !XrGameBoot_IsInteractiveGame())return "Kein laufendes Spiel";
	if(!s_splitEnabled)return "Normales Fenster gewählt";
	const auto *cinematic=XrGameBoot_CinematicBlocker();if(*cinematic)return cinematic;
	// hideShell retains its stack. Its active flag, not stack size, owns input.
	if(TheShell && TheShell->isShellActive())return "Spielmenü";
	return "";
}
// Dialogs retain the world capture; their full UI canvas owns gameplay input.
bool GX_XR_SplitUIAllowed() {
	return !*XrGameBoot_PresentationBlocker();
}
bool XrGameBoot_CanControlCamera() {
	return XrGameBoot_IsInteractiveGame() && !*XrGameBoot_CinematicBlocker() &&
		!XrGameBoot_ExpandedUI() && TheTacticalView && !TheTacticalView->isUserControlLocked() &&
		(!TheShell || !TheShell->isShellActive());
}
std::string XrGameBoot_PresentationStatus(bool stereoVisible,bool requested) {
	char text[256];
	if(stereoVisible)snprintf(text,sizeof(text),"Stereo %dx%d · P18 %s · %s",s_worldFrame.width,s_worldFrame.height,d3d8gles_XRStereoMultiview() ? "Multiview":d3d8gles_XRStereoAtlas() ? "Atlas":"Ref",xrTr(d3d8gles_XROrdinarySkipped() ? "Zusatzwelt aus":"Zusatzwelt aktiv"));
	else {
		const auto *why=XrGameBoot_PresentationBlocker();
		if(!*why)why=requested ? "Tabletop wird vorbereitet":"Tabletop ausgeschaltet";
		snprintf(text,sizeof(text),"Flat %dx%d · %s",kXrGameWidth,kXrGameHeight,xrTr(why));
	}
	return text;
}
bool GX_XR_BeginUILayer() { return GX_XR_SplitUIAllowed() && d3d8gles_BeginXRUI(s_worldFrame.elideWorldCopy); }
// GeneralsX @feature Codex 13/09/2026 Read-only graphics prototype for offline games.
bool XrGameBoot_CanStereoWorld() {
	if(!TheGameLogic || !XrGameBoot_IsInteractiveGame()) return false;
	const auto mode=TheGameLogic->getGameMode();return mode==GAME_SKIRMISH || mode==GAME_SINGLE_PLAYER;
}
const char *XrGameBoot_PerformanceScene() {
	if(!TheGameLogic)return "shell";
	const auto mode=TheGameLogic->getGameMode();
	return mode==GAME_SINGLE_PLAYER ? "campaign":mode==GAME_SKIRMISH ? "skirmish":"other";
}
bool XrGameBoot_CanAdjustWorld() {
	return XrGameBoot_CanStereoWorld() && GX_XR_SplitUIAllowed() && XrGameBoot_CanControlCamera();
}
static float s_worldMapping[16]={};
static float s_worldAspect=0;
static float s_worldMaxHeight=0;
static bool s_mappingReady=false,s_spatialActive=false,s_ignoreSpatial=false;
static Vector3 s_pickStart,s_pickEnd,s_activeStart,s_activeEnd;
static ICoord2D s_pickPixel={},s_activePixel={};
static XrPosef s_pickAim={},s_activeAim={};
static XrVector3f s_pickRoom={},s_activeRoom={};
static Vector3 s_triggerRayStart,s_triggerRayEnd;
static ICoord2D s_triggerPixel={};
static XrRayDrag s_rayDrag;
static XrTactics s_tactics;
// GeneralsX @feature Codex 14/09/2026 XR bookmarks use native ViewLocation,
// never room poses. They expire on leaving/restarting/loading a match.
static ViewLocation s_bookmarks[4];
static bool s_bookmarkKnown[4]={};
static unsigned s_bookmarkFrame=0;
static XrTriggerGesture s_triggerGesture;
static XrVector3f s_triggerStart;
static bool s_triggerPreview=false;
static unsigned s_pickCounts[6]={}; // unavailable, outside board, terrain miss, projection, viewport, hit
static bool s_renderReady=false;
static CameraClass *s_renderCamera=nullptr;
static float s_worldSpan=619;
bool GX_XR_WorldRequested() {return s_worldFrame.enabled && XrGameBoot_CanStereoWorld() && GX_XR_SplitUIAllowed();}
int GX_XR_ShadowCategory(int category) {return d3d8gles_SetDrawCategory(category);}
CameraClass *GX_XR_RenderCamera() {return s_renderReady ? s_renderCamera:nullptr;}
int GX_XR_CullSphere(const SphereClass &sphere) {
	if(!s_renderReady) return -1;
	return xrBoardContainsSphere(s_worldMapping,s_worldAspect,{sphere.Center.X,sphere.Center.Y,sphere.Center.Z},sphere.Radius) ? 0:1;
}
// Conservative table coverage, independent of either eye. Quantize allocation
// to terrain tile blocks rather than reallocating on each tiny zoom change.
bool GX_XR_UpdateTerrainCoverage() {
	if(!GX_XR_WorldRequested() || !s_mappingReady || !TheTerrainRenderObject || !TheTerrainRenderObject->getMap()) return false;
	const auto center=xrInversePoint(s_worldMapping,{});
	const float extent=s_worldSpan*(fabsf(s_worldMapping[0])+fabsf(s_worldMapping[1]))*s_worldSpan;
	const int cells=std::clamp(1+64*int(ceilf((extent+160)/640)),129,513);
	TheTerrainRenderObject->setTerrainDrawSize(cells,cells);
	CameraClass coverage;Matrix3D pose(1);
	pose.Set_Translation(Vector3(center.x,center.y,center.z+s_worldSpan));
	coverage.Set_Transform(pose);coverage.Set_View_Plane(Vector2(-.6f,-.6f),Vector2(.6f,.6f));
	coverage.Set_Clip_Planes(1,s_worldSpan+1000);
	const Vector3 pivot(center.x,center.y,center.z);
	auto *lights=W3DDisplay::m_3DScene->createLightsIterator();
	TheTerrainRenderObject->updateCenter(&coverage,&pivot,lights);
	W3DDisplay::m_3DScene->destroyLightsIterator(lights);return true;
}
static void xrUpdateBookmarkSession(bool active,unsigned gameFrame) {
	if(!active || gameFrame<s_bookmarkFrame) {
		for(auto &known:s_bookmarkKnown)known=false;
		s_tactics.cancel();s_groupNotice="";
	}
	s_bookmarkFrame=gameFrame;
}
void XrGameBoot_SetWorldFrame(const XrWorldFrame &frame) {
	if(s_worldFrame.enabled && !frame.enabled) s_tactics.cancel();
	if(!XrGameBoot_IsInteractiveGame()) s_tactics=XrTactics{};
	xrUpdateBookmarkSession(XrGameBoot_CanStereoWorld(),TheGameLogic ? TheGameLogic->getFrame():0);
	s_worldFrame=frame;
	if(!frame.enabled) {s_mappingReady=false;XrGameBoot_SpatialPointer(false);}
}
unsigned int XrGameBoot_StereoTexture(int eye) {return d3d8gles_XRStereoTexture(eye);}
bool XrGameBoot_StereoMultiview() {return d3d8gles_XRStereoMultiview();}
void XrGameBoot_ConfigureMultiview(void *(*resolver)(const char *)) {d3d8gles_ConfigureXRMultiview(resolver);}
bool XrGameBoot_StereoAtlas() {return d3d8gles_XRStereoAtlas();}
// GeneralsX @bugfix Codex 14/09/2026 P18 engine adapter, also compiled by
// the height bridge regression. No terrain picks or camera mutation here.
static bool xrPrepareWorldMapping() {
	s_mappingReady=false;
	if(!s_worldFrame.enabled || !XrGameBoot_CanStereoWorld() || !GX_XR_SplitUIAllowed() || !TheTacticalView || !TheTerrainLogic) return false;
	const int w=TheTacticalView->getWidth(),h=TheTacticalView->getHeight();
	if(w<2 || h<2) return false;
	// GeneralsX @bugfix Codex 14/09/2026 P18 map-wide height bounds are
	// computed by the native loader. They survive panning, zoom, boundary
	// expansion and movies, and are replaced on a map/load transition.
	Region3D extent;TheTerrainLogic->getExtent(&extent);
	if(!std::isfinite(extent.lo.z+extent.hi.z) || extent.hi.z<extent.lo.z) return false;
	static_assert(255*MAP_HEIGHT_SCALE==GX_XR_TERRAIN_MAX_Z,"XR height envelope matches native heightmap bytes");
	const auto &pos=TheTacticalView->getPosition();const Coord3D center={pos.x,pos.y,extent.lo.z};
	const float angle=TheTacticalView->getAngle();const XrVector3f axis={cosf(angle),sinf(angle),0};
	float board[16];
	s_worldSpan=xrStableWorldSpan(TheTacticalView->getHeightAboveGround(),TheTacticalView->getFieldOfView(),
		ViewDefaultPitchRadians,s_worldFrame.coverage);
	if(!xrWorldToBoard(board,{center.x,center.y,center.z},axis,s_worldSpan)) return false;
	memcpy(s_worldMapping,board,sizeof(board));s_worldAspect=float(h)/w;s_worldMaxHeight=extent.hi.z;
	s_mappingReady=true;return true;
}
void GX_XR_BeginStereoWorld() {
	s_renderReady=false;
	if(!xrPrepareWorldMapping()) return;
	float clip[2][16];const float *board=s_worldMapping;
	const int w=TheTacticalView->getWidth(),h=TheTacticalView->getHeight();
	const auto center=xrInversePoint(s_worldMapping,{});
	GX_XR_UpdateTerrainCoverage();
	// Render billboards from the head midpoint. Input/UI retain the tactical
	// camera. Each eye still gets its own projection, depth and stencil.
	float room[16],physical[16];surfaceMatrix(s_worldFrame.board,physical);
	for(int i=8;i<11;++i) physical[i]*=s_worldFrame.board.width;
	matMultiply(room,physical,board);
	const auto head=xrScale(xrAdd(s_worldFrame.eyes[0].position,s_worldFrame.eyes[1].position),.5f);
	const auto position=xrInversePoint(room,head);
	float camera[16]={};camera[15]=1;
	for(int col=0;col<3;++col) {
		XrVector3f basis={col==0 ? 1.0f:0,col==1 ? 1.0f:0,col==2 ? 1.0f:0};
		const auto endpoint=xrInversePoint(room,xrAdd(head,xrRotate(s_worldFrame.eyes[0].orientation,basis)));
		const auto d=xrSub(endpoint,position);const auto unit=xrScale(d,1/xrLength(d));
		camera[col*4]=unit.x;camera[col*4+1]=unit.y;camera[col*4+2]=unit.z;
	}
	camera[12]=position.x;camera[13]=position.y;camera[14]=position.z;
	if(!s_renderCamera) s_renderCamera=new CameraClass;
	Matrix3D pose(1);for(int row=0;row<3;++row) for(int col=0;col<4;++col) pose[row][col]=camera[col*4+row];
	s_renderCamera->Set_Transform(pose);
	const auto &f=s_worldFrame.fov[0],&g=s_worldFrame.fov[1];
	s_renderCamera->Set_View_Plane(Vector2(std::min(tanf(f.angleLeft),tanf(g.angleLeft))-.05f,std::min(tanf(f.angleDown),tanf(g.angleDown))-.05f),
		Vector2(std::max(tanf(f.angleRight),tanf(g.angleRight))+.05f,std::max(tanf(f.angleUp),tanf(g.angleUp))+.05f));
	s_renderCamera->Set_Clip_Planes(1,20000);
	static unsigned mappingFrames=0;
	if((mappingFrames++%180)==0) {
		GXLOG("P18 stable height datum=%.3f map-max=%.3f span=%.3f ceiling=%.3f plinth=-0.036 board-widths",
			center.z,s_worldMaxHeight,s_worldSpan,gxXrBoardCeiling(s_worldMapping));
		GXLOG("P7.4 mapping center=(%.1f,%.1f,%.1f) span=%.1f viewport=%dx%d board=%.2fm eye=%dx%d",
			center.x,center.y,center.z,s_worldSpan,w,h,s_worldFrame.board.width,s_worldFrame.width,s_worldFrame.height);
		GXLOG("P7.4 pointer picks unavailable=%u outside=%u terrain-miss=%u projection=%u viewport=%u hit=%u",
			s_pickCounts[0],s_pickCounts[1],s_pickCounts[2],s_pickCounts[3],s_pickCounts[4],s_pickCounts[5]);
		memset(s_pickCounts,0,sizeof(s_pickCounts));
		// Read-only engine collision probe. Unlike host spies this exercises
		// the actual map, W3D terrain/model ray casting and screen projection.
		const auto &surface=s_worldFrame.board;
		const auto oldStart=s_pickStart,oldEnd=s_pickEnd;const auto oldPixel=s_pickPixel;
		const auto oldAim=s_pickAim;const auto oldRoom=s_pickRoom;
		for(float height:{1.0f,.06f}) {
			const XrPosef probe={surface.pose.orientation,xrAdd(surface.pose.position,
				xrRotate(surface.pose.orientation,{0,0,surface.width*height}))};
			XrWorldHit hit;
			const bool ok=XrGameBoot_PickWorld(surface,probe,hit);
			GXLOG("P7.4 center collision probe height=%.2f result=%s pixel=(%.0f,%.0f)",height,ok ? "HIT":"MISS",hit.x,hit.y);
		}
		s_pickStart=oldStart;s_pickEnd=oldEnd;s_pickPixel=oldPixel;
		s_pickAim=oldAim;s_pickRoom=oldRoom;
	}
	for(int eye=0;eye<2;++eye) xrWorldEyeClip(clip[eye],s_worldFrame,eye,board);
	s_renderReady=d3d8gles_BeginXRStereo(s_worldFrame.width,s_worldFrame.height,clip[0],clip[1],board,float(h)/w,camera,s_worldFrame.atlasStereo,s_worldFrame.multiviewStereo);
}
static void drawXrWorldDecorations();
void GX_XR_EndStereoWorld() {
	if(GX_XR_OffscreenBoot) {d3d8gles_EndXRStereo();if(s_renderReady) drawXrWorldDecorations();}
	s_renderReady=false;
}

// GeneralsX @feature Codex 13/09/2026 Replace only the active pointer's
// pick ray, not camera calibration or other world/screen conversions.
bool GX_XR_PointerRay(const ICoord2D *screen,Vector3 *start,Vector3 *end) {
	if(!s_spatialActive || s_ignoreSpatial || !screen || !GX_XR_SplitUIAllowed() ||
		!TheMouse || TheMouse->getPointerTarget()!=Mouse::PointerTarget::WORLD ||
		abs(screen->x-s_activePixel.x)>1 || abs(screen->y-s_activePixel.y)>1) return false;
	*start=s_activeStart;*end=s_activeEnd;return true;
}
bool XrGameBoot_PickWorld(const XrSurface &board,const XrPosef &aim,XrWorldHit &hit) {
	const auto result=[](int status) {++s_pickCounts[status];return status==5;};
	if(!s_mappingReady || !XrGameBoot_CanStereoWorld() || !GX_XR_SplitUIAllowed() ||
		!TheTerrainRenderObject || !W3DDisplay::m_3DScene) return result(0);
	setFPMode();
	XrVector3f start,end;if(!xrWorldRay(board,s_worldAspect,s_worldMapping,aim,start,end)) return result(1);
	const Vector3 a(start.x,start.y,start.z),b(end.x,end.y,end.z);
	LineSegClass line;line.Set(a,b);CastResultStruct groundResult;groundResult.ComputeContactPoint=true;
	RayCollisionTestClass ground(line,&groundResult);
	const bool groundHit=TheTerrainRenderObject->Cast_Ray(ground);
	Vector3 target=groundHit ? groundResult.ContactPoint:b;
	// Scene castRay publishes its closest hit in Ray.P1, NOT Result.Fraction.
	LineSegClass modelLine;modelLine.Set(a,target);CastResultStruct modelResult;
	RayCollisionTestClass model(modelLine,&modelResult,COLL_TYPE_ALL,false,false);
	const bool modelHit=W3DDisplay::m_3DScene->castRay(model,false,PICK_TYPE_ALL_DRAWABLES);
	if(modelHit) target=model.Ray.Get_P1();
	if(!groundHit && !modelHit)return result(2);
	const Coord3D world={target.X,target.Y,target.Z};ICoord2D pixel={640,288};
	TheTacticalView->worldToScreen(&world,&pixel);
	int ox=0,oy=0;TheTacticalView->getOrigin(&ox,&oy);
	// The legacy pixel is only a token: the spatial ray owns the actual hit.
	// Expanded table targets need not be inside the old camera rectangle.
	pixel.x=std::clamp(pixel.x,ox+1,ox+TheTacticalView->getWidth()-2);
	pixel.y=std::clamp(pixel.y,oy+1,oy+TheTacticalView->getHeight()-2);
	const auto local=xrTransformPoint(s_worldMapping,{world.x,world.y,world.z});
	hit.room=xrAdd(board.pose.position,xrRotate(board.pose.orientation,xrScale(local,board.width)));
	hit.distance=xrLength(xrSub(hit.room,aim.position));hit.x=float(pixel.x);hit.y=float(pixel.y);
	s_pickStart=a;s_pickEnd=b;s_pickPixel=pixel;s_pickAim=aim;s_pickRoom=hit.room;return result(5);
}
// GeneralsX @feature Codex 14/09/2026 Adjust the actual preview, not a
// second model. The original click translator sends this same angle.
bool XrGameBoot_CanRotatePlacement() {
	return XrGameBoot_CanAdjustWorld() && TheInGameUI &&
		static_cast<W3DInGameUI *>(TheInGameUI)->rotateXrPlacement(0);
}
bool XrGameBoot_RotatePlacement(float radians) {
	return s_spatialActive && XrGameBoot_CanRotatePlacement() &&
		static_cast<W3DInGameUI *>(TheInGameUI)->rotateXrPlacement(radians);
}
float XrGameBoot_PlacementDegrees() {
	if(!XrGameBoot_CanRotatePlacement())return 0;
	const float degrees=TheInGameUI->getPlacementAngle()*57.29577951f;
	return degrees<0 ? degrees+360:degrees;
}
void XrGameBoot_SpatialPointer(bool active) {
	s_spatialActive=active && s_mappingReady && XrGameBoot_CanStereoWorld() && GX_XR_SplitUIAllowed();
	if(!TheInGameUI) return;
	if(s_spatialActive) {
		s_activeStart=s_pickStart;s_activeEnd=s_pickEnd;s_activePixel=s_pickPixel;
		s_activeAim=s_pickAim;s_activeRoom=s_pickRoom;
		TheInGameUI->setTouchAimPoint(s_activePixel.x,s_activePixel.y,TRUE);
		if(TouchInput::hasArmedCommand()) TouchInput::beginAiming(s_activePixel.x,s_activePixel.y);
	} else if(GX_XR_OffscreenBoot) TheInGameUI->clearTouchAimPoint();
}
// GeneralsX @feature Codex 14/09/2026 Same guard messages and relationship
// policy as native GUI guard; hidden/dead/selected objects are not escorts.
static bool xrIssueGuard(Drawable *picked,const Coord3D &ground,bool onTerrain,bool hold) {
	Object *target=picked ? picked->getObject():nullptr;
	auto *player=ThePlayerList ? ThePlayerList->getLocalPlayer():nullptr;
	if(!hold && target) {
		if(!player || picked->getFullyObscuredByShroud() || picked->isSelected() ||
			target->isEffectivelyDead() || target->isContained() || target->isOffMap() ||
			player->getRelationship(target->getTeam())!=ALLIES) {
			s_groupNotice="Ungültiges Ziel: Boden oder anderes verbündetes Objekt";return false;
		}
		auto *m=TheMessageStream->appendMessage(GameMessage::MSG_DO_GUARD_OBJECT);
		m->appendObjectIDArgument(target->getID());m->appendIntegerArgument(GUARDMODE_NORMAL);
		pickAndPlayUnitVoiceResponse(TheInGameUI->getAllSelectedDrawables(),GameMessage::MSG_DO_GUARD_OBJECT);
	} else {
		if(!onTerrain){s_groupNotice="Keine Bodenposition getroffen";return false;}
		auto *m=TheMessageStream->appendMessage(GameMessage::MSG_DO_GUARD_POSITION);
		m->appendLocationArgument(ground);m->appendIntegerArgument(hold ? GUARDMODE_GUARD_WITHOUT_PURSUIT:GUARDMODE_NORMAL);
		pickAndPlayUnitVoiceResponse(TheInGameUI->getAllSelectedDrawables(),GameMessage::MSG_DO_GUARD_POSITION);
	}
	s_groupNotice="";return true;
}
static void xrIssueMovement(XrOrderMode mode,const Coord3D &ground,bool queue) {
	const bool oldQueue=TheInGameUI->isInWaypointMode(),oldForce=TheInGameUI->isInForceAttackMode();
	const bool oldMove=TheInGameUI->isInForceMoveToMode(),oldAttack=TheInGameUI->isInAttackMoveToMode();
	TheInGameUI->setWaypointMode(queue && (mode==XrOrderMode::Move || mode==XrOrderMode::Context));
	TheInGameUI->setForceAttackMode(mode==XrOrderMode::ForceAttack);
	TheInGameUI->setForceMoveMode(mode==XrOrderMode::ForceMove);
	TheInGameUI->clearAttackMoveToMode();if(mode==XrOrderMode::AttackMove)TheInGameUI->toggleAttackMoveToMode();
	const bool force=mode==XrOrderMode::ForceAttack;
	auto *target=(mode==XrOrderMode::Move || mode==XrOrderMode::AttackMove || mode==XrOrderMode::ForceMove) ? nullptr:
		TheTacticalView->pickDrawable(&s_activePixel,force,(PickType)getPickTypesForContext(force));
	TheGameClient->evaluateContextCommand(target,&ground,CommandTranslator::DO_COMMAND);
	TheInGameUI->setWaypointMode(oldQueue);TheInGameUI->setForceAttackMode(oldForce);
	TheInGameUI->setForceMoveMode(oldMove);
	TheInGameUI->clearAttackMoveToMode();if(oldAttack)TheInGameUI->toggleAttackMoveToMode();
}
void XrGameBoot_SpatialClick(bool cancel) {
	if(!s_spatialActive || !XrGameBoot_CanAdjustWorld() || !TheInGameUI || !TheMessageStream || !TheGameClient) return;
	if(cancel) {
		s_groupNotice="";
		if(s_tactics.mode!=XrOrderMode::Context || s_tactics.cornerKnown || s_tactics.queue) s_tactics.cancel();
		else TouchInput::cancelOrDeselect();return;
	}
	if(TouchInput::hasArmedCommand() || TheInGameUI->getPendingPlaceType()) {
		// Commit through the existing placement/GUI translator, no custom order.
		XrGameBoot_Pointer(true,float(s_activePixel.x),float(s_activePixel.y),true,false,0);
		XrGameBoot_Pointer(true,float(s_activePixel.x),float(s_activePixel.y),false,false,0);
	} else {
		Coord3D ground={};const bool onTerrain=TheTacticalView->screenToTerrain(&s_activePixel,&ground);
		auto *picked=TheTacticalView->pickDrawable(&s_activePixel,FALSE,PICK_TYPE_SELECTABLE);
		const auto mode=s_tactics.mode;
		const auto selectable=[](Drawable *d) {return d && CanSelectDrawable(d,FALSE,TRUE) && d->getObject() &&
			d->getObject()->isLocallyControlled() && !d->getObject()->isContained() && !d->getObject()->isOffMap() &&
			!d->getObject()->isEffectivelyDead() && !d->getFullyObscuredByShroud();};
		if(mode==XrOrderMode::Select || mode==XrOrderMode::Add || mode==XrOrderMode::Box || mode==XrOrderMode::BoxAdd) {
			std::vector<Drawable *> targets;
			const bool box=mode==XrOrderMode::Box || mode==XrOrderMode::BoxAdd;
			if(box) {
				if(!onTerrain)return;
				if(!s_tactics.cornerKnown) {s_tactics.corner={ground.x,ground.y,ground.z};s_tactics.cornerKnown=true;return;}
				const auto first=xrTransformPoint(s_worldMapping,s_tactics.corner),second=xrTransformPoint(s_worldMapping,{ground.x,ground.y,ground.z});
				for(auto *d=TheGameClient->getDrawableList();d;d=d->getNextDrawable()) if(selectable(d) && d->isMassSelectable()) {
					const auto *p=d->getPosition();const auto local=xrTransformPoint(s_worldMapping,{p->x,p->y,p->z});
					if(xrSelectionContains(first,second,local)) targets.push_back(d);
				}
				s_tactics.cornerKnown=false;
			} else if(selectable(picked)) targets.push_back(picked);
			bool add=mode==XrOrderMode::Add || mode==XrOrderMode::BoxAdd;
			// A building and an army cannot share the native command bar.
			// Replace that incompatible selection instead of silently doing nothing.
			if(add && !box && picked && !picked->isMassSelectable()) add=false;
			if(add && TheInGameUI->getSelectCount()>0 && !TheInGameUI->getFirstSelectedDrawable()->isMassSelectable()) add=false;
			if(add && !box && targets.size()==1 && picked->isSelected()) {
				TheInGameUI->deselectDrawable(picked);auto *m=TheMessageStream->appendMessage(GameMessage::MSG_REMOVE_FROM_SELECTED_GROUP);
				m->appendObjectIDArgument(picked->getObject()->getID());return;
			}
			if(targets.empty()) return; // A miss never destroys the current army selection.
			if(!add) TheInGameUI->deselectAllDrawables(false);
			auto *message=TheMessageStream->appendMessage(GameMessage::MSG_CREATE_SELECTED_GROUP);
			message->appendBooleanArgument(!add);
			for(auto *d:targets) {
				if(d->isSelected()) continue;
				if(!xrSelectionHasRoom(TheInGameUI->getSelectCount(),TheInGameUI->getMaxSelectCount())) break;
				TheInGameUI->selectDrawable(d);message->appendObjectIDArgument(d->getObject()->getID());
			}
			// Replacement selection returns to ordinary orders; additive mode is sticky.
			if(!add) s_tactics.setMode(XrOrderMode::Context);
		} else if(mode==XrOrderMode::Context && !s_tactics.queue) TouchInput::tap(s_activePixel.x,s_activePixel.y,TRUE);
		else if(TouchInput::hasControllableSelection()) {
			if(mode==XrOrderMode::Guard || mode==XrOrderMode::GuardHold) {
				if(xrIssueGuard(picked,ground,onTerrain,mode==XrOrderMode::GuardHold))s_tactics.cancel();
				return;
			}
			if(!onTerrain){s_groupNotice="Keine Bodenposition getroffen";return;}
			xrIssueMovement(mode,ground,s_tactics.queue);s_groupNotice="";
			if(!s_tactics.queue) s_tactics.setMode(XrOrderMode::Context);
		}
	}
	GXLOG("P7.4 spatial commit pixel=(%d,%d)",s_activePixel.x,s_activePixel.y);
}

// GeneralsX @feature Codex 13/09/2026 World-space drag, not raw mouse events.
void XrGameBoot_SpatialTrigger(bool down,bool available,bool additive) {
	Coord3D ground={};
	available=available && s_spatialActive && XrGameBoot_CanAdjustWorld() && TheInGameUI &&
		TheTacticalView;
	const bool onTerrain=available && TheTacticalView->screenToTerrain(&s_activePixel,&ground);
	if(!s_triggerGesture.active)s_rayDrag.begin(s_activeAim,s_activeRoom);
	// A visible object can be clicked without ground behind it. A box needs ground.
	if(s_triggerGesture.active && s_triggerGesture.canDrag && !onTerrain) {
		if(s_triggerGesture.dragging)available=false;
		else s_triggerGesture.canDrag=false;
	}
	const auto point=s_rayDrag.point(s_activeAim);
	const bool allow=available && onTerrain && !TouchInput::hasArmedCommand() && !TheInGameUI->getPendingPlaceType() &&
		!s_tactics.queue && int(s_tactics.mode)<=int(XrOrderMode::BoxAdd);
	const auto event=s_triggerGesture.update(down,available,point,allow,additive || s_tactics.mode==XrOrderMode::Add || s_tactics.mode==XrOrderMode::BoxAdd);
	if(event==XrTriggerEvent::Begin) {
		s_triggerStart={ground.x,ground.y,ground.z};
		s_triggerRayStart=s_activeStart;s_triggerRayEnd=s_activeEnd;s_triggerPixel=s_activePixel;
	}
	if(event==XrTriggerEvent::Drag) {s_tactics.corner=s_triggerStart;s_tactics.cornerKnown=true;s_triggerPreview=true;}
	if(event==XrTriggerEvent::Drop) {
		s_tactics.setMode(s_triggerGesture.add ? XrOrderMode::BoxAdd:XrOrderMode::Box);
		s_tactics.corner=s_triggerStart;s_tactics.cornerKnown=true;
		XrGameBoot_SpatialClick(false);s_tactics.setMode(XrOrderMode::Context);s_triggerPreview=false;
	}
	if(event==XrTriggerEvent::Click) {
		const bool addClick=s_triggerGesture.add && !TouchInput::hasArmedCommand() &&
			!TheInGameUI->getPendingPlaceType() && !s_tactics.queue && int(s_tactics.mode)<=int(XrOrderMode::BoxAdd);
		if(addClick)s_tactics.setMode(XrOrderMode::Add);
		// Click what the laser hit at press; release jitter must not change target.
		const auto start=s_activeStart,end=s_activeEnd;const auto pixel=s_activePixel;
		s_activeStart=s_triggerRayStart;s_activeEnd=s_triggerRayEnd;s_activePixel=s_triggerPixel;
		XrGameBoot_SpatialClick(false);
		s_activeStart=start;s_activeEnd=end;s_activePixel=pixel;
		if(addClick)s_tactics.setMode(XrOrderMode::Context);
	}
	if(event==XrTriggerEvent::Cancel && s_triggerPreview) {s_tactics.cornerKnown=false;s_triggerPreview=false;}
}

// GeneralsX @feature Codex 13/09/2026 Explicit controller command palette.
// Local meta events enter the same translators as keyboard hotkeys; their
// resulting network messages and group membership remain owned by the engine.
void XrGameBoot_TacticalAction(int action) {
	if(!XrGameBoot_CanAdjustWorld() || !TheInGameUI || !TheMessageStream) return;
	s_groupNotice="";
	if(action>=40 && action<=42) {
		if(!XrGameBoot_TacticalReason(action).empty())return;
		TouchInput::backOutOfArmedState();s_tactics.cancel();
		if(action==40){TheMessageStream->appendMessage(GameMessage::MSG_META_CREATE_FORMATION);return;}
		s_tactics.setMode(action==41 ? XrOrderMode::ForceMove:XrOrderMode::GuardHold);return;
	}
	if(action>=0 && action<=8) {
		if(action==8 && !XrGameBoot_TacticalReason(action).empty())return;
		TouchInput::backOutOfArmedState();s_tactics.setMode(static_cast<XrOrderMode>(action));
		if(action!=5)s_tactics.queue=false;return;
	}
	if(action==9) {s_tactics.queue=!s_tactics.queue;if(s_tactics.queue)s_tactics.setMode(XrOrderMode::Move);return;}
	if(action==10) {s_tactics.cancel();TouchInput::backOutOfArmedState();TheMessageStream->appendMessage(GameMessage::MSG_META_STOP);return;}
	if(action==11) {TheMessageStream->appendMessage(GameMessage::MSG_META_SCATTER);return;}
	if(action==12) {TheMessageStream->appendMessage(GameMessage::MSG_META_SELECT_NEXT_IDLE_WORKER);return;}
	if(action==13) {s_tactics.cancel();TouchInput::cancelOrDeselect();return;}
	if(action==20 || action==21) {s_tactics.group=(s_tactics.group+(action==20 ? 9:1))%10;return;}
	GameMessage::Type message=GameMessage::MSG_INVALID;
	switch(action) {
	case 22:case 23:case 24:case 25:{const int op[]={1,0,2,3};XrGameBoot_TacticalGroup(s_tactics.group,op[action-22]);return;}
	case 26:message=GameMessage::MSG_META_SELECT_NEXT_UNIT;break;
	case 27:message=GameMessage::MSG_META_SELECT_NEXT_WORKER;break;
	case 28:message=GameMessage::MSG_META_SELECT_HERO;break;
	case 29:message=GameMessage::MSG_META_SELECT_ALL_AIRCRAFT;break;
	case 30:TheInGameUI->selectMatchingAcrossMap();break;
	case 31:{KindOfMaskType required,excluded;required.set(KINDOF_SELECTABLE);excluded.set(KINDOF_STRUCTURE);
		TheInGameUI->selectAllUnitsByTypeAcrossMap(required,excluded);break;}
	}
	if(message!=GameMessage::MSG_INVALID) TheMessageStream->appendMessage(message);
	s_tactics.setMode(XrOrderMode::Context);
}
bool XrGameBoot_FormationActive() {
	if(!TheInGameUI || !TheInGameUI->getSelectCount())return false;
	FormationID id=NO_FORMATION_ID;
	for(auto *draw:*TheInGameUI->getAllSelectedDrawables()) {
		auto *object=draw ? draw->getObject():nullptr;
		if(!object || !object->isLocallyControlled() || !object->getAIUpdateInterface() ||
			object->getFormationID()==NO_FORMATION_ID)return false;
		if(id!=NO_FORMATION_ID && object->getFormationID()!=id)return false;
		id=object->getFormationID();
	}
	return id!=NO_FORMATION_ID;
}
std::string XrGameBoot_TacticalReason(int action) {
	if(!XrGameBoot_CanAdjustWorld())return xrTr("Im Dialog oder bei gesperrter Kamera nicht verfügbar");
	if(!TheInGameUI || !TouchInput::hasControllableSelection())return xrTr("Zuerst eigene bewegliche Einheiten auswählen");
	int mobile=0;bool hasFormation=false;
	for(auto *draw:*TheInGameUI->getAllSelectedDrawables()) {
		auto *o=draw ? draw->getObject():nullptr;
		if(o && o->getFormationID()!=NO_FORMATION_ID)hasFormation=true;
		if(o && o->isLocallyControlled() && o->getAIUpdateInterface() && !o->isKindOf(KINDOF_STRUCTURE) &&
			!o->isEffectivelyDead() && !o->isContained() && !o->isOffMap())++mobile;
	}
	if(!mobile || mobile!=TheInGameUI->getSelectCount())return xrTr("Zuerst eigene bewegliche Einheiten auswählen");
	if(action==40 && hasFormation && !XrGameBoot_FormationActive())return xrTr("Gemischte Formationen: zuerst einzeln lösen");
	if(action==40 && mobile<2 && !XrGameBoot_FormationActive())return xrTr("Formation benötigt mindestens zwei Einheiten");
	return {};
}
bool XrGameBoot_BookmarkKnown(int slot) {
	return slot>=0 && slot<4 && s_bookmarkKnown[slot];
}
void XrGameBoot_Bookmark(int slot,bool save) {
	if(slot<0 || slot>=4 || !XrGameBoot_CanAdjustWorld())return;
	if(!save && !s_bookmarkKnown[slot]){s_groupNotice="Kartenplatz leer: Ansicht merken → A–D";return;}
	TouchInput::backOutOfArmedState();s_tactics.cancel();
	if(save){TheTacticalView->getLocation(&s_bookmarks[slot]);s_bookmarkKnown[slot]=true;}
	else TheTacticalView->userSetLocation(&s_bookmarks[slot]);
	s_groupNotice=save ? "Kartenansicht gespeichert (nur diese Partie)":"Kartenansicht aufgerufen; Tisch bleibt unverändert";
}
void XrGameBoot_CancelTarget() {
	// Focus/tracking cancellation never becomes deselection or an order.
	s_tactics.cancel();s_triggerGesture=XrTriggerGesture{};s_triggerPreview=false;s_groupNotice="";
	if(TheInGameUI)TouchInput::backOutOfArmedState();
}
void XrGameBoot_TacticalGroup(int group,int operation) {
	if(group<0 || group>=10 || operation<0 || operation>3 || !XrGameBoot_CanAdjustWorld() || !TheInGameUI || !TheMessageStream) return;
	s_tactics.group=group;
	// GeneralsX @bugfix Codex 14/09/2026 Empty recall must explain itself,
	// not silently clear the units the player was trying to put in a group.
	if(operation==1 && TheInGameUI->getSelectCount()==0){s_groupNotice="Keine Auswahl: zuerst eigene Einheiten markieren";return;}
	if(operation!=1 && XrGameBoot_GroupSize(group)==0){s_groupNotice="Gruppe leer: Einheiten wählen → Speichern → Zahl";return;}
	TouchInput::backOutOfArmedState();s_tactics.setMode(XrOrderMode::Context);
	if(operation==3) {
		// Native view-team translator excludes slot zero (group >= 1). Use
		// its camera-only operation for all ten XR slots; no game rule changes.
		auto *player=ThePlayerList ? ThePlayerList->getLocalPlayer():nullptr;
		auto *squad=player ? player->getHotkeySquad(group):nullptr;
		if(squad)for(auto *object:squad->getLiveObjects())if(object && object->getDrawable()) {
			TheTacticalView->userLookAt(object->getDrawable()->getPosition());break;
		}
	} else {
		const GameMessage::Type messages[]={GameMessage::MSG_META_SELECT_TEAM0,GameMessage::MSG_META_CREATE_TEAM0,GameMessage::MSG_META_ADD_TEAM0};
		TheMessageStream->appendMessage(static_cast<GameMessage::Type>(messages[operation]+group));
	}
	const char *notices[]={"Gruppe ausgewählt","Speicherauftrag gesendet; Belegung siehe Gruppentaste","Gruppe zur Auswahl hinzugefügt, nicht neu gespeichert","Gruppenansicht zentriert"};
	s_groupNotice=notices[operation];
}
int XrGameBoot_GroupSize(int group) {
	if(group<0 || group>=10)return 0;
	auto *player=ThePlayerList ? ThePlayerList->getLocalPlayer():nullptr;
	auto *squad=player ? player->getHotkeySquad(group):nullptr;
	return squad ? int(squad->getLiveObjects().size()):0;
}
void XrGameBoot_Communicator() {
	if(!XrGameBoot_CanAdjustWorld())return;
	s_tactics.cancel();s_groupNotice="";TouchInput::backOutOfArmedState();ToggleDiplomacy(FALSE);
}
void XrGameBoot_SetLanguage(int language) {
	if(language<0 || language>1)return;
	g_xrLanguage=static_cast<XrLanguage>(language);
	const char *token=language==0 ? "german":"english";
	const std::string base=std::string("data/")+token+"/generals.";
	if(!TheFileSystem || (!TheFileSystem->doesFileExist((base+"str").c_str()) && !TheFileSystem->doesFileExist((base+"csf").c_str()))) {
		s_languageNotice="Nur XR übersetzt: Spiel-Sprachdateien fehlen";return;
	}
	const std::string temp=s_textLanguagePath+".tmp";
	FILE *file=s_textLanguagePath.empty() ? nullptr:fopen(temp.c_str(),"w");
	bool saved=false;
	if(file){const bool wrote=fprintf(file,"%s\n",token)>0;const bool closed=fclose(file)==0;saved=wrote && closed && rename(temp.c_str(),s_textLanguagePath.c_str())==0;}
	s_languageNotice=saved ? "Spieltexte vorbereitet: Neustart nötig":"XR übersetzt; Spielsprache konnte nicht gespeichert werden";
}
std::string XrGameBoot_LanguageStatus() {
	return xrTr(s_languageNotice);
}
std::string XrGameBoot_TacticalStatus() {
	const char *names[]={"Kontextbefehl","Einheit wählen","Auswahl +/-","Bereich: zwei Ecken","Bereich hinzufügen","Bewegen","Angriffsmarsch","Erzwungener Angriff","Position bewachen","Zwangsbewegung","Ohne Verfolgung"};
	const char *mode=s_triggerPreview ? (s_triggerGesture.add ? "Rahmen hinzufügen":"Auswahlrahmen"):names[int(s_tactics.mode)];
	char text[320];snprintf(text,sizeof(text),xrTr("%s%s · %d gewählt · Gruppe %d%s"),xrTr(mode),
		xrTr(s_triggerPreview ? " (loslassen zum Wählen)":s_tactics.cornerKnown ? " (zweite Ecke)":""),TheInGameUI ? TheInGameUI->getSelectCount():0,s_tactics.group+1,xrTr(s_tactics.queue ? " · Wegpunkte AN":""));return text;
}
std::string XrGameBoot_TacticalHint() {
	if(*s_groupNotice)return xrTr(s_groupNotice);
	return xrTr(xrOrderHint(s_tactics.mode,TheInGameUI ? TheInGameUI->getSelectCount():0));
}
void XrGameBoot_TacticalState(int &mode,int &group,bool &queue) {
	mode=int(s_tactics.mode);group=s_tactics.group;queue=s_tactics.queue;
}

static std::string xrText(const UnicodeString &s) {
	char *bytes=SDL_iconv_string("UTF-8","WCHAR_T",reinterpret_cast<const char *>(s.str()),(s.getLength()+1)*sizeof(WideChar));
	std::string result=bytes ? bytes:"";SDL_free(bytes);return result;
}
std::string XrGameBoot_WorldHoverInfo() {
	if(!s_spatialActive || !TheTacticalView || !GX_XR_SplitUIAllowed()) return {};
	auto *d=TheTacticalView->pickDrawable(&s_activePixel,FALSE,PICK_TYPE_SELECTABLE);
	if(!d || !d->getObject() || d->getFullyObscuredByShroud())
		return s_tactics.mode!=XrOrderMode::Context || s_tactics.queue ? std::string(xrTr("Einheitensteuerung"))+"\n"+XrGameBoot_TacticalStatus():std::string{};
	auto *o=d->getObject();return xrText(o->getTemplate()->getDisplayName())+"\n"+XrGameBoot_TacticalStatus();
}

// GeneralsX @feature Codex 13/09/2026 Decoration uses the same eye depth as
// the live game. Terrain samples close its cut faces; no replacement map.
static void drawXrWorldDecorations() {
	if(!d3d8gles_XRStereoTexture(0) || !s_mappingReady || !TheTerrainLogic || !TheGameClient) return;
	XrBoardMesh mesh;
	if(s_worldFrame.boardFrame) {
		static XrBoardMesh cached;static float previous[16]={};static float aspect=0;static unsigned frame=0;
		if(cached.vertices.empty() || memcmp(previous,s_worldMapping,sizeof(previous)) || aspect!=s_worldAspect || (++frame%30)==0) {
			cached=xrBuildBoard(s_worldAspect,int(ceilf(s_worldSpan/5)),[](float x,float y) {
				const auto p=xrInversePoint(s_worldMapping,{x,y,0});
				return xrTransformPoint(s_worldMapping,{p.x,p.y,TheTerrainLogic->getGroundHeight(p.x,p.y)}).z;
			},gxXrBoardCeiling(s_worldMapping));
			for(auto &v:cached.vertices) v.position=xrInversePoint(s_worldMapping,v.position);
			memcpy(previous,s_worldMapping,sizeof(previous));aspect=s_worldAspect;
		}
		mesh.vertices=cached.vertices;
	}
	const auto feedbackStart=mesh.vertices.size();
	Drawable *hover=s_spatialActive ? TheTacticalView->pickDrawable(&s_activePixel,FALSE,PICK_TYPE_SELECTABLE):nullptr;
	unsigned marked=0;
	for(auto *d=TheGameClient->getDrawableList();d && marked<192;d=d->getNextDrawable()) {
		auto *o=d->getObject();if(!o || o->isEffectivelyDead() || d->getFullyObscuredByShroud()) continue;
		const auto *p=o->getPosition();const auto center=xrTransformPoint(s_worldMapping,{p->x,p->y,p->z});
		if(fabsf(center.x)>.49f || fabsf(center.y)>s_worldAspect*.5f-.01f || center.z<-.14f || center.z>gxXrBoardCeiling(s_worldMapping)-.01f) continue;
		const bool selected=d->isSelected(),pointed=d==hover;
		const bool infantry=o->isLocallyControlled() && o->isKindOf(KINDOF_INFANTRY);
		if(!selected && !pointed && !infantry) continue;++marked;
		if(s_worldFrame.unitRings && (selected || pointed || infantry)) {
			const auto firstRing=mesh.vertices.size();
			const float radius=std::max(o->getGeometryInfo().getMajorRadius(),3.0f);
			const XrVector3f at={p->x,p->y,TheTerrainLogic->getGroundHeight(p->x,p->y)+.7f};
			mesh.ring(at,radius+1,1.8f,{.06f,.09f,.08f});
			if(selected || pointed) mesh.ring(xrAdd(at,{0,0,.12f}),radius+1.3f,.8f,selected ? XrVector3f{.25f,.95f,.38f}:XrVector3f{.15f,.85f,1});
			for(size_t i=firstRing;i<mesh.vertices.size();++i) {
				auto &v=mesh.vertices[i].position;v.z+=TheTerrainLogic->getGroundHeight(v.x,v.y)-at.z+.7f;
			}
		}
		if(s_worldFrame.healthBars && (selected || pointed) && o->getBodyModule()) {
			const auto *body=o->getBodyModule();const float maxHealth=body->getMaxHealth();if(maxHealth<=0) continue;
			const float health=std::clamp(body->getHealth()/maxHealth,0.0f,1.0f);
			const float w=.035f*s_worldSpan/s_worldFrame.board.width,h=w*.13f;
			const auto &camera=s_renderCamera->Get_Transform();
			const XrVector3f right={camera[0][0],camera[1][0],camera[2][0]},up={camera[0][1],camera[1][1],camera[2][1]};
			const XrVector3f top={p->x,p->y,p->z+o->getGeometryInfo().getMaxHeightAbovePosition()+4};
			const auto left=xrSub(top,xrScale(right,w*.5f));
			const auto rect=[&](float width,float height,XrVector3f color) {
				mesh.quad(left,xrAdd(left,xrScale(right,width)),xrAdd(xrAdd(left,xrScale(right,width)),xrScale(up,height)),xrAdd(left,xrScale(up,height)),color);
			};
			rect(w,h,{.04f,.06f,.06f});
			// Offset toward the viewer to avoid coplanar fill/outline flicker.
			const auto back=XrVector3f{camera[0][2]*.1f,camera[1][2]*.1f,camera[2][2]*.1f};
			const auto a=xrAdd(xrAdd(left,xrScale(right,w*.04f)),xrScale(up,h*.2f));
			const auto b=xrAdd(a,xrScale(right,w*.92f*health));
			mesh.quad(xrAdd(a,back),xrAdd(b,back),xrAdd(xrAdd(b,back),xrScale(up,h*.6f)),xrAdd(xrAdd(a,back),xrScale(up,h*.6f)),{1-health,health,.12f});
		}
	}
	if(s_tactics.cornerKnown && s_spatialActive) {
		Coord3D ground;if(TheTacticalView->screenToTerrain(&s_activePixel,&ground)) {
			const auto a=xrTransformPoint(s_worldMapping,s_tactics.corner),b=xrTransformPoint(s_worldMapping,{ground.x,ground.y,ground.z});
			const XrVector3f points[]={{a.x,a.y,0},{b.x,a.y,0},{b.x,b.y,0},{a.x,b.y,0}};
			for(int side=0;side<4;++side) for(int i=0;i<32;++i) {
				const auto first=points[side],delta=xrSub(points[(side+1)%4],first);
				auto p=xrInversePoint(s_worldMapping,xrAdd(first,xrScale(delta,float(i)/32)));
				auto q=xrInversePoint(s_worldMapping,xrAdd(first,xrScale(delta,float(i+1)/32)));
				p.z=TheTerrainLogic->getGroundHeight(p.x,p.y)+1;q.z=TheTerrainLogic->getGroundHeight(q.x,q.y)+1;
				auto normal=xrCross(xrSub(q,p),{0,0,1});const float n=xrLength(normal);if(n<.001f) continue;normal=xrScale(normal,1/n);
				mesh.quad(xrSub(p,normal),xrSub(q,normal),xrAdd(q,normal),xrAdd(p,normal),{.2f,.95f,.8f});
			}
		}
	}
	for(size_t i=0;i<mesh.vertices.size();++i) mesh.vertices[i].a=i>=feedbackStart ? 1:0;
	if(!mesh.vertices.empty()) d3d8gles_DrawXRDecorations(reinterpret_cast<const float *>(mesh.vertices.data()),int(mesh.vertices.size()));
}
void XrGameBoot_SetSplitEnabled(bool enabled) { s_splitEnabled=enabled; }
bool XrGameBoot_SplitReady() { return GX_XR_SplitUIAllowed() && d3d8gles_XRSplitReady(); }
unsigned int XrGameBoot_WorldTexture() { return d3d8gles_GetXRWorldTexture(); }
unsigned int XrGameBoot_UITexture() { return d3d8gles_GetXRUITexture(); }
XrGameRect XrGameBoot_WorldRect() {
	if(!TheTacticalView) return {};
	int x=0,y=0; TheTacticalView->getOrigin(&x,&y);
	return xrViewportRect(x,y,TheTacticalView->getWidth(),TheTacticalView->getHeight(),
		XrGameBoot_GameWidth(),XrGameBoot_GameHeight());
}
void XrGameBoot_RoutePointer(int target) {
	if(!TheMouse) return;
	const auto route=target==1 ? Mouse::PointerTarget::WORLD : target==2 ? Mouse::PointerTarget::WINDOWS : Mouse::PointerTarget::ALL;
	if(route==Mouse::PointerTarget::WORLD && TheMouse->getPointerTarget()!=route && TheWindowManager) {
		ICoord2D outside={-1000,-1000};
		TheWindowManager->winProcessMouseEvent(GWM_MOUSE_POS,&outside,nullptr);
	}
	TheMouse->setPointerTarget(route);
}

// GeneralsX @feature Codex 13/09/2026 Query current widget geometry; never retain
// pointers across frames because the control bar rebuilds them on selection.
XrGameRect XrGameBoot_CommandRect() {
	static unsigned cachedFrame=~0u;
	static XrGameRect cached;
	if(cachedFrame==s_xrPresentationFrame) return cached;
	cachedFrame=s_xrPresentationFrame;
	float top=XrGameBoot_WorldRect().y;
	if(TheWindowManager && TheNameKeyGenerator) {
		auto *bar=TheWindowManager->winGetWindowFromId(nullptr,
			TheNameKeyGenerator->nameToKey("ControlBar.wnd:ControlBarParent"));
		if(bar && !bar->winIsHidden()) {
			int x=0,y=0; bar->winGetScreenPosition(&x,&y);
			top=1.0f-float(y)/XrGameBoot_GameHeight();
		}
	}
	cached=xrCommandRect(top);
	return cached;
}
bool XrGameBoot_HasUIAt(float x,float y) {
	return TheWindowManager && TheWindowManager->getWindowForInputAt(int(x),int(y))!=nullptr;
}

// GeneralsX @feature Codex 13/09/2026 Localized build/command descriptions
// in an independent XR card, including disabled buttons. Never retain widget
// pointers across frames or interpret unvalidated gadget user-data as commands.
std::string XrGameBoot_HoverInfo(float x,float y) {
	if(!TheControlBar || !TheGameText || !TheWindowManager || !ThePlayerList || !ThePlayerList->getLocalPlayer() || !GX_XR_SplitUIAllowed()) return {};
	const auto utf8=[](const UnicodeString &s) {
		char *bytes=SDL_iconv_string("UTF-8","WCHAR_T",reinterpret_cast<const char *>(s.str()),(s.getLength()+1)*sizeof(WideChar));
		std::string text=bytes ? bytes:"";SDL_free(bytes);
		text.erase(std::remove(text.begin(),text.end(),'&'),text.end());return text;
	};
	// GeneralsX @feature Codex 14/09/2026 Respect the native modal/input root,
	// then include disabled descendants (science tree as well as command bar).
	// Never reinterpret arbitrary gadget data or retain windows across frames.
	auto *target=TheWindowManager->getWindowForTooltipAt(int(x),int(y));
	if(!target) return {};
	for(auto *window=target;window;window=window->winGetParent()) {
		if(window->winIsHidden()) continue;
		if(!(window->winGetStyle() & GWS_PUSH_BUTTON)) continue;
		int px=0,py=0,w=0,h=0;window->winGetScreenPosition(&px,&py);window->winGetSize(&w,&h);
		if(x<px || y<py || x>=px+w || y>=py+h) continue;
		const void *data=GadgetButtonGetData(window);
		auto *cmd=TheControlBar->getCommandButtons();while(cmd && cmd!=data) cmd=cmd->getNext();
		if(!cmd) continue;
		UnicodeString name,cost,description;
		TheControlBar->describeCommand(cmd,name,cost,description);
		// A genuinely free production command still has a visible price.
		if(cost.isEmpty() && cmd->getThingTemplate() &&
			(cmd->getCommandType()==GUI_COMMAND_DOZER_CONSTRUCT ||
			 cmd->getCommandType()==GUI_COMMAND_UNIT_BUILD))
			cost.format(TheGameText->fetch("TOOLTIP:Cost"),cmd->getThingTemplate()->calcCostToBuild(ThePlayerList->getLocalPlayer()));
		const auto title=utf8(name);
		return (title.empty() ? std::string(cmd->getName().str()):title)+"\n"+
			(cost.isEmpty() ? std::string():utf8(cost)+"\n\n")+utf8(description);
	}
	// Original static tooltips remain useful for shell/dialog controls too.
	for(auto *window=target;window;window=window->winGetParent()) {
		UnicodeString name,description;
		const int id=window->winGetWindowId();
		if(id==NAMEKEY("ControlBar.wnd:MoneyDisplay")) {
			name=TheGameText->fetch("CONTROLBAR:Money");description=TheGameText->fetch("CONTROLBAR:MoneyDescription");
		} else if(id==NAMEKEY("ControlBar.wnd:PowerWindow")) {
			name=TheGameText->fetch("CONTROLBAR:Power");description=TheGameText->fetch("CONTROLBAR:PowerDescription");
			auto *player=TheControlBar->getCurrentlyViewedPlayer();
			auto *energy=player ? player->getEnergy():nullptr;
			description.format(description,energy ? energy->getProduction():0,energy ? energy->getConsumption():0);
		} else if(id==NAMEKEY("ControlBar.wnd:GeneralsExp")) {
			name=TheGameText->fetch("CONTROLBAR:GeneralsExp");description=TheGameText->fetch("CONTROLBAR:GeneralsExpDescription");
		} else description=window->winGetInstanceData()->getTooltipText();
		if(!description.isEmpty())return utf8(name)+"\n"+utf8(description);
	}
	return {};
}

bool XrGameBoot_CameraPreset(int preset)
{
	if (!XrGameBoot_CanControlCamera()) return false;
	// GeneralsX @bugfix Codex 13/09/2026 W3DView::setZoomToDefault only
	// invalidates state; userSetZoom(1) actually resets desired height to max.
	// Do not restore getZoom() through setZoom(): their units differ on terrain.
	if(preset==kXrCameraFavorite && s_hasCameraFavorite) {
		TheTacticalView->userSetAngle(s_cameraFavorite.yaw);
		TheTacticalView->userSetPitch(s_cameraFavorite.pitch);
		TheTacticalView->userZoom(s_cameraFavorite.height-TheTacticalView->getHeightAboveGround());
	} else {
		if(preset<0 || preset>=kXrCameraPresetCount) preset=1;
		TheTacticalView->userSetAngleToDefault();
		TheTacticalView->userSetZoom(1.0f);
		TheTacticalView->userSetPitch(xrCameraPitch(preset,TheTacticalView->getDefaultPitch()));
	}
	GXLOG("camera P4: %s pitch=%.1f yaw=%.1f height=%.1f zoom=%.3f",xrCameraName(preset),
		TheTacticalView->getPitch()/kXrCameraRadians,TheTacticalView->getAngle()/kXrCameraRadians,
		TheTacticalView->getHeightAboveGround(),TheTacticalView->getZoom());
	return true;
}

int XrGameBoot_DefaultCameraPreset() { return s_hasCameraFavorite ? kXrCameraFavorite : 1; }
float XrGameBoot_CameraPitchDegrees() {
	return XrGameBoot_IsInteractiveGame() && TheTacticalView ? TheTacticalView->getPitch()/kXrCameraRadians : 0;
}
bool XrGameBoot_SaveCameraDefault()
{
	if(!XrGameBoot_CanControlCamera()) return false;
	XrCameraProfile candidate;
	candidate.yaw=std::remainder(TheTacticalView->getAngle(),6.28318530718f);
	candidate.pitch=TheTacticalView->getPitch(); candidate.height=TheTacticalView->getHeightAboveGround();
	if(!candidate.save(s_cameraPath.c_str())) { GXLOGE("P4 camera default save failed"); return false; }
	s_cameraFavorite=candidate; s_hasCameraFavorite=true;
	GXLOG("P4 camera default saved pitch=%.1f yaw=%.1f height=%.1f",candidate.pitch/kXrCameraRadians,
		candidate.yaw/kXrCameraRadians,candidate.height);
	return true;
}

bool XrGameBoot_AdjustCamera(float yawRadians, float pitchRadians)
{
	if (!XrGameBoot_CanControlCamera()) return false;
	if (yawRadians != 0) TheTacticalView->userSetAngle(TheTacticalView->getAngle()+yawRadians);
	if (pitchRadians != 0) TheTacticalView->userSetPitch(TheTacticalView->getPitch()+pitchRadians);
	return true;
}

// GeneralsX @feature Codex 13/09/2026 Camera-only navigation needs no pointer
// hit on raised geometry. Route through native user actions and script locks.
bool XrGameBoot_NavigateWorld(float rightSeconds,float forwardSeconds,float zoomSeconds)
{
	if(!XrGameBoot_CanAdjustWorld() || !std::isfinite(rightSeconds) ||
		!std::isfinite(forwardSeconds) || !std::isfinite(zoomSeconds)) return false;
	bool changed=false;
	if(rightSeconds!=0 || forwardSeconds!=0) {
		if(!s_mappingReady || !TheTerrainLogic)return false;
		// GeneralsX @bugfix Codex 14/09/2026 scrollBy uses a z=-1 view plane,
		// not map units. Navigate in the board basis through the native user gate.
		const auto delta=xrWorldPan(s_worldMapping,s_worldSpan,rightSeconds,forwardSeconds);
		auto target=TheTacticalView->getPosition();target.x+=delta.x;target.y+=delta.y;
		target.z=TheTerrainLogic->getGroundHeight(target.x,target.y);
		changed=TheTacticalView->userLookAt(&target);
	}
	if(zoomSeconds!=0) changed=TheTacticalView->userZoom(-std::clamp(zoomSeconds,-.05f,.05f)*
		TheTacticalView->getHeightAboveGround()*.8f) || changed;
	return changed;
}

bool XrGameBoot_IsInteractiveGame()
{
	if (!s_booted || TheGameLogic == nullptr) {
		return false;
	}
	return GameLogic::isInInteractiveGame(TheGameLogic->getGameMode()) == TRUE;
}

// GeneralsX @feature Codex 13/09/2026 Persistent controller ray -> existing
// pointer pipeline, including hover, drag and balanced button releases.
void XrGameBoot_Pointer(bool active, float x, float y, bool select, bool secondary, float wheel)
{
	static bool held[2] = {false, false};
	static float lastX = kXrGameWidth * 0.5f, lastY = kXrGameHeight * 0.5f;
	auto *mouse = dynamic_cast<SDL3Mouse *>(TheMouse);
	if (!s_booted || !mouse) return;
	SDL_Event e = {};
	e.motion.timestamp = SDL_GetTicksNS();
	e.motion.which = 1; // persistent XR pointer; never SDL_TOUCH_MOUSEID
	if (active) {
		e.type = SDL_EVENT_MOUSE_MOTION;
		e.motion.x = x; e.motion.y = y;
		e.motion.xrel = x-lastX; e.motion.yrel = y-lastY;
		mouse->addSDLEvent(&e);
		lastX = x; lastY = y;
		mouse->onCursorMovedInside();
	} else {
		mouse->onCursorMovedOutside();
	}
	const bool pressed[2] = {active && select, active && secondary};
	if ((!active || (held[1] && !pressed[1])) && TheLookAtTranslator)
		TheLookAtTranslator->cancelScrolling();
	for (int i=0; i<2; ++i) if (pressed[i] != held[i]) {
		e = {};
		e.type = pressed[i] ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP;
		e.button.timestamp = SDL_GetTicksNS(); e.button.which = 1;
		e.button.button = i == 0 ? SDL_BUTTON_LEFT : SDL_BUTTON_RIGHT;
		e.button.down = pressed[i]; e.button.clicks = 1;
		e.button.x = lastX; e.button.y = lastY;
		mouse->addSDLEvent(&e);
		held[i] = pressed[i];
		GXLOG("pointer button=%d down=%d at=(%.0f,%.0f)", i, (int)pressed[i], lastX, lastY);
	}
	if (active && wheel != 0.0f) {
		e = {}; e.type = SDL_EVENT_MOUSE_WHEEL;
		e.wheel.timestamp = SDL_GetTicksNS(); e.wheel.which = 1;
		e.wheel.y = wheel; e.wheel.mouse_x = lastX; e.wheel.mouse_y = lastY;
		mouse->addSDLEvent(&e);
	}
}

void XrGameBoot_Key(XrGameKey key, bool down)
{
	auto *keyboard = dynamic_cast<SDL3Keyboard *>(TheKeyboard);
	if (!s_booted || !keyboard) return;
	const SDL_Scancode scans[] = {SDL_SCANCODE_ESCAPE, SDL_SCANCODE_LEFT,
		SDL_SCANCODE_RIGHT, SDL_SCANCODE_UP, SDL_SCANCODE_DOWN};
	SDL_Event e = {};
	e.type = down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
	e.key.timestamp = SDL_GetTicksNS(); e.key.scancode = scans[(int)key];
	e.key.down = down;
	keyboard->addSDLEvent(&e);
}

unsigned int XrGameBoot_GameTexture()
{
	return d3d8gles_GetGameTexture();
}

int XrGameBoot_GameWidth()
{
	if (TheDisplay != nullptr && TheDisplay->getWidth() > 0) {
		return TheDisplay->getWidth();
	}
	return kXrGameWidth;
}

int XrGameBoot_GameHeight()
{
	if (TheDisplay != nullptr && TheDisplay->getHeight() > 0) {
		return TheDisplay->getHeight();
	}
	return kXrGameHeight;
}

void XrGameBoot_Shutdown()
{
	GXLOG("shutdown");
	if (TheGameEngine != nullptr) {
		delete TheGameEngine;
		TheGameEngine = nullptr;
	}
	if (TheFramePacer != nullptr) {
		delete TheFramePacer;
		TheFramePacer = nullptr;
	}
	if (TheVersion != nullptr) {
		delete TheVersion;
		TheVersion = nullptr;
	}
	GX_XR_OffscreenBoot = false;
	d3d8gles_SetXRConfig(nullptr);
	s_booted = false;
	SDL_Quit();
}

#else // !__ANDROID__
#error "XrGameBoot.cpp is Android-only"
#endif
