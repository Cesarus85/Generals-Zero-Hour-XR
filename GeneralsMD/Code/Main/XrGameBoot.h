// GeneralsX @feature XR port Phase 1.0 - offscreen game boot for the XR
// tabletop (XrHello.cpp drives these from the XR thread).
//
// Boots the full game with no SDL window on the caller's already-current
// EGL context: GX_XR_OffscreenBoot skips the window binding, the XR config
// redirects rendering into the d3d8gles owned FBO, and each XR frame steps
// the game once via executeSingleFrame(). The finished frame is sampled
// from d3d8gles_GetGameTexture() by the XR quad.
//
// Single-threaded by design: init, frames, and shutdown all run on the XR
// thread, the only thread that ever touches this GL context.
#pragma once

#ifdef __ANDROID__

#include <jni.h>
#include "XrLayers.h"
#include "XrWorld.h"
#include <string>
std::string XrGameBoot_HoverInfo(float x,float y);
bool XrGameBoot_CanAdjustWorld();
// GeneralsX @feature Codex 14/09/2026 Native building preview orientation.
bool XrGameBoot_CanRotatePlacement();
bool XrGameBoot_RotatePlacement(float radians);
float XrGameBoot_PlacementDegrees();
void XrGameBoot_TacticalAction(int action);
bool XrGameBoot_ViewBase(); // Native command-center view; selection unchanged.
// GeneralsX @feature Codex 14/09/2026 Native tactics and session-only views.
std::string XrGameBoot_TacticalReason(int action);
bool XrGameBoot_FormationActive();
void XrGameBoot_Bookmark(int slot,bool save);
bool XrGameBoot_BookmarkKnown(int slot);
void XrGameBoot_CancelTarget();
void XrGameBoot_TacticalGroup(int group,int operation); // recall, save, add, center
void XrGameBoot_SpatialTrigger(bool down,bool available,bool additive);
std::string XrGameBoot_TacticalStatus();
std::string XrGameBoot_TacticalHint();
// GeneralsX @feature Ultron 15/09/2026 P21 read-only panel state: armed
// order mode, current group and waypoint queue for persistent button states.
void XrGameBoot_TacticalState(int &mode,int &group,bool &queue);
int XrGameBoot_GroupSize(int group);
void XrGameBoot_Communicator();
void XrGameBoot_SetLanguage(int language);
std::string XrGameBoot_LanguageStatus();
bool XrGameBoot_ExpandedUI();
std::string XrGameBoot_WorldHoverInfo();

#include "Lib/BaseType.h" // Bool

// Game render resolution (injected as -xres/-yres): 16:9, proven panel
// aspect for this UI, sampled 1:1 by the tabletop quad.
constexpr int kXrGameWidth = 1280;
constexpr int kXrGameHeight = 720;

// Full boot: storage env, working directory, SDL events, GL XR config,
// engine init. eglDisplay/eglContext are informational (the context must
// already be current on this thread). Returns false on failure -- the
// caller then falls back to the triangle loop and the log says why.
bool XrGameBoot_Init(JNIEnv *env, jobject activity, void *eglDisplay, void *eglContext);

// One game frame. Returns FALSE once the game wants to quit (or on an
// unexpected exception, logged, rather than crashing across the thread).
Bool XrGameBoot_Frame();

// GeneralsX @bugfix Codex 14/09/2026 Scoped, XR-thread-only presentation
// callback for synchronous loadscreen draws; never steps engine/simulation.
void XrGameBoot_SetLoadingPresenter(void (*present)(void *),void *context);

// True only for an interactive campaign/skirmish/network/replay match.
// Intro movies and every shell/menu state return false so the XR host can
// present them as a conventional front panel rather than a tabletop.
bool XrGameBoot_IsInteractiveGame();

// XR-thread-only pointer and keyboard bridge. UVs are converted by the host
// to engine pixels; this is a persistent tracked pointer, not touch input.
void XrGameBoot_Pointer(bool active, float x, float y, bool select, bool secondary, float wheel);
enum class XrGameKey { Back, Left, Right, Up, Down };
void XrGameBoot_Key(XrGameKey key, bool down);
// Controller-operated XR keyboard for the native Direct Connect text fields.
// 0 = no focused field, 1 = player name, 2 = remote IPv4 address.
int XrGameBoot_DirectConnectTextField();
std::string XrGameBoot_DirectConnectTextValue(int field);
bool XrGameBoot_DirectConnectTextKey(int field,int ascii);

// GeneralsX @feature Codex 13/09/2026 Native camera actions preserve script locks.
bool XrGameBoot_CameraPreset(int preset);
bool XrGameBoot_AdjustCamera(float yawRadians, float pitchRadians);
bool XrGameBoot_NavigateWorld(float rightSeconds,float forwardSeconds,float zoomSeconds);
int XrGameBoot_DefaultCameraPreset();
bool XrGameBoot_SaveCameraDefault();
float XrGameBoot_CameraPitchDegrees();
const char *XrGameBoot_LayoutPath();
const char *XrGameBoot_LegacyLayoutPath();
void XrGameBoot_SetSplitEnabled(bool enabled);
bool XrGameBoot_SplitReady();
XrGameRect XrGameBoot_WorldRect();
XrGameRect XrGameBoot_CommandRect();
bool XrGameBoot_HasUIAt(float x,float y);
bool XrGameBoot_CanStereoWorld();
const char *XrGameBoot_PerformanceScene();
std::string XrGameBoot_PresentationStatus(bool stereoVisible,bool requested);
void XrGameBoot_SetWorldFrame(const XrWorldFrame &frame);
unsigned int XrGameBoot_StereoTexture(int eye);
bool XrGameBoot_StereoAtlas();
bool XrGameBoot_StereoMultiview();
void XrGameBoot_ConfigureMultiview(void *(*resolver)(const char *));
bool XrGameBoot_PickWorld(const XrSurface &board,const XrPosef &aim,XrWorldHit &hit);
void XrGameBoot_SpatialPointer(bool active);
void XrGameBoot_SpatialClick(bool cancel);
unsigned int XrGameBoot_WorldTexture();
unsigned int XrGameBoot_UITexture();
void XrGameBoot_RoutePointer(int target); // 0 composed, 1 world, 2 windows

// Current game frame texture (d3d8gles owned FBO), 0 until the first frame
// has rendered. Bottom-up GL-native: sample with flipped V.
unsigned int XrGameBoot_GameTexture();

// Actual render size (TheDisplay once booted, else the kXrGame* defaults).
int XrGameBoot_GameWidth();
int XrGameBoot_GameHeight();

// Orderly teardown (mirrors GameMain's). The process itself is ended by
// the activity (System.exit) so the next launch starts fresh.
void XrGameBoot_Shutdown();

#endif // __ANDROID__
