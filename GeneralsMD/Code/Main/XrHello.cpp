// GeneralsX @spike XR port Phase 0.4 - minimal native OpenXR hello loop,
// extended in Phase 1.0 to boot the full game offscreen (XrGameBoot.cpp)
// and show its frame as a textured tabletop panel, and in Phase 1.1a with
// passthrough + a flat board: the real room shows through, the game lies
// like paper on the user's own table (one-time head-relative latch; exact
// controller placement and explicit recenter come in the input phase).
//
// Each XR frame steps the game once via executeSingleFrame() and samples
// the finished frame (d3d8gles owned FBO) onto a quad. The Phase-0.4
// triangle survives as the boot-failure fallback: triangle in the headset
// with no game panel means "XR works, the engine didn't boot -- read the
// xr log". Entry from XrHelloActivity (own launcher icon, own process).
//
// Two deliberate isolations from the game code in this same libmain.so:
//  1. XR-side logging goes to logcat (__android_log_print), NOT stderr:
//     the game redirects stderr to its own log file during boot.
//  2. Every GL call goes through this file's own eglGetProcAddress dispatch
//     (xr_gl*): d3d8gles defines global gl* wrapper symbols whose function
//     pointers stay NULL without game GL init -- a bare gl call from here
//     would segfault. (Calling d3d8gles_* API functions is fine -- only
//     bare gl* is forbidden.)
//
// Shared-context discipline: the game and the quad renderer use one EGL
// context. The quad owns its program/VAO/VBO/FBO and samples on texture
// unit 2 (the game only touches 0-1); after both eyes,
// d3d8gles_InvalidateCachedState() drops the pipeline's CPU-side GL caches
// so the next game draw re-applies program/VAO/state/binds from scratch.
#ifdef __ANDROID__

#define XR_USE_PLATFORM_ANDROID
#define XR_USE_GRAPHICS_API_OPENGL_ES

#include <jni.h>
#include <android/log.h>
#include <unistd.h>

#include <EGL/egl.h>
#include <GLES3/gl3.h>

#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#include "XrGameBoot.h"
#include "XrColor.h"
#include "XrPerformance.h"
#include "XrGpuTimerGL.h"
// Only this one d3d8gles entry point is used here; declared manually
// instead of including d3d8gles.h, which pulls in DXVK's d3d8.h (needs
// the COM compat headers this TU deliberately doesn't include).
extern "C" void d3d8gles_InvalidateCachedState();
extern "C" void d3d8gles_RequireXRFullWorld();

#define GX_XR_TAG "gx-xr"
// Dual logging: logcat (live) AND stderr (lands in generals-xr-stderr.log
// once the engine boot redirects it, so placement diagnostics survive the
// tiny logcat ring buffer and the headset being doffed to type a report).
#define XR_LOG(...) do { \
	__android_log_print(ANDROID_LOG_INFO, GX_XR_TAG, __VA_ARGS__); \
	fprintf(stderr, "[xr] " __VA_ARGS__); \
	fputc('\n', stderr); \
} while (0)
#define XR_LOGE(...) do { \
	__android_log_print(ANDROID_LOG_ERROR, GX_XR_TAG, __VA_ARGS__); \
	fprintf(stderr, "[xr] ERROR: " __VA_ARGS__); \
	fputc('\n', stderr); \
} while (0)

// Only used inside initXr(), where failure means "return false" (a goto
// would jump over std::vector initializations -- clang rejects that).
#define XR_CHECK(call, what)                                               \
	do {                                                               \
		const XrResult _r = (call);                                \
		if (!XR_SUCCEEDED(_r)) {                                   \
			XR_LOGE("%s failed: %d", (what), (int)_r);         \
			return false;                                      \
		}                                                          \
	} while (0)

// ---------------------------------------------------------------------------
// Own GLES dispatch (see the file comment: never call bare gl* from here).
// ---------------------------------------------------------------------------
static PFNGLCLEARCOLORPROC xr_glClearColor = nullptr;
static PFNGLCLEARPROC xr_glClear = nullptr;
static PFNGLVIEWPORTPROC xr_glViewport = nullptr;
static PFNGLGENFRAMEBUFFERSPROC xr_glGenFramebuffers = nullptr;
static PFNGLBINDFRAMEBUFFERPROC xr_glBindFramebuffer = nullptr;
static PFNGLFRAMEBUFFERTEXTURE2DPROC xr_glFramebufferTexture2D = nullptr;
static PFNGLCHECKFRAMEBUFFERSTATUSPROC xr_glCheckFramebufferStatus = nullptr;
static PFNGLDELETEFRAMEBUFFERSPROC xr_glDeleteFramebuffers = nullptr;
static PFNGLCREATESHADERPROC xr_glCreateShader = nullptr;
static PFNGLSHADERSOURCEPROC xr_glShaderSource = nullptr;
static PFNGLCOMPILESHADERPROC xr_glCompileShader = nullptr;
static PFNGLGETSHADERIVPROC xr_glGetShaderiv = nullptr;
static PFNGLGETSHADERINFOLOGPROC xr_glGetShaderInfoLog = nullptr;
static PFNGLCREATEPROGRAMPROC xr_glCreateProgram = nullptr;
static PFNGLATTACHSHADERPROC xr_glAttachShader = nullptr;
static PFNGLLINKPROGRAMPROC xr_glLinkProgram = nullptr;
static PFNGLGETPROGRAMIVPROC xr_glGetProgramiv = nullptr;
static PFNGLGETPROGRAMINFOLOGPROC xr_glGetProgramInfoLog = nullptr;
static PFNGLUSEPROGRAMPROC xr_glUseProgram = nullptr;
static PFNGLGETATTRIBLOCATIONPROC xr_glGetAttribLocation = nullptr;
static PFNGLGETUNIFORMLOCATIONPROC xr_glGetUniformLocation = nullptr;
static PFNGLUNIFORMMATRIX4FVPROC xr_glUniformMatrix4fv = nullptr;
static PFNGLGENBUFFERSPROC xr_glGenBuffers = nullptr;
static PFNGLBINDBUFFERPROC xr_glBindBuffer = nullptr;
static PFNGLBUFFERDATAPROC xr_glBufferData = nullptr;
static PFNGLENABLEVERTEXATTRIBARRAYPROC xr_glEnableVertexAttribArray = nullptr;
static PFNGLVERTEXATTRIBPOINTERPROC xr_glVertexAttribPointer = nullptr;
static PFNGLDRAWARRAYSPROC xr_glDrawArrays = nullptr;
static PFNGLDELETEBUFFERSPROC xr_glDeleteBuffers = nullptr;
static PFNGLDELETEPROGRAMPROC xr_glDeleteProgram = nullptr;
static PFNGLDELETESHADERPROC xr_glDeleteShader = nullptr;
static PFNGLGETERRORPROC xr_glGetError = nullptr;
// Phase 1.0 quad additions: own VAO, texture-unit-2 sampling, explicit
// state disables (the game leaves blend/depth/cull/scissor dirty).
static PFNGLGENVERTEXARRAYSPROC xr_glGenVertexArrays = nullptr;
static PFNGLBINDVERTEXARRAYPROC xr_glBindVertexArray = nullptr;
static PFNGLDELETEVERTEXARRAYSPROC xr_glDeleteVertexArrays = nullptr;
static PFNGLDISABLEVERTEXATTRIBARRAYPROC xr_glDisableVertexAttribArray = nullptr;
static PFNGLBINDTEXTUREPROC xr_glBindTexture = nullptr;
static PFNGLACTIVETEXTUREPROC xr_glActiveTexture = nullptr;
static PFNGLUNIFORM1IPROC xr_glUniform1i = nullptr;
static PFNGLDISABLEPROC xr_glDisable = nullptr;
static PFNGLUNIFORM4FPROC xr_glUniform4f = nullptr;
static PFNGLCOLORMASKPROC xr_glColorMask = nullptr;
static PFNGLGENTEXTURESPROC xr_glGenTextures = nullptr;
static PFNGLTEXIMAGE2DPROC xr_glTexImage2D = nullptr;
static PFNGLTEXPARAMETERIPROC xr_glTexParameteri = nullptr;
static PFNGLDELETETEXTURESPROC xr_glDeleteTextures = nullptr;
static PFNGLPIXELSTOREIPROC xr_glPixelStorei = nullptr;
static PFNGLGENRENDERBUFFERSPROC xr_glGenRenderbuffers = nullptr;
static PFNGLBINDRENDERBUFFERPROC xr_glBindRenderbuffer = nullptr;
static PFNGLRENDERBUFFERSTORAGEPROC xr_glRenderbufferStorage = nullptr;
static PFNGLFRAMEBUFFERRENDERBUFFERPROC xr_glFramebufferRenderbuffer = nullptr;
static PFNGLDELETERENDERBUFFERSPROC xr_glDeleteRenderbuffers = nullptr;
static PFNGLENABLEPROC xr_glEnable = nullptr;
static PFNGLDEPTHMASKPROC xr_glDepthMask = nullptr;
static PFNGLDEPTHFUNCPROC xr_glDepthFunc = nullptr;
static PFNGLCLEARDEPTHFPROC xr_glClearDepthf = nullptr;
static PFNGLBLENDFUNCPROC xr_glBlendFunc = nullptr;

static bool loadGlProcs()
{
	struct Entry { const char *name; void **slot; };
#define GL_ENTRY(n) { #n, reinterpret_cast<void **>(&xr_##n) }
	const Entry table[] = {
		GL_ENTRY(glClearColor), GL_ENTRY(glClear), GL_ENTRY(glViewport),
		GL_ENTRY(glGenFramebuffers), GL_ENTRY(glBindFramebuffer),
		GL_ENTRY(glFramebufferTexture2D), GL_ENTRY(glCheckFramebufferStatus),
		GL_ENTRY(glDeleteFramebuffers), GL_ENTRY(glCreateShader),
		GL_ENTRY(glShaderSource), GL_ENTRY(glCompileShader),
		GL_ENTRY(glGetShaderiv), GL_ENTRY(glGetShaderInfoLog),
		GL_ENTRY(glCreateProgram), GL_ENTRY(glAttachShader),
		GL_ENTRY(glLinkProgram), GL_ENTRY(glGetProgramiv),
		GL_ENTRY(glGetProgramInfoLog), GL_ENTRY(glUseProgram),
		GL_ENTRY(glGetAttribLocation), GL_ENTRY(glGetUniformLocation),
		GL_ENTRY(glUniformMatrix4fv), GL_ENTRY(glGenBuffers),
		GL_ENTRY(glBindBuffer), GL_ENTRY(glBufferData),
		GL_ENTRY(glEnableVertexAttribArray), GL_ENTRY(glVertexAttribPointer),
		GL_ENTRY(glDrawArrays), GL_ENTRY(glDeleteBuffers),
		GL_ENTRY(glDeleteProgram), GL_ENTRY(glDeleteShader),
		GL_ENTRY(glGetError), GL_ENTRY(glGenVertexArrays),
		GL_ENTRY(glBindVertexArray), GL_ENTRY(glDeleteVertexArrays),
		GL_ENTRY(glDisableVertexAttribArray), GL_ENTRY(glBindTexture),
		GL_ENTRY(glActiveTexture), GL_ENTRY(glUniform1i),
		GL_ENTRY(glDisable),
		GL_ENTRY(glUniform4f), GL_ENTRY(glColorMask),
		GL_ENTRY(glGenTextures), GL_ENTRY(glTexImage2D), GL_ENTRY(glTexParameteri), GL_ENTRY(glDeleteTextures),
		GL_ENTRY(glPixelStorei),
		GL_ENTRY(glGenRenderbuffers), GL_ENTRY(glBindRenderbuffer), GL_ENTRY(glRenderbufferStorage),
		GL_ENTRY(glFramebufferRenderbuffer), GL_ENTRY(glDeleteRenderbuffers), GL_ENTRY(glEnable),
		GL_ENTRY(glDepthMask), GL_ENTRY(glDepthFunc), GL_ENTRY(glClearDepthf),
		GL_ENTRY(glBlendFunc),
	};
#undef GL_ENTRY
	for (size_t i = 0; i < sizeof(table) / sizeof(table[0]); i++) {
		*table[i].slot = reinterpret_cast<void *>(eglGetProcAddress(table[i].name));
		if (*table[i].slot == nullptr) {
			XR_LOGE("eglGetProcAddress failed: %s", table[i].name);
			return false;
		}
	}
	return true;
}

// ---------------------------------------------------------------------------
// Column-major 4x4 math (OpenXR right-handed, -Z forward, +Y up; GLES NDC).
// ---------------------------------------------------------------------------
#include "XrMath.h"
#include "XrControls.h"
#include "XrLayout.h"
#include "XrWorkspacePlacement.h"
#include "XrTracking.h"
#include "XrDiorama.h"
#include "XrMenu.h"
#include "XrCommands.h"
#include "XrBuildRotation.h"
#include "XrScene.h"
#include "XrReferenceSpace.h"

// Durable on-device check for this file's matrix math (no test harness
// exists in this repo, so the check rides along every launch instead).
// Fails the boot loudly rather than rendering a misplaced world.
static bool selfCheckMath()
{
	const XrPosef identity = {{0.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 0.0f}}; // orientation, then position
	float m[16];
	matViewFromPose(m, identity);
	for (int i = 0; i < 16; i++) {
		const float want = (i % 5 == 0) ? 1.0f : 0.0f;
		if (fabsf(m[i] - want) > 1e-5f) {
			XR_LOGE("math self-check FAILED: identity view m[%d]=%f", i, m[i]);
			return false;
		}
	}
	float fx = 0.0f, fz = 0.0f;
	yawForwardFromQuat(identity.orientation, &fx, &fz);
	if (fabsf(fx) > 1e-5f || fabsf(fz + 1.0f) > 1e-5f) {
		XR_LOGE("math self-check FAILED: identity yaw-forward=(%f,%f)", fx, fz);
		return false;
	}
	// Check actual frustum-edge output; an identity view alone cannot detect
	// a wrong perspective scale (the original nearZ factor passed it).
	const XrFovf fov = {-0.8f, 0.7f, 0.75f, -0.65f};
	matPerspectiveFromFov(m, fov, 0.05f, 100.0f);
	const float rightNdc = m[0] * tanf(fov.angleRight) - m[8];
	const float topNdc = m[5] * tanf(fov.angleUp) - m[9];
	if (fabsf(rightNdc - 1.0f) > 1e-5f || fabsf(topNdc - 1.0f) > 1e-5f) {
		XR_LOGE("math self-check FAILED: frustum edges=(%f,%f)", rightNdc, topNdc);
		return false;
	}
	XR_LOG("math self-check PASS: frustum edges=(%.3f,%.3f), projection-v2", rightNdc, topNdc);
	return true;
}

// ---------------------------------------------------------------------------
// Triangle (1m, fixed at head height 2m ahead -- look straight to see it).
// The per-eye clear colors (blue left / red right) prove stereo submission
// in every gaze direction even if the triangle is off-view.
// ---------------------------------------------------------------------------
static const char *kVertShader =
	"#version 300 es\n"
	"uniform mat4 uMVP;\n"
	"in vec4 aPos;\n"
	"out vec3 vColor;\n"
	"void main() {\n"
	"  gl_Position = uMVP * aPos;\n"
	"  vColor = vec3(aPos.x + 0.5, aPos.y + 0.5, 0.6);\n"
	"}\n";

static const char *kFragShader =
	"#version 300 es\n"
	"precision mediump float;\n"
	"in vec3 vColor;\n"
	"out vec4 oColor;\n"
	"void main() { oColor = vec4(vColor, 1.0); }\n";

static GLuint compileShader(GLenum type, const char *src)
{
	const GLuint sh = xr_glCreateShader(type);
	xr_glShaderSource(sh, 1, &src, nullptr);
	xr_glCompileShader(sh);
	GLint ok = 0;
	xr_glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
	if (!ok) {
		char log[512];
		xr_glGetShaderInfoLog(sh, sizeof(log), nullptr, log);
		XR_LOGE("shader compile failed: %s", log);
		xr_glDeleteShader(sh);
		return 0;
	}
	return sh;
}

// ---------------------------------------------------------------------------
// Phase 1.0 game panel: a tilted quad sampling the game frame texture.
// UVs map 1:1 (v=0 at the quad's bottom = texture row 0 = the game image's
// bottom row -- GL-native, no flip needed in the shader).
// ---------------------------------------------------------------------------
static const char *kQuadVertShader =
	"#version 300 es\n"
	"uniform mat4 uMVP;\n"
	"in vec4 aPos;\n"
	"in vec2 aUV;\n"
	"out highp vec2 vUV;\n"
	"void main() {\n"
	"  gl_Position = uMVP * aPos;\n"
	"  vUV = aUV;\n"
	"}\n";

static const char *kQuadFragShader =
	"#version 300 es\n"
	"precision mediump float;\n"
	"uniform sampler2D uTex;\n"
	"uniform highp sampler2DArray uStereoArray;\n"
	"uniform int uArrayEye;\n" // -1 for conventional 2D panels; otherwise array layer.
	"uniform vec4 uPointer;\n" // u, v, visible, pressed
	"uniform highp vec4 uUVRect;\n"
	"uniform int uLayer;\n"
	"in highp vec2 vUV;\n"
	"out vec4 oColor;\n"
	XR_DISPLAY_TO_LINEAR_GLSL
	"void main() {\n"
	// GeneralsX @feature Codex 14/09/2026 Texture-free editing outline. The
	// center discards, so this never redraws or obscures the game world.
	"  if (uLayer == 7 || uLayer == 8 || uLayer == 9) {\n"
	"    vec2 edge=min(vUV,1.0-vUV);\n"
	"    if(edge.x>uPointer.x && edge.y>uPointer.y) discard;\n"
	"    vec3 color=uLayer==8 ? vec3(0.15,0.95,1.0):uLayer==9 ? vec3(1.0,0.2,0.15):vec3(1.0,166.0/255.0,31.0/255.0);\n"
	"    oColor=vec4(xrDisplayToLinear(color),1.0); return;\n"
	"  }\n"
	"  if (uLayer == 4 || uLayer == 5) {\n"
	"    float r=length(vUV-vec2(0.5)); if(uLayer==4 && (r>0.48 || (r<0.32 && r>0.10))) discard;\n"
	"    vec3 rayColor=mix(vec3(0.55),vec3(0.15,0.95,1.0),uPointer.z);\n"
	"    oColor=vec4(xrDisplayToLinear(mix(rayColor,vec3(1.0,0.65,0.12),uPointer.w)),1.0); return;\n"
	"  }\n"
	"  highp vec2 texSize = uArrayEye>=0 ? vec2(textureSize(uStereoArray,0).xy):vec2(textureSize(uTex,0));\n"
	"  vec2 size = texSize*uUVRect.zw;\n"
	// GeneralsX @bugfix Codex 14/09/2026 Clamp each atlas eye to its own
	// texel centers: linear filtering must not sample the other eye.
	"  highp vec2 uv = uUVRect.xy+vUV*uUVRect.zw;\n"
	"  if(uLayer==3) { highp vec2 halfTexel=.5/texSize;\n"
	"    uv=clamp(uv,uUVRect.xy+halfTexel,uUVRect.xy+uUVRect.zw-halfTexel); }\n"
	"  vec4 sampleColor = uArrayEye>=0 ? texture(uStereoArray,vec3(uv,float(uArrayEye))):texture(uTex, uv);\n"
	"  vec3 color = sampleColor.rgb;\n"
	"  float alpha=1.0;\n"
	"  if (uLayer == 2 || uLayer == 3) {\n"
	"    alpha=sampleColor.a;\n"
	"    if (alpha < 0.001) discard;\n"
	"    color/=alpha;\n"
	"  }\n"
	"  if (uLayer == 6) { alpha=sampleColor.a; if(alpha<0.001) discard; }\n"
	"  if (uLayer == 1) color += vec3(0.025,0.035,0.045)*(1.0-sampleColor.a);\n"
	"  vec2 edge = min(vUV, 1.0-vUV) * size;\n"
	"  if (uLayer < 2 && min(edge.x, edge.y) < 2.0) color = vec3(0.15,0.55,0.60);\n"
	"  if (uPointer.z > 0.5) {\n"
	"    float d = length((vUV-uPointer.xy)*size);\n"
	"    if (d < 9.0) { color = vec3(0.02); alpha=1.0; }\n"
	"    if (d < 7.0 && d > 4.0) color = mix(vec3(0.85,1.0,1.0),vec3(1.0,0.65,0.15),uPointer.w);\n"
	"    if (d < 1.5) color = vec3(1.0);\n"
	"  }\n"
	"  oColor = vec4(xrDisplayToLinear(color)*alpha, alpha);\n"
	"}\n";

// GeneralsX @feature Quest tabletop recovery 13/09/2026 Presentation is
// state-dependent. Intro/shell content is a large upright screen at a
// comfortable reading distance. Only an interactive match lies flat as a
// command table. Geometry is unit-width and the model matrix applies these
// physical sizes, so switching modes never rebuilds the VBO.
static constexpr float kShellPanelWidthM = 1.35f;
static constexpr float kShellPanelAheadM = 1.10f;
static constexpr float kShellPanelBelowHeadM = 0.02f;
static constexpr float kShellPanelPitchRad = 0.0f;
static constexpr float kGamePanelWidthM = 1.10f;
static constexpr float kGamePanelAheadM = 0.70f;
static constexpr float kGamePanelBelowHeadM = 0.45f;
static constexpr float kGamePanelPitchRad = -3.14159265f * 0.5f;

static std::atomic<bool> g_stopFlag(false);

struct XrHello {
	XrInstance instance = XR_NULL_HANDLE;
	XrSession session = XR_NULL_HANDLE;
	XrSpace localSpace = XR_NULL_HANDLE;
	XrSessionState state = XR_SESSION_STATE_UNKNOWN;
	bool sessionRunning = false;
	EGLDisplay eglDisplay = EGL_NO_DISPLAY;
	EGLConfig eglConfig = nullptr;
	EGLContext eglContext = EGL_NO_CONTEXT;
	EGLSurface eglSurface = EGL_NO_SURFACE;
	uint32_t viewWidth = 0, viewHeight = 0;
	uint32_t maxSwapchainWidth = 0, maxSwapchainHeight = 0;
	uint32_t maxLayerCount = 0;
	struct ViewSwapchain {
		XrSwapchain handle = XR_NULL_HANDLE;
		std::vector<XrSwapchainImageOpenGLESKHR> images;
	};
	ViewSwapchain swapchains[2];
	int64_t swapchainFormat = 0;
	GLuint fbo = 0;
	GLuint panelDepth = 0;
	GLuint program = 0;
	GLuint vbo = 0;
	GLint uMVP = -1;
	GLint aPos = -1;
	// Phase 1.0 game panel: own program/VAO/VBO, sized to the game frame
	// aspect. Only built when the engine booted (gameBooted).
	bool gameBooted = false;
	GLuint quadProgram = 0;
	GLuint quadVAO = 0;
	GLuint quadVBO = 0;
	GLint qMVP = -1;
	GLint qTex = -1,qStereoArray=-1,qArrayEye=-1;
	GLint qPos = -1;
	GLint qUV = -1;
	GLint qPointer = -1;
	GLint qUVRect = -1, qLayer = -1;
	XrPointerRoute pointerRoute;
	XrBuildRotation buildRotation;
	XrScene scene; jobject activityRef=nullptr;
	XrReferenceChanges referenceChanges;bool roomPoseLost=false,headTrackingLost=false;
	GLuint sceneTexture=0;std::string sceneKey;
	JNIEnv *panelEnv=nullptr;jclass panelPainter=nullptr;
	XrMenuState menu;XrCommandState commands;float worldZoom=1.0f;bool startViewApplied=false;
	GLuint commandsTexture=0,commandButtonTexture=0;std::string commandsKey;
	GLuint uiButtonTexture=0,settingsTexture=0,hoverTexture=0;
	GLuint recoveryTexture=0;
	bool recoveryVisible=false;
	// GeneralsX @feature Muse 16/09/2026 Match-result card: head-yaw
	// billboard texture, refreshed from the read-only end-state latch.
	GLuint resultTexture=0;
	std::string resultKey;
	XrSurface resultSurface{};
	bool resultVisible=false,resultPressHeld=false;
	bool loadingPresentation=false;
	std::string settingsKey,hoverKey,hoverCandidate;XrTime hoverSince=0;
	bool hoverVisible=false;
	int pointerPiece=-1;
	bool splitVisible=false,expandedUI=false;
	bool diorama=false, dioramaReady=false;
	bool stereoWorld=false,stereoVisible=false;
	XrWorldClick worldClick,worldCancel;
	XrVector3f worldCursor={};bool worldCursorVisible=false;
	XrVector3f rayStart={},rayEnd={};bool rayVisible=false,rayHit=false;
	GLuint dioramaProgram=0,dioramaVAO=0,dioramaVBO=0;
	GLint dioramaMVP=-1;
	GLsizei dioramaVertices=0;
	int arrangeSlot=1;
	XrControls controls;
	bool pointerVisible = false, pointerPressed = false;
	float pointerU = 0.5f, pointerV = 0.5f;
	bool keyHeld[5] = {};
	bool inputArmed = false;
	XrTime nextZoomTime = 0;
	bool uprightGame = false;
	XrLayout layout;
	XrPerformance performance;XrGpuTimer gpuTimer;
	XrSurface surfaces[3];
	XrPosef layoutAnchor = {{0,0,0,1},{0,0,0}};
	bool anchorKnown = false, arranging = false, layoutDirty = false, controlsArmed = false;
	XrSurfaceGrab grab;
	XrTime previousInputTime = 0;
	int cameraPreset = 1;
	bool cameraPending = true, cameraCustom = false;
	bool cameraSaveFailed = false;
	// Phase 1.1a passthrough (XR_FB_passthrough, optional): the real room
	// shows through wherever the projection layer is transparent, so the
	// flat board reads as lying on the user's own table.
	bool passthroughActive = false;
	XrPassthroughFB passthrough = XR_NULL_HANDLE;
	XrPassthroughLayerFB passthroughLayer = XR_NULL_HANDLE;
	// Head-relative panel placement (one-time latch): LOCAL-space origin
	// and head height vary per session (sitting/standing, recenter,
	// launch-while-on-table), so a fixed panel lands ~1m off for some
	// users. The target is computed from the live head pose on first sight,
	// then remains fixed in LOCAL space. It must not follow the headset when
	// the player leans, walks, or takes it off.
	bool panelLatched = false;
	bool presentationKnown = false;
	bool interactiveGame = false;
	float latchYaw = 0.0f;
	float panelPos[3] = {0.0f, 1.15f, -1.1f};
	float panelTarget[3] = {0.0f, 1.15f, -1.1f};
	float panelModel[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
	unsigned frame = 0;
	// Last-frame diagnostics for the periodic log line.
	bool lastFrameShouldRender = false;
	XrResult lastLocateResult = XR_SUCCESS;
	uint32_t lastViewCount = 0;
};

static bool initEgl(XrHello &x)
{
	x.eglDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
	if (x.eglDisplay == EGL_NO_DISPLAY) {
		XR_LOGE("eglGetDisplay failed");
		return false;
	}
	EGLint major = 0, minor = 0;
	if (!eglInitialize(x.eglDisplay, &major, &minor)) {
		XR_LOGE("eglInitialize failed");
		return false;
	}
	const EGLint cfgAttribs[] = {
		EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
		EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
		EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8,
		EGL_DEPTH_SIZE, 0, EGL_STENCIL_SIZE, 0,
		EGL_NONE,
	};
	EGLint cfgCount = 0;
	if (!eglChooseConfig(x.eglDisplay, cfgAttribs, &x.eglConfig, 1, &cfgCount) || cfgCount < 1) {
		XR_LOGE("eglChooseConfig failed");
		return false;
	}
	const EGLint ctxAttribs[] = { EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE };
	x.eglContext = eglCreateContext(x.eglDisplay, x.eglConfig, EGL_NO_CONTEXT, ctxAttribs);
	if (x.eglContext == EGL_NO_CONTEXT) {
		XR_LOGE("eglCreateContext failed: 0x%x", eglGetError());
		return false;
	}
	const EGLint pbAttribs[] = { EGL_WIDTH, 16, EGL_HEIGHT, 16, EGL_NONE };
	x.eglSurface = eglCreatePbufferSurface(x.eglDisplay, x.eglConfig, pbAttribs);
	if (x.eglSurface == EGL_NO_SURFACE) {
		XR_LOGE("eglCreatePbufferSurface failed: 0x%x", eglGetError());
		return false;
	}
	if (!eglMakeCurrent(x.eglDisplay, x.eglSurface, x.eglSurface, x.eglContext)) {
		XR_LOGE("eglMakeCurrent failed: 0x%x", eglGetError());
		return false;
	}
	XR_LOG("EGL ready: %d.%d", (int)major, (int)minor);
	return true;
}

static bool hasExtension(const char *name)
{
	uint32_t count = 0;
	if (!XR_SUCCEEDED(xrEnumerateInstanceExtensionProperties(nullptr, 0, &count, nullptr)) || count == 0)
		return false;
	std::vector<XrExtensionProperties> props(count, { XR_TYPE_EXTENSION_PROPERTIES, nullptr });
	if (!XR_SUCCEEDED(xrEnumerateInstanceExtensionProperties(nullptr, count, &count, props.data())))
		return false;
	for (uint32_t i = 0; i < count; i++) {
		if (strcmp(props[i].extensionName, name) == 0)
			return true;
	}
	return false;
}

static bool initXr(XrHello &x, JavaVM *vm, jobject activity)
{
	// Loader init (Android): must precede every other xr call.
	PFN_xrInitializeLoaderKHR initLoader = nullptr;
	XrResult r = xrGetInstanceProcAddr(XR_NULL_HANDLE, "xrInitializeLoaderKHR",
		reinterpret_cast<PFN_xrVoidFunction *>(&initLoader));
	if (!XR_SUCCEEDED(r) || initLoader == nullptr) {
		XR_LOGE("xrGetInstanceProcAddr(xrInitializeLoaderKHR) failed: %d", (int)r);
		return false;
	}
	XrLoaderInitInfoAndroidKHR loaderInfo = { XR_TYPE_LOADER_INIT_INFO_ANDROID_KHR, nullptr };
	loaderInfo.applicationVM = vm;
	loaderInfo.applicationContext = activity;
	XR_CHECK(initLoader(reinterpret_cast<const XrLoaderInitInfoBaseHeaderKHR *>(&loaderInfo)),
		"xrInitializeLoaderKHR");

	const char *wantExts[] = {
		XR_KHR_ANDROID_CREATE_INSTANCE_EXTENSION_NAME,
		XR_KHR_OPENGL_ES_ENABLE_EXTENSION_NAME,
	};
	for (size_t i = 0; i < sizeof(wantExts) / sizeof(wantExts[0]); i++) {
		if (!hasExtension(wantExts[i])) {
			XR_LOGE("missing instance extension: %s", wantExts[i]);
			return false;
		}
	}
	// Phase 1.1a: passthrough is OPTIONAL (graceful opaque fallback keeps
	// the board usable where the extension is missing).
	const bool wantPassthrough = hasExtension(XR_FB_PASSTHROUGH_EXTENSION_NAME);
	// GeneralsX @feature Codex 14/09/2026 Scene access is optional and XR-only.
	std::vector<const char *> allExts={wantExts[0],wantExts[1]};
	if(wantPassthrough)allExts.push_back(XR_FB_PASSTHROUGH_EXTENSION_NAME);
	const char *sceneExts[]={XR_FB_SPATIAL_ENTITY_EXTENSION_NAME,XR_FB_SPATIAL_ENTITY_STORAGE_EXTENSION_NAME,XR_FB_SPATIAL_ENTITY_QUERY_EXTENSION_NAME,XR_FB_SCENE_EXTENSION_NAME};
	bool wantScene=true;for(const char *name:sceneExts)wantScene=wantScene && hasExtension(name);
	if(wantScene)for(const char *name:sceneExts)allExts.push_back(name);
	const bool wantCapture=wantScene && hasExtension(XR_FB_SCENE_CAPTURE_EXTENSION_NAME);
	if(wantCapture)allExts.push_back(XR_FB_SCENE_CAPTURE_EXTENSION_NAME);
	XR_LOG("passthrough extension: %s", wantPassthrough ? "present" : "MISSING (opaque fallback)");

	XrInstanceCreateInfoAndroidKHR androidInfo = { XR_TYPE_INSTANCE_CREATE_INFO_ANDROID_KHR, nullptr };
	androidInfo.applicationVM = vm;
	androidInfo.applicationActivity = activity;
	XrInstanceCreateInfo ici = { XR_TYPE_INSTANCE_CREATE_INFO, &androidInfo };
	// GeneralsX @feature Codex 14/09/2026 Public XR product identity.
	strncpy(ici.applicationInfo.applicationName, "Generals: Zero Hour XR",
		sizeof(ici.applicationInfo.applicationName) - 1);
	ici.applicationInfo.applicationVersion = 1;
	strncpy(ici.applicationInfo.engineName, "GeneralsX",
		sizeof(ici.applicationInfo.engineName) - 1);
	ici.applicationInfo.engineVersion = 1;
	ici.applicationInfo.apiVersion = XR_CURRENT_API_VERSION;
	ici.enabledExtensionCount = uint32_t(allExts.size());
	ici.enabledExtensionNames = allExts.data();
	XR_CHECK(xrCreateInstance(&ici, &x.instance), "xrCreateInstance");
	x.scene.init(x.instance,wantScene,wantCapture);
	XR_LOG("P19 scene=%d capture=%d",int(x.scene.available),int(wantCapture));

	XrInstanceProperties iprops = { XR_TYPE_INSTANCE_PROPERTIES, nullptr };
	XR_CHECK(xrGetInstanceProperties(x.instance, &iprops), "xrGetInstanceProperties");
	XR_LOG("runtime: %s", iprops.runtimeName);

	XrSystemGetInfo sgi = { XR_TYPE_SYSTEM_GET_INFO, nullptr };
	sgi.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
	XrSystemId systemId = XR_NULL_SYSTEM_ID;
	XR_CHECK(xrGetSystem(x.instance, &sgi, &systemId), "xrGetSystem");
	XrSystemProperties systemProps = { XR_TYPE_SYSTEM_PROPERTIES, nullptr };
	XR_CHECK(xrGetSystemProperties(x.instance, systemId, &systemProps),
		"xrGetSystemProperties");
	x.maxSwapchainWidth = systemProps.graphicsProperties.maxSwapchainImageWidth;
	x.maxSwapchainHeight = systemProps.graphicsProperties.maxSwapchainImageHeight;
	x.maxLayerCount = systemProps.graphicsProperties.maxLayerCount;
	XR_LOG("system: id=%llu name=%s vendor=%u maxSwapchain=%ux%u maxLayers=%u tracking(position=%d orientation=%d)",
		(unsigned long long)systemId,
		systemProps.systemName,
		systemProps.vendorId,
		x.maxSwapchainWidth,
		x.maxSwapchainHeight,
		x.maxLayerCount,
		(int)systemProps.trackingProperties.positionTracking,
		(int)systemProps.trackingProperties.orientationTracking);
	if (x.maxLayerCount < 2) {
		XR_LOGE("runtime exposes only %u composition layer(s); hybrid tabletop will require fallback",
			x.maxLayerCount);
	}

	// Stereo view config: recommended per-eye size.
	uint32_t viewCount = 0;
	XR_CHECK(xrEnumerateViewConfigurationViews(x.instance, systemId,
		XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 0, &viewCount, nullptr),
		"xrEnumerateViewConfigurationViews(count)");
	if (viewCount != 2) {
		XR_LOGE("expected 2 stereo views, got %u", viewCount);
		return false;
	}
	std::vector<XrViewConfigurationView> vcviews(viewCount, { XR_TYPE_VIEW_CONFIGURATION_VIEW, nullptr });
	XR_CHECK(xrEnumerateViewConfigurationViews(x.instance, systemId,
		XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, viewCount, &viewCount, vcviews.data()),
		"xrEnumerateViewConfigurationViews");
	x.viewWidth = vcviews[0].recommendedImageRectWidth;
	x.viewHeight = vcviews[0].recommendedImageRectHeight;
	XR_LOG("per-eye: %ux%u", x.viewWidth, x.viewHeight);

	if (!initEgl(x))
		return false;
	if (!loadGlProcs())
		return false;

	// GLES requirements check.
	PFN_xrGetOpenGLESGraphicsRequirementsKHR getGlesReqs = nullptr;
	XR_CHECK(xrGetInstanceProcAddr(x.instance, "xrGetOpenGLESGraphicsRequirementsKHR",
		reinterpret_cast<PFN_xrVoidFunction *>(&getGlesReqs)),
		"xrGetInstanceProcAddr(xrGetOpenGLESGraphicsRequirementsKHR)");
	XrGraphicsRequirementsOpenGLESKHR reqs = { XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_ES_KHR, nullptr };
	XR_CHECK(getGlesReqs(x.instance, systemId, &reqs), "xrGetOpenGLESGraphicsRequirementsKHR");
	XR_LOG("GLES reqs: min %d.%d", (int)XR_VERSION_MAJOR(reqs.minApiVersionSupported),
		(int)XR_VERSION_MINOR(reqs.minApiVersionSupported));

	XrGraphicsBindingOpenGLESAndroidKHR binding = { XR_TYPE_GRAPHICS_BINDING_OPENGL_ES_ANDROID_KHR, nullptr };
	binding.display = x.eglDisplay;
	binding.config = x.eglConfig;
	binding.context = x.eglContext;
	XrSessionCreateInfo sci = { XR_TYPE_SESSION_CREATE_INFO, &binding };
	sci.systemId = systemId;
	XR_CHECK(xrCreateSession(x.instance, &sci, &x.session), "xrCreateSession");

	XrReferenceSpaceCreateInfo rsci = { XR_TYPE_REFERENCE_SPACE_CREATE_INFO, nullptr };
	rsci.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
	rsci.poseInReferenceSpace.orientation.w = 1.0f;
	XR_CHECK(xrCreateReferenceSpace(x.session, &rsci, &x.localSpace), "xrCreateReferenceSpace(LOCAL)");
	if (!initControls(x.controls, x.instance, x.session)) return false;

	// Phase 1.1a passthrough: full-room reconstruction layer, submitted
	// below the projection layer. Any failure here is NON-fatal (opaque
	// fallback) -- but each step logs, so a missing room is diagnosable.
	if (wantPassthrough) {
		PFN_xrCreatePassthroughFB pCreate = nullptr;
		PFN_xrPassthroughStartFB pStart = nullptr;
		PFN_xrCreatePassthroughLayerFB pLayerCreate = nullptr;
		PFN_xrPassthroughLayerResumeFB pLayerResume = nullptr;
		xrGetInstanceProcAddr(x.instance, "xrCreatePassthroughFB",
			reinterpret_cast<PFN_xrVoidFunction *>(&pCreate));
		xrGetInstanceProcAddr(x.instance, "xrPassthroughStartFB",
			reinterpret_cast<PFN_xrVoidFunction *>(&pStart));
		xrGetInstanceProcAddr(x.instance, "xrCreatePassthroughLayerFB",
			reinterpret_cast<PFN_xrVoidFunction *>(&pLayerCreate));
		xrGetInstanceProcAddr(x.instance, "xrPassthroughLayerResumeFB",
			reinterpret_cast<PFN_xrVoidFunction *>(&pLayerResume));
		if (!pCreate || !pStart || !pLayerCreate || !pLayerResume) {
			XR_LOGE("passthrough entry points missing, opaque fallback");
		} else {
			XrPassthroughCreateInfoFB pci = { XR_TYPE_PASSTHROUGH_CREATE_INFO_FB, nullptr };
			pci.flags = 0;
			XrPassthroughLayerCreateInfoFB plci = { XR_TYPE_PASSTHROUGH_LAYER_CREATE_INFO_FB, nullptr };
			plci.flags = 0;
			plci.purpose = XR_PASSTHROUGH_LAYER_PURPOSE_RECONSTRUCTION_FB;
			if (!XR_SUCCEEDED(pCreate(x.session, &pci, &x.passthrough))) {
				XR_LOGE("xrCreatePassthroughFB failed, opaque fallback");
				x.passthrough = XR_NULL_HANDLE;
			} else if (!XR_SUCCEEDED(pStart(x.passthrough))) {
				XR_LOGE("xrPassthroughStartFB failed, opaque fallback");
			} else {
				plci.passthrough = x.passthrough;
				if (!XR_SUCCEEDED(pLayerCreate(x.session, &plci, &x.passthroughLayer))) {
					XR_LOGE("xrCreatePassthroughLayerFB failed, opaque fallback");
					x.passthroughLayer = XR_NULL_HANDLE;
				} else if (!XR_SUCCEEDED(pLayerResume(x.passthroughLayer))) {
					XR_LOGE("xrPassthroughLayerResumeFB failed, opaque fallback");
				} else {
					x.passthroughActive = true;
					XR_LOG("passthrough ACTIVE (reconstruction layer)");
				}
			}
		}
	}

	// Swapchain format: sRGB preferred, plain RGBA fallback.
	uint32_t fmtCount = 0;
	XR_CHECK(xrEnumerateSwapchainFormats(x.session, 0, &fmtCount, nullptr),
		"xrEnumerateSwapchainFormats(count)");
	std::vector<int64_t> formats(fmtCount);
	XR_CHECK(xrEnumerateSwapchainFormats(x.session, fmtCount, &fmtCount, formats.data()),
		"xrEnumerateSwapchainFormats");
	x.swapchainFormat = 0;
	for (uint32_t i = 0; i < fmtCount; i++) {
		if (formats[i] == (int64_t)GL_SRGB8_ALPHA8) { x.swapchainFormat = formats[i]; break; }
	}
	if (x.swapchainFormat == 0) {
		for (uint32_t i = 0; i < fmtCount; i++) {
			if (formats[i] == (int64_t)GL_RGBA8) { x.swapchainFormat = formats[i]; break; }
		}
	}
	if (x.swapchainFormat == 0) {
		XR_LOGE("no RGBA8/sRGB swapchain format (first offered: 0x%llx)",
			(unsigned long long)(fmtCount ? formats[0] : 0));
		return false;
	}
	XR_LOG("swapchain format: 0x%llx", (unsigned long long)x.swapchainFormat);

	for (int eye = 0; eye < 2; eye++) {
		XrSwapchainCreateInfo swci = { XR_TYPE_SWAPCHAIN_CREATE_INFO, nullptr };
		swci.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
		swci.format = x.swapchainFormat;
		swci.sampleCount = 1;
		swci.width = x.viewWidth;
		swci.height = x.viewHeight;
		swci.faceCount = 1;
		swci.arraySize = 1;
		swci.mipCount = 1;
		XR_CHECK(xrCreateSwapchain(x.session, &swci, &x.swapchains[eye].handle),
			"xrCreateSwapchain");
		uint32_t imgCount = 0;
		XR_CHECK(xrEnumerateSwapchainImages(x.swapchains[eye].handle, 0, &imgCount, nullptr),
			"xrEnumerateSwapchainImages(count)");
		x.swapchains[eye].images.resize(imgCount,
			{ XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_ES_KHR, nullptr });
		XR_CHECK(xrEnumerateSwapchainImages(x.swapchains[eye].handle, imgCount, &imgCount,
			reinterpret_cast<XrSwapchainImageBaseHeader *>(x.swapchains[eye].images.data())),
			"xrEnumerateSwapchainImages");
		XR_LOG("eye %d: %u swapchain images", eye, imgCount);
	}

	// Render target FBO (reused per eye), triangle program + VBO.
	xr_glGenFramebuffers(1, &x.fbo);
	// GeneralsX @feature Codex 13/09/2026 Depth-test both movable panels consistently with ray picking.
	xr_glGenRenderbuffers(1,&x.panelDepth);
	xr_glBindRenderbuffer(GL_RENDERBUFFER,x.panelDepth);
	xr_glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH_COMPONENT24,x.viewWidth,x.viewHeight);
	const GLuint vs = compileShader(GL_VERTEX_SHADER, kVertShader);
	const GLuint fs = compileShader(GL_FRAGMENT_SHADER, kFragShader);
	if (vs == 0 || fs == 0)
		return false;
	x.program = xr_glCreateProgram();
	xr_glAttachShader(x.program, vs);
	xr_glAttachShader(x.program, fs);
	xr_glLinkProgram(x.program);
	GLint linked = 0;
	xr_glGetProgramiv(x.program, GL_LINK_STATUS, &linked);
	if (!linked) {
		char log[512];
		xr_glGetProgramInfoLog(x.program, sizeof(log), nullptr, log);
		XR_LOGE("program link failed: %s", log);
		return false;
	}
	xr_glDeleteShader(vs);
	xr_glDeleteShader(fs);
	x.uMVP = xr_glGetUniformLocation(x.program, "uMVP");
	x.aPos = xr_glGetAttribLocation(x.program, "aPos");
	static const float verts[] = {
		-0.5f, -0.4f, 0.0f,
		 0.5f, -0.4f, 0.0f,
		 0.0f,  0.5f, 0.0f,
	};
	xr_glGenBuffers(1, &x.vbo);
	xr_glBindBuffer(GL_ARRAY_BUFFER, x.vbo);
	xr_glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
	xr_glBindBuffer(GL_ARRAY_BUFFER, 0);
	XR_LOG("XR init complete");
	return true;
}

static void pollEvents(XrHello &x, bool &quit)
{
	XrEventDataBuffer ev = { XR_TYPE_EVENT_DATA_BUFFER, nullptr };
	while (xrPollEvent(x.instance, &ev) == XR_SUCCESS) {
		x.scene.event(x.session,ev);
		if (ev.type == XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED) {
			const auto *sc =
				reinterpret_cast<const XrEventDataSessionStateChanged *>(&ev);
			if (sc->session != x.session)
				continue;
			x.state = sc->state;
			if(x.state!=XR_SESSION_STATE_FOCUSED && x.scene.placing) {
				x.scene.cancel();x.menu.open=true;x.menu.page=5;x.controlsArmed=false;
				x.scene.message="Platzierung unterbrochen; Vorschau neu starten";
			}
			XR_LOG("session state: %d", (int)x.state);
			if (x.state == XR_SESSION_STATE_READY && !x.sessionRunning) {
				XrSessionBeginInfo bi = { XR_TYPE_SESSION_BEGIN_INFO, nullptr };
				bi.primaryViewConfigurationType =
					XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
				if (XR_SUCCEEDED(xrBeginSession(x.session, &bi))) {
					x.sessionRunning = true;
					XR_LOG("session begun");
				} else {
					XR_LOGE("xrBeginSession failed");
				}
			} else if (x.state == XR_SESSION_STATE_STOPPING && x.sessionRunning) {
				xrEndSession(x.session);
				x.sessionRunning = false;
				XR_LOG("session ended");
			} else if (x.state == XR_SESSION_STATE_EXITING ||
			           x.state == XR_SESSION_STATE_LOSS_PENDING) {
				quit = true;
			}
		} else if (ev.type == XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING) {
			quit = true;
		} else if (ev.type == XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING) {
			const auto *change = reinterpret_cast<const XrEventDataReferenceSpaceChangePending *>(&ev);
			if (change->session == x.session && change->referenceSpaceType == XR_REFERENCE_SPACE_TYPE_LOCAL) {
				x.referenceChanges.enqueue(*change);
				XR_LOG("P19.1 reference change queued valid=%d",int(change->poseValid));
			}
		}
		ev.type = XR_TYPE_EVENT_DATA_BUFFER;
	}
}

static bool buildQuadResources(XrHello &x)
{
	const int gw = XrGameBoot_GameWidth();
	const int gh = XrGameBoot_GameHeight();
	const float halfW = 0.5f;
	const float halfH = halfW * (float)gh / (float)gw;
	// Two triangles: (x, y, z, u, v), v=0 along the bottom edge.
	const float verts[] = {
		-halfW, -halfH, 0.0f,  0.0f, 0.0f,
		 halfW, -halfH, 0.0f,  1.0f, 0.0f,
		 halfW,  halfH, 0.0f,  1.0f, 1.0f,
		-halfW, -halfH, 0.0f,  0.0f, 0.0f,
		 halfW,  halfH, 0.0f,  1.0f, 1.0f,
		-halfW,  halfH, 0.0f,  0.0f, 1.0f,
	};
	const GLuint vs = compileShader(GL_VERTEX_SHADER, kQuadVertShader);
	const GLuint fs = compileShader(GL_FRAGMENT_SHADER, kQuadFragShader);
	if (vs == 0 || fs == 0)
		return false;
	x.quadProgram = xr_glCreateProgram();
	xr_glAttachShader(x.quadProgram, vs);
	xr_glAttachShader(x.quadProgram, fs);
	xr_glLinkProgram(x.quadProgram);
	GLint linked = 0;
	xr_glGetProgramiv(x.quadProgram, GL_LINK_STATUS, &linked);
	xr_glDeleteShader(vs);
	xr_glDeleteShader(fs);
	if (!linked) {
		char log[512];
		xr_glGetProgramInfoLog(x.quadProgram, sizeof(log), nullptr, log);
		XR_LOGE("quad program link failed: %s", log);
		return false;
	}
	x.qMVP = xr_glGetUniformLocation(x.quadProgram, "uMVP");
	x.qTex = xr_glGetUniformLocation(x.quadProgram, "uTex");
	x.qStereoArray=xr_glGetUniformLocation(x.quadProgram,"uStereoArray");
	x.qArrayEye=xr_glGetUniformLocation(x.quadProgram,"uArrayEye");
	x.qPos = xr_glGetAttribLocation(x.quadProgram, "aPos");
	x.qUV = xr_glGetAttribLocation(x.quadProgram, "aUV");
	x.qPointer = xr_glGetUniformLocation(x.quadProgram, "uPointer");
	x.qUVRect = xr_glGetUniformLocation(x.quadProgram,"uUVRect");
	x.qLayer = xr_glGetUniformLocation(x.quadProgram,"uLayer");
	xr_glGenVertexArrays(1, &x.quadVAO);
	xr_glBindVertexArray(x.quadVAO);
	xr_glGenBuffers(1, &x.quadVBO);
	xr_glBindBuffer(GL_ARRAY_BUFFER, x.quadVBO);
	xr_glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
	xr_glEnableVertexAttribArray((GLuint)x.qPos);
	xr_glVertexAttribPointer((GLuint)x.qPos, 3, GL_FLOAT, GL_FALSE,
		5 * sizeof(float), nullptr);
	xr_glEnableVertexAttribArray((GLuint)x.qUV);
	xr_glVertexAttribPointer((GLuint)x.qUV, 2, GL_FLOAT, GL_FALSE,
		5 * sizeof(float), reinterpret_cast<const void *>(3 * sizeof(float)));
	xr_glBindVertexArray(0);
	xr_glBindBuffer(GL_ARRAY_BUFFER, 0);
	XR_LOG("quad ready: game %dx%d shellWidth=%.2fm tableWidth=%.2fm",
		gw, gh, kShellPanelWidthM, kGamePanelWidthM);
	return true;
}

// State-dependent one-time placement. Shell/intro is an upright display;
// interactive gameplay is a flat table. A state transition re-latches once
// from the then-current head pose, but normal head movement never moves it.
static int activeSurface(const XrHello &x) { return x.diorama ? 1:x.splitVisible ? (x.arranging ? x.arrangeSlot:1):0; }

// GeneralsX @feature Codex 13/09/2026 One static mesh, actual per-eye poses.
static bool buildDioramaResources(XrHello &x) {
	const GLuint vs=compileShader(GL_VERTEX_SHADER,kXrDioramaVert);
	const GLuint fs=compileShader(GL_FRAGMENT_SHADER,kXrDioramaFrag);
	if(!vs || !fs) { if(vs) xr_glDeleteShader(vs); if(fs) xr_glDeleteShader(fs); return false; }
	x.dioramaProgram=xr_glCreateProgram();
	xr_glAttachShader(x.dioramaProgram,vs);xr_glAttachShader(x.dioramaProgram,fs);
	xr_glLinkProgram(x.dioramaProgram);xr_glDeleteShader(vs);xr_glDeleteShader(fs);
	GLint ok=0;xr_glGetProgramiv(x.dioramaProgram,GL_LINK_STATUS,&ok);
	if(!ok) {
		char log[512];xr_glGetProgramInfoLog(x.dioramaProgram,sizeof(log),nullptr,log);
		XR_LOGE("P6 diorama link failed: %s",log);return false;
	}
	x.dioramaMVP=xr_glGetUniformLocation(x.dioramaProgram,"uMVP");
	const auto mesh=xrBuildDiorama();x.dioramaVertices=static_cast<GLsizei>(mesh.vertices.size());
	xr_glGenVertexArrays(1,&x.dioramaVAO);xr_glBindVertexArray(x.dioramaVAO);
	xr_glGenBuffers(1,&x.dioramaVBO);xr_glBindBuffer(GL_ARRAY_BUFFER,x.dioramaVBO);
	xr_glBufferData(GL_ARRAY_BUFFER,mesh.vertices.size()*sizeof(XrDioramaVertex),mesh.vertices.data(),GL_STATIC_DRAW);
	for(GLuint i=0;i<3;++i) {
		xr_glEnableVertexAttribArray(i);
		xr_glVertexAttribPointer(i,3,GL_FLOAT,GL_FALSE,sizeof(XrDioramaVertex),reinterpret_cast<const void *>(i*sizeof(XrVector3f)));
	}
	xr_glBindVertexArray(0);xr_glBindBuffer(GL_ARRAY_BUFFER,0);
	const GLenum error=xr_glGetError();
	if(error!=GL_NO_ERROR) {XR_LOGE("P6 mesh upload error 0x%x",error);return false;}
	XR_LOG("P6 diorama ready: %d triangles, +Z height scales with board; shell LT+Y toggles",x.dioramaVertices/3);
	return true;
}
static XrGameRect surfaceRect(int piece) {
	if(piece==1) return XrGameBoot_WorldRect();
	if(piece>=2) return xrUIPieceRect(piece,XrGameBoot_CommandRect(),XrGameBoot_ExpandedUI());
	return {};
}
static float surfaceAspect(int slot) {
	const float full=float(XrGameBoot_GameHeight())/XrGameBoot_GameWidth();
	const auto r=surfaceRect(slot); return full*r.h/r.w;
}
static XrSurface displayedSurface(const XrHello &x,int piece) {
	XrSurface result=x.surfaces[piece==3 ? 2:piece];
	if(piece>=2) {
		const float full=float(XrGameBoot_GameHeight())/XrGameBoot_GameWidth();
		const float offset=xrUIBandOffset(surfaceRect(piece),XrGameBoot_CommandRect(),full)*result.width;
		result.pose.position=xrAdd(result.pose.position,xrRotate(result.pose.orientation,{0,offset,0}));
	}
	return result;
}
static void placePanel(XrHello &x, const XrView *views)
{
	if (xrInitializeWorkspace(x.anchorKnown,x.layout,x.surfaces,x.layoutAnchor,views)) {
		XR_LOG("P3 layout anchored to current head heading");
	}
	const int slot=activeSurface(x);
	surfaceMatrix(x.surfaces[slot],x.panelModel);
	const auto &p=x.surfaces[slot].pose.position;
	x.panelPos[0]=p.x; x.panelPos[1]=p.y; x.panelPos[2]=p.z;
	x.panelLatched=true;
}

// GeneralsX @feature Codex 13/09/2026 Refresh a persistent ray pointer each
// focused frame, with balanced releases and re-arming after tracking loss.
#include "XrInput.h"

static void saveLayout(XrHello &x);
static bool xrSceneMenuAction(XrHello &,int);
static void xrSceneMenuText(const XrHello &,std::string &,char *,size_t);
#include "XrMenuUI.h"
#include "XrCommandUI.h"
#include "XrMenuPainting.h"
#include "XrInteraction.h"
#include "XrSceneUI.h"

// GeneralsX @bugfix Codex 15/09/2026 Movies and gameplay consume origin changes
// before rendering with current eye poses, exactly once through the same path.
static void applyWorkspaceReference(XrHello &x,const XrView *views,XrTime time) {
	if(!x.anchorKnown)x.referenceChanges.adoptCurrentOrigin(time);
	placePanel(x,views);
	const int rebase=x.referenceChanges.apply(time,x.surfaces,x.layoutAnchor,x.menu.surface);
	if(!rebase)return;
	x.grab.cancel();x.controlsArmed=false;x.inputArmed=false;x.buildRotation={};
	x.menu.click.cancel();x.commands.input.click.cancel();XrGameBoot_CancelTarget();
	updateControls(x,XrControllerState{},time);
	if(x.scene.placing){x.scene.cancel();x.menu.open=true;x.menu.page=5;}
	if(rebase==2) {
		x.roomPoseLost=true;x.scene.step=XrScene::Step::Choice;
		x.layoutAnchor=xrWorkspaceHeading(views);
		x.menu.surface.pose=xrPoseMul(x.layoutAnchor,{{0,0,0,1},{0,-.16f,-.9f}});
		x.menu.surface.width=.64f;x.menu.open=true;x.menu.page=5;
		// The upright movie/shell surface is also unsafe after an unknown origin.
		// Recover it in front of the user while the tabletop awaits confirmation.
		const XrLayout defaults;x.surfaces[0]=defaults.relative[0];
		x.surfaces[0].pose=xrPoseMul(x.layoutAnchor,x.surfaces[0].pose);
	}
	placePanel(x,views);
	XR_LOG("P20.2 reference change applied result=%d movie=%d",rebase,int(x.loadingPresentation));
}


static bool renderEye(XrHello &x, int eye, const XrPosef &pose, const XrFovf &fov)
{
	XrSwapchainImageAcquireInfo acquireInfo = { XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO, nullptr };
	uint32_t imgIndex = 0;
	XrResult r = xrAcquireSwapchainImage(x.swapchains[eye].handle, &acquireInfo, &imgIndex);
	if (!XR_SUCCEEDED(r)) {
		XR_LOGE("xrAcquireSwapchainImage(eye %d) failed: %d", eye, (int)r);
		return false;
	}
	XrSwapchainImageWaitInfo waitInfo = { XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO, nullptr };
	waitInfo.timeout = XR_INFINITE_DURATION;
	r = xrWaitSwapchainImage(x.swapchains[eye].handle, &waitInfo);
	if (!XR_SUCCEEDED(r)) {
		XR_LOGE("xrWaitSwapchainImage(eye %d) failed: %d", eye, (int)r);
		return false;
	}

	const GLuint tex = x.swapchains[eye].images[imgIndex].image;
	xr_glBindFramebuffer(GL_FRAMEBUFFER, x.fbo);
	xr_glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
	xr_glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,x.panelDepth);
	if (xr_glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
		XR_LOGE("eye %d FBO incomplete (glerr 0x%x)", eye, xr_glGetError());
		return false;
	}
	xr_glViewport(0, 0, (GLsizei)x.viewWidth, (GLsizei)x.viewHeight);
	// Clear must ignore the game's scissor and color-write mask too.
	xr_glDisable(GL_SCISSOR_TEST);
	xr_glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	// Transparent clear: with passthrough active the real room shows
	// through everywhere except the board (which writes alpha 1). Without
	// passthrough (opaque fallback) the background is plain black.
	// (The Phase-0.4 blue/red per-eye tints proved eye order then; stereo
	// is long established, transparency matters more now.)
	xr_glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	xr_glDepthMask(GL_TRUE); xr_glClearDepthf(1);
	xr_glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	float proj[16], view[16], model[16], viewModel[16], mvp[16];
	matPerspectiveFromFov(proj, fov, 0.05f, 100.0f);
	matViewFromPose(view, pose);
	if (x.gameBooted && x.quadProgram != 0) {
		// Game panel: world-locked quad (placePanel() in runLoop keeps
		// x.panelModel current), sampling the live game frame on texture
		// unit 2. Every piece of fixed-function state this draw depends
		// on is disabled explicitly -- the game leaves all of it dirty
		// (blend for UI, depth for 3D, scissor from clears, cull from
		// models).
		xr_glEnable(GL_DEPTH_TEST); xr_glDepthFunc(GL_LEQUAL);
		xr_glDisable(GL_BLEND);
		xr_glDisable(GL_CULL_FACE);
		xr_glDisable(GL_STENCIL_TEST);
		xr_glDisable(GL_SCISSOR_TEST);
		xr_glDisable(GL_POLYGON_OFFSET_FILL);
		xr_glUseProgram(x.quadProgram);
		xr_glActiveTexture(GL_TEXTURE2);
		xr_glUniform1i(x.qTex, 2);
		xr_glUniform1i(x.qStereoArray,3);xr_glUniform1i(x.qArrayEye,-1);
		xr_glBindVertexArray(x.quadVAO);
		const float aspect=float(XrGameBoot_GameHeight())/XrGameBoot_GameWidth();
		const bool hideWorkspace=x.scene.placing || x.roomPoseLost;
		if(x.stereoVisible && !hideWorkspace) {
			// The texture is already projected for this exact eye pose/FOV.
			// UI remains a conventional overlay until cross-layer depth is integrated.
			matScale(mvp,2,2/aspect,1);xr_glUniformMatrix4fv(x.qMVP,1,GL_FALSE,mvp);
			const bool atlas=XrGameBoot_StereoAtlas();
			xr_glUniform4f(x.qUVRect,atlas ? eye*.5f:0,0,atlas ? .5f:1,1);xr_glUniform1i(x.qLayer,3);xr_glUniform4f(x.qPointer,0,0,0,0);
			xr_glDisable(GL_DEPTH_TEST);xr_glDepthMask(GL_FALSE);
			xr_glEnable(GL_BLEND);xr_glBlendFunc(GL_ONE,GL_ONE_MINUS_SRC_ALPHA);
			const bool multiview=XrGameBoot_StereoMultiview();
			if(multiview) {
				xr_glActiveTexture(GL_TEXTURE3);xr_glBindTexture(GL_TEXTURE_2D_ARRAY,XrGameBoot_StereoTexture(eye));
				xr_glUniform1i(x.qArrayEye,eye);
			} else xr_glBindTexture(GL_TEXTURE_2D,XrGameBoot_StereoTexture(eye));
			xr_glDrawArrays(GL_TRIANGLES,0,6);
			if(multiview) {
				xr_glBindTexture(GL_TEXTURE_2D_ARRAY,0);xr_glActiveTexture(GL_TEXTURE2);xr_glUniform1i(x.qArrayEye,-1);
			}
			xr_glEnable(GL_DEPTH_TEST);xr_glDepthMask(GL_TRUE);xr_glDisable(GL_BLEND);
		}
		for(int slot=x.splitVisible ? 1:0;slot<=(x.splitVisible ? 3:0);++slot) {
			if(hideWorkspace)break;
			if(x.diorama) break;
			if(x.recoveryVisible) break; // Never sample the incomplete composed frame.
			if(surfaceRect(slot).h<=0)continue;
			if(x.stereoVisible && slot==1) continue;
			float base[16],scale[16]; surfaceMatrix(displayedSurface(x,slot),base);
			matScale(scale,1,surfaceAspect(slot)/aspect,1); matMultiply(model,base,scale);
			matMultiply(viewModel,view,model); matMultiply(mvp,proj,viewModel);
			xr_glUniformMatrix4fv(x.qMVP,1,GL_FALSE,mvp);
			const auto rect=surfaceRect(slot);
			xr_glUniform4f(x.qUVRect,rect.x,rect.y,rect.w,rect.h);
			xr_glUniform1i(x.qLayer,slot==3 ? 2:slot==2 ? 1:0);
			if(slot==3) { xr_glEnable(GL_BLEND); xr_glBlendFunc(GL_ONE,GL_ONE_MINUS_SRC_ALPHA); }
			else xr_glDisable(GL_BLEND);
			xr_glBindTexture(GL_TEXTURE_2D,slot==1 ? XrGameBoot_WorldTexture():slot>=2 ? XrGameBoot_UITexture():XrGameBoot_GameTexture());
			xr_glUniform4f(x.qPointer,x.pointerU,x.pointerV,x.pointerVisible && x.pointerPiece==slot ? 1.0f:0.0f,
				x.pointerPressed ? 1.0f:0.0f);
			xr_glDrawArrays(GL_TRIANGLES,0,6);
		}
		if(x.diorama && x.dioramaReady && !hideWorkspace) {
			xrDioramaMatrix(x.surfaces[1],model);
			matMultiply(viewModel,view,model);matMultiply(mvp,proj,viewModel);
			xr_glDisable(GL_BLEND);xr_glUseProgram(x.dioramaProgram);
			xr_glUniformMatrix4fv(x.dioramaMVP,1,GL_FALSE,mvp);
			xr_glBindVertexArray(x.dioramaVAO);
			xr_glDrawArrays(GL_TRIANGLES,0,x.dioramaVertices);
			xr_glUseProgram(x.quadProgram);xr_glBindVertexArray(x.quadVAO);
		}
		// P8 smooth on-demand panels replace the permanent instruction strip.
		// GeneralsX @feature Codex 14/09/2026 Highlight the exact selected
		// physical surface, including the dynamically expanded native UI crop.
		const int editing=xrEditTarget(x);
		if(editing>=0 && editing<=2 && !x.loadingPresentation && !x.recoveryVisible &&
			!x.diorama && x.panelLatched && surfaceRect(editing).h>0) {
			float outlineAspect=surfaceAspect(editing);
			const auto outline=xrEditOutline(displayedSurface(x,editing),outlineAspect,editing==1);
			float base[16],scale[16];surfaceMatrix(outline,base);
			matScale(scale,1,outlineAspect/aspect,1);matMultiply(model,base,scale);
			matMultiply(viewModel,view,model);matMultiply(mvp,proj,viewModel);
			xr_glUniformMatrix4fv(x.qMVP,1,GL_FALSE,mvp);xr_glUniform1i(x.qLayer,7);
			xr_glUniform4f(x.qPointer,.004f/outline.width,.004f/(outline.width*outlineAspect),0,0);
			xr_glDisable(GL_DEPTH_TEST);xr_glDepthMask(GL_FALSE);xr_glDisable(GL_BLEND);
			xr_glDrawArrays(GL_TRIANGLES,0,6);
		}
		// GeneralsX @feature Codex 14/09/2026 Show detected planes even when
		// the proposed board does not fit. Cyan surface, orange valid, red invalid.
		if(x.scene.placing)for(const auto &face:x.scene.visibleFaces) {
			const size_t count=face.boundary.size(),stride=std::max<size_t>(1,(count+127)/128);
			for(size_t i=0;i<count;i+=stride) {
				const auto a=face.boundary[i],b=face.boundary[(i+stride<count ? i+stride:0)];
				const float dx=b.x-a.x,dy=b.y-a.y,len=sqrtf(dx*dx+dy*dy);
				if(len<.001f)continue;
				XrSurface edge;edge.width=len;
				edge.pose=xrPoseMul(face.pose,{xrAxisAngle({0,0,1},atan2f(dy,dx)),{(a.x+b.x)*.5f,(a.y+b.y)*.5f,.003f}});
				float base[16],scale[16];surfaceMatrix(edge,base);
				matScale(scale,1,.005f/(len*aspect),1);matMultiply(model,base,scale);
				matMultiply(viewModel,view,model);matMultiply(mvp,proj,viewModel);
				xr_glUniformMatrix4fv(x.qMVP,1,GL_FALSE,mvp);xr_glUniform1i(x.qLayer,8);
				xr_glUniform4f(x.qPointer,1,1,0,0);
				xr_glDisable(GL_DEPTH_TEST);xr_glDepthMask(GL_FALSE);xr_glDisable(GL_BLEND);
				xr_glDrawArrays(GL_TRIANGLES,0,6);
			}
		}
		if(x.scene.placing && x.scene.detected) {
			const auto &s=x.scene.candidate;
			float base[16],scale[16];surfaceMatrix(s,base);
			matScale(scale,1,surfaceAspect(1)/aspect,1);matMultiply(model,base,scale);
			matMultiply(viewModel,view,model);matMultiply(mvp,proj,viewModel);
			xr_glUniformMatrix4fv(x.qMVP,1,GL_FALSE,mvp);xr_glUniform1i(x.qLayer,x.scene.preview ? 7:9);
			xr_glUniform4f(x.qPointer,.008f/s.width,.008f/(s.width*surfaceAspect(1)),0,0);
			xr_glDisable(GL_DEPTH_TEST);xr_glDepthMask(GL_FALSE);xr_glDisable(GL_BLEND);
			xr_glDrawArrays(GL_TRIANGLES,0,6);
		}
		auto panel=[&](const XrSurface &s,float panelAspect,GLuint texture) {
			if(!texture) return;
			float base[16],scale[16];surfaceMatrix(s,base);matScale(scale,1,panelAspect/aspect,1);
			matMultiply(model,base,scale);matMultiply(viewModel,view,model);matMultiply(mvp,proj,viewModel);
			xr_glUniformMatrix4fv(x.qMVP,1,GL_FALSE,mvp);xr_glUniform4f(x.qPointer,0,0,0,0);
			xr_glUniform4f(x.qUVRect,0,0,1,1);xr_glUniform1i(x.qLayer,6);
			xr_glDisable(GL_DEPTH_TEST);xr_glDepthMask(GL_FALSE);xr_glEnable(GL_BLEND);xr_glBlendFunc(GL_ONE,GL_ONE_MINUS_SRC_ALPHA);
			xr_glBindTexture(GL_TEXTURE_2D,texture);xr_glDrawArrays(GL_TRIANGLES,0,6);
		};
		if(x.scene.placing)panel(x.menu.surface,.5f,x.sceneTexture);
		if(x.recoveryVisible)panel(x.surfaces[0],.5f,x.recoveryTexture);
		if(x.hoverVisible && !x.menu.open && !x.recoveryVisible && !x.scene.placing) panel(hoverCardSurface(x),1.0f,x.hoverTexture);
		if(commandsAvailable(x) && !hideWorkspace) {
			if(x.layout.commandsVisible)panel(commandSurface(x),float(xrCommandHeight(x.commands))/768,x.commandsTexture);
			panel(commandButtonSurface(x),128.0f/192,x.commandButtonTexture);
		}
		if(!x.loadingPresentation && !hideWorkspace)panel(uiButtonSurface(x),128.0f/192,x.uiButtonTexture);
		if(x.menu.open) panel(x.menu.surface,float(kXrMenuHeight)/kXrMenuWidth,x.settingsTexture);
		// GeneralsX @feature Muse 16/09/2026 Match-result card last: the
		// head-yaw billboard stays readable above every other panel.
		if(x.resultVisible && !x.loadingPresentation) panel(x.resultSurface,.5f,x.resultTexture);
		xr_glDisable(GL_BLEND);xr_glEnable(GL_DEPTH_TEST);xr_glDepthMask(GL_TRUE);
		// P7.4 Always show the tracked right aim, including misses. UI is an
		// overlay, so the pointer is drawn last and stops at its selected surface.
		if(x.rayVisible && xrRayRibbon(model,x.rayStart,x.rayEnd,pose.position,aspect)) {
			xr_glDisable(GL_DEPTH_TEST);xr_glDepthMask(GL_FALSE);xr_glDisable(GL_BLEND);
			matMultiply(viewModel,view,model);matMultiply(mvp,proj,viewModel);
			xr_glUniformMatrix4fv(x.qMVP,1,GL_FALSE,mvp);xr_glUniform1i(x.qLayer,5);
			xr_glUniform4f(x.qPointer,0,0,x.rayHit ? 1:0,x.pointerPressed ? 1:0);
			xr_glDrawArrays(GL_TRIANGLES,0,6);
			if(x.rayHit) {
				XrSurface cursor;cursor.pose={pose.orientation,x.rayEnd};cursor.width=.014f;
				float cm[16],cs[16];surfaceMatrix(cursor,cm);matScale(cs,1,1/aspect,1);
				matMultiply(model,cm,cs);matMultiply(viewModel,view,model);matMultiply(mvp,proj,viewModel);
				xr_glUniformMatrix4fv(x.qMVP,1,GL_FALSE,mvp);xr_glUniform1i(x.qLayer,4);
				xr_glDrawArrays(GL_TRIANGLES,0,6);
			}
			xr_glEnable(GL_DEPTH_TEST);xr_glDepthMask(GL_TRUE);
		}
		xr_glBindVertexArray(0);
		xr_glUseProgram(0);
	} else {
		matTranslate(model, 0.0f, 1.5f, -2.0f);
		matMultiply(viewModel, view, model);
		matMultiply(mvp, proj, viewModel);
		xr_glUseProgram(x.program);
		xr_glUniformMatrix4fv(x.uMVP, 1, GL_FALSE, mvp);
		xr_glBindBuffer(GL_ARRAY_BUFFER, x.vbo);
		xr_glEnableVertexAttribArray((GLuint)x.aPos);
		xr_glVertexAttribPointer((GLuint)x.aPos, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
		xr_glDrawArrays(GL_TRIANGLES, 0, 3);
		xr_glBindBuffer(GL_ARRAY_BUFFER, 0);
		xr_glUseProgram(0);
	}

	XrSwapchainImageReleaseInfo releaseInfo = { XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO, nullptr };
	r = xrReleaseSwapchainImage(x.swapchains[eye].handle, &releaseInfo);
	if (!XR_SUCCEEDED(r)) {
		XR_LOGE("xrReleaseSwapchainImage(eye %d) failed: %d", eye, (int)r);
		return false;
	}
	return true;
}

#include "XrLoadingFrame.h"

// GeneralsX @bugfix Codex 14/09/2026 Campaign loading is synchronous in the
// original engine. Its display callback owns fresh XR frames while it runs.
// No executeSingleFrame, game update, world orders or layout saves occur here.
struct XrLoadingPresenter {
	XrHello &x;XrTime outerTime;bool &quit;XrLoadingFrame frame;
	XrLoadingPresenter(XrHello &host,XrTime time,bool &stop):x(host),outerTime(time),quit(stop) {
		XrGameBoot_SetLoadingPresenter([](void *context){static_cast<XrLoadingPresenter *>(context)->present();},this);
	}
	~XrLoadingPresenter() {
		XrGameBoot_SetLoadingPresenter(nullptr,nullptr);
		x.loadingPresentation=false;
		if(frame.consumed)XrGameBoot_Key(XrGameKey::Back,false);
	}
	void present() {
		// GeneralsX @performance Codex 14/09/2026 Never time a nested movie
		// presenter as gameplay, nor leave a timer active across its XR waits.
		x.gpuTimer.end(false);x.performance.invalidate();
		XrFrameState state={XR_TYPE_FRAME_STATE,nullptr};
		XrCompositionLayerProjection projection={XR_TYPE_COMPOSITION_LAYER_PROJECTION,nullptr};
		projection.layerFlags=XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
		projection.space=x.localSpace;
		XrCompositionLayerProjectionView eyeLayers[2]={};
		XrCompositionLayerPassthroughFB passthrough={XR_TYPE_COMPOSITION_LAYER_PASSTHROUGH_FB,nullptr};
		passthrough.flags=XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
		passthrough.layerHandle=x.passthroughLayer;
		const XrCompositionLayerBaseHeader *layers[2]={};uint32_t count=0;
		const bool ok=frame.pump([&] {
			XR_LOG("P11.1 loading presenter: conventional video panel, outer frame yielded");
			// End the already-begun outer frame exactly once. It must not later
			// submit stale eye poses or an incomplete game-world capture.
			XrFrameEndInfo end={XR_TYPE_FRAME_END_INFO,nullptr};end.displayTime=outerTime;
			end.environmentBlendMode=XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
			const bool ended=XR_SUCCEEDED(xrEndFrame(x.session,&end));
			x.splitVisible=false;x.stereoVisible=false;x.diorama=false;
			x.loadingPresentation=true;x.previousInputTime=0;
			x.menu.open=false;x.arranging=false;x.grab.cancel();x.controlsArmed=false;x.inputArmed=false;
			x.menu.click.cancel();x.commands.input.click.cancel();
			x.rayVisible=false;x.pointerVisible=false;x.hoverVisible=false;
			updateControls(x,XrControllerState{},outerTime);
			return ended;
		},[&] {
			pollEvents(x,quit);
			if(g_stopFlag.load())quit=true;
			if(quit || !x.sessionRunning) {
				// The original movie abort check consumes Escape; do not run
				// simulation recursively or hold an XR frame while STOPPING.
				XrGameBoot_Key(XrGameKey::Back,true);return false;
			}
			return true;
		},[&] {
			XrFrameWaitInfo wait={XR_TYPE_FRAME_WAIT_INFO,nullptr};
			if(!XR_SUCCEEDED(xrWaitFrame(x.session,&wait,&state)))return false;
			XrFrameBeginInfo begin={XR_TYPE_FRAME_BEGIN_INFO,nullptr};
			return XR_SUCCEEDED(xrBeginFrame(x.session,&begin));
		},[&] {
			const bool focused=x.state==XR_SESSION_STATE_FOCUSED && state.shouldRender;
			const auto controls=pollControls(x.controls,x.session,x.localSpace,state.predictedDisplayTime,focused,x.layout.leftHanded);
			XrGameBoot_Key(XrGameKey::Back,frame.skip(focused && controls.aimValid,controls.back || controls.select));
			if(!state.shouldRender || (x.state!=XR_SESSION_STATE_VISIBLE && x.state!=XR_SESSION_STATE_FOCUSED))return true;
			if(x.passthroughActive)layers[count++]=reinterpret_cast<const XrCompositionLayerBaseHeader *>(&passthrough);
			XrView views[2]={{XR_TYPE_VIEW,nullptr},{XR_TYPE_VIEW,nullptr}};
			XrViewLocateInfo locate={XR_TYPE_VIEW_LOCATE_INFO,nullptr};
			locate.viewConfigurationType=XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
			locate.displayTime=state.predictedDisplayTime;locate.space=x.localSpace;
			XrViewState viewState={XR_TYPE_VIEW_STATE,nullptr};uint32_t viewCount=0;
			const auto located=xrLocateViews(x.session,&locate,&viewState,2,&viewCount,views);
			if(!XR_SUCCEEDED(located) || viewCount!=2 || !xrTrackedViews(viewState.viewStateFlags))return true;
			// GeneralsX @bugfix Codex 14/09/2026 Nested movie frames also respect
			// P17 completeness; a fresh movie Present clears a prior recovery card.
			x.recoveryVisible=XrGameBoot_GameTexture()==0;
			if(x.recoveryVisible) {d3d8gles_RequireXRFullWorld();updateMenuTextures(x,state.predictedDisplayTime);}
			applyWorkspaceReference(x,views,state.predictedDisplayTime);
			bool drawn=true;
			for(int eye=0;eye<2 && drawn;++eye)drawn=renderEye(x,eye,views[eye].pose,views[eye].fov);
			d3d8gles_InvalidateCachedState();
			if(!drawn)return false;
			for(int eye=0;eye<2;++eye) {
				auto &layer=eyeLayers[eye];layer.type=XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW;
				layer.pose=views[eye].pose;layer.fov=views[eye].fov;
				layer.subImage.swapchain=x.swapchains[eye].handle;
				layer.subImage.imageRect.extent={(int32_t)x.viewWidth,(int32_t)x.viewHeight};
			}
			projection.viewCount=2;projection.views=eyeLayers;
			layers[count++]=reinterpret_cast<const XrCompositionLayerBaseHeader *>(&projection);
			return true;
		},[&] {
			XrFrameEndInfo end={XR_TYPE_FRAME_END_INFO,nullptr};end.displayTime=state.predictedDisplayTime;
			end.environmentBlendMode=XR_ENVIRONMENT_BLEND_MODE_OPAQUE;end.layerCount=count;end.layers=layers;
			const bool ended=XR_SUCCEEDED(xrEndFrame(x.session,&end));
			++x.frame;if(x.frame%120==0)XR_LOG("P11.1 loading XR frame %u",x.frame);
			return ended;
		});
		if(!ok) {XR_LOGE("P11.1 loading presentation failed");quit=true;XrGameBoot_Key(XrGameKey::Back,true);}
	}
};

static void runLoop(XrHello &x)
{
	XR_LOG("P12 GPU timer: %s (optional, measurements off initially)",xrInitGpuTimer(x.gpuTimer) ? "EXT_disjoint_timer_query":"unavailable");
	bool quit = false;
	XrView views[2] = { { XR_TYPE_VIEW, nullptr }, { XR_TYPE_VIEW, nullptr } };
	int idleFrames = 0;
	while (!quit && !g_stopFlag.load()) {
		pollEvents(x, quit);
		if (quit || g_stopFlag.load())
			break;
		if (!x.sessionRunning) {
			// No READY within ~30s (e.g. launched outside VR): give up with
			// a log instead of spinning forever.
			if(x.scene.capturing || x.scene.permissionPending)idleFrames=0;
			else if (++idleFrames > 600) {
				XR_LOGE("no session READY after ~30s, quitting");
				break;
			}
			usleep(50 * 1000);
			continue;
		}
		idleFrames = 0;

		XrFrameWaitInfo waitInfo = { XR_TYPE_FRAME_WAIT_INFO, nullptr };
		XrFrameState frameState = { XR_TYPE_FRAME_STATE, nullptr };
		const double perfFrameStart=xrPerfNow();
		if (!XR_SUCCEEDED(xrWaitFrame(x.session, &waitInfo, &frameState)))
			break;
		const double perfWait=xrPerfNow()-perfFrameStart;
		bool perfMeasured=false;double perfEngine=0,perfEyes=0;
		XrFrameBeginInfo beginInfo = { XR_TYPE_FRAME_BEGIN_INFO, nullptr };
		if (!XR_SUCCEEDED(xrBeginFrame(x.session, &beginInfo)))
			break;

		const bool shouldRender =
			x.state == XR_SESSION_STATE_VISIBLE || x.state == XR_SESSION_STATE_FOCUSED;
		XrCompositionLayerProjection layer = { XR_TYPE_COMPOSITION_LAYER_PROJECTION, nullptr };
		// The eye buffers are transparent outside the game board so the
		// passthrough underlay can show through those pixels.
		layer.layerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
		XrCompositionLayerProjectionView layerViews[2];
		// Passthrough goes FIRST (bottom); the game projection blends over
		// it wherever its alpha is > 0 (the board; the clear is alpha 0).
		XrCompositionLayerPassthroughFB ptLayer = { XR_TYPE_COMPOSITION_LAYER_PASSTHROUGH_FB, nullptr };
		ptLayer.flags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
		ptLayer.space = XR_NULL_HANDLE;
		ptLayer.layerHandle = x.passthroughLayer;
		const XrCompositionLayerBaseHeader *layers[2] = {
			reinterpret_cast<const XrCompositionLayerBaseHeader *>(&ptLayer),
			reinterpret_cast<const XrCompositionLayerBaseHeader *>(&layer),
		};
		uint32_t layerCount = 0;
		uint32_t layerBase = 0;
		if (x.passthroughActive) {
			layerCount = 1; // passthrough submits even on frames the game skips
			layerBase = 1;
		}
		x.lastFrameShouldRender = frameState.shouldRender;
		x.lastViewCount = 0;
		const XrControllerState controls = pollControls(x.controls,x.session,x.localSpace,
			frameState.predictedDisplayTime,x.state==XR_SESSION_STATE_FOCUSED && frameState.shouldRender,x.layout.leftHanded);
		if (x.gameBooted && (!shouldRender || !frameState.shouldRender)) {
			if(x.scene.placing) {x.scene.cancel();x.menu.open=true;x.menu.page=5;}
			x.performance.invalidate();
			updateControls(x,XrControllerState{},frameState.predictedDisplayTime);
			x.grab.cancel(); x.controlsArmed=false; x.previousInputTime=0;
		}
		if (shouldRender && frameState.shouldRender) {
			// Phase 1.0: one game frame per XR frame, rendered into the
			// owned FBO before the eyes sample it. The game only steps
			// while the session actually renders (pausing with the
			// headset instead of burning battery in the background).
			XrViewLocateInfo locateInfo = { XR_TYPE_VIEW_LOCATE_INFO, nullptr };
			locateInfo.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
			locateInfo.displayTime = frameState.predictedDisplayTime;
			locateInfo.space = x.localSpace;
			XrViewState viewState = { XR_TYPE_VIEW_STATE, nullptr };
			uint32_t viewCount = 0;
			x.lastLocateResult = xrLocateViews(x.session, &locateInfo, &viewState,
				2, &viewCount, views);
			x.lastViewCount = viewCount;
				if (XR_SUCCEEDED(x.lastLocateResult) && viewCount == 2 &&
				    xrTrackedViews(viewState.viewStateFlags)) {
					if(x.headTrackingLost){XR_LOG("P20.2 head tracking recovered; workspace retained");x.headTrackingLost=false;}
				if (x.gameBooted) {
					const bool interactiveGame = XrGameBoot_IsInteractiveGame();
					// Never obscure a match started by any asynchronous shell transition.
					if(interactiveGame) x.diorama=false;
					if(!XrGameBoot_CanStereoWorld()) x.stereoWorld=false;
					if (!x.presentationKnown || interactiveGame != x.interactiveGame) {
						x.presentationKnown = true;
						x.interactiveGame = interactiveGame;
						x.startViewApplied=false;
						x.commands.groupOperation=0;x.commands.input.click.cancel();
						saveLayout(x);
						x.grab.cancel(); x.arranging=false; x.controlsArmed=false;
						x.cameraPending=interactiveGame; x.cameraCustom=false; x.cameraSaveFailed=false;
						if(interactiveGame) x.cameraPreset=XrGameBoot_DefaultCameraPreset();
						x.uprightGame = false;
						x.inputArmed = false;
						XR_LOG("presentation mode -> %s",
							x.interactiveGame ? "tabletop-game" : "shell-panel");
					}
					const bool split=XrGameBoot_SplitReady();
					if(split!=x.splitVisible) {
						saveLayout(x); x.splitVisible=split;
						x.grab.cancel(); x.arranging=false; x.controlsArmed=false; x.inputArmed=false;
						XR_LOG("P5 presentation -> %s",split ? "world + detached UI":"composed fallback");
					}
					applyWorkspaceReference(x,views,frameState.predictedDisplayTime);
					// P12.1 Remember intent before the first capture. Cinematic
					// policy controls actual rendering, not the stored view choice.
					xrApplyWorldStartup(x,XrGameBoot_CanStereoWorld());
					// GeneralsX @bugfix Codex 14/09/2026 Geometry changes cannot
					// reinterpret a held build click as a dialog/world command.
					const bool expanded=XrGameBoot_ExpandedUI();
					if(expanded!=x.expandedUI) {
						x.expandedUI=expanded;x.controlsArmed=false;x.inputArmed=false;x.grab.cancel();
						x.commands.input.click.cancel();
					}
					if(x.recoveryVisible) {
						if(x.scene.placing){x.scene.cancel();x.menu.open=true;x.menu.page=5;}
						XrGameBoot_CancelTarget();x.grab.cancel();x.controlsArmed=false;x.inputArmed=false;
						updateControls(x,XrControllerState{},frameState.predictedDisplayTime);
					} else if(!updateScenePlacement(x,controls,frameState.predictedDisplayTime)) {
						if(x.roomPoseLost) {
							x.menu.open=true;x.menu.page=5;
							updateControls(x,XrControllerState{},frameState.predictedDisplayTime);
							updateXrMenu(x,controls,views,frameState.predictedDisplayTime);
						} else updateInteraction(x,controls,views,frameState.predictedDisplayTime);
					}
					// GeneralsX @feature Codex 13/09/2026 Locate before rendering:
					// captured geometry and compositor submission use the SAME eye poses.
					XrWorldFrame world;world.enabled=x.stereoWorld;world.board=x.surfaces[1];
					xrStereoExtent(x.viewWidth,x.viewHeight,world.width,world.height,x.layout.resolutionTier);
					world.coverage=xrMapCoverage(x.worldZoom,x.surfaces[1].width);
					world.healthBars=x.layout.healthBars;world.unitRings=x.layout.unitRings;world.boardFrame=x.layout.boardFrame;
					world.volumeShadows=x.performance.volumeShadows;
					world.atlasStereo=x.performance.atlasStereo;
					world.multiviewStereo=x.performance.multiviewStereo;
					world.elideWorldCopy=x.performance.elideWorldCopy;
					for(int eye=0;eye<2;++eye) {world.eyes[eye]=views[eye].pose;world.fov[eye]=views[eye].fov;}
					XrGameBoot_SetWorldFrame(world);XrGameBoot_SetSplitEnabled(!x.uprightGame);
					// GeneralsX @performance Codex 14/09/2026 Settings changes,
					// focus loss and movies start a fresh, warmed-up measurement epoch.
					char perfKey[192];snprintf(perfKey,sizeof(perfKey),"scene=%s shadows=%s eye=%dx%d coverage=%.4f board=%.4f stereo=%s copy=%s",
						XrGameBoot_PerformanceScene(),world.volumeShadows ? "A-original":"B-light",world.width,world.height,world.coverage,world.board.width,world.multiviewStereo ? "multiview":world.atlasStereo ? "atlas":"reference",world.elideWorldCopy ? "auto":"always");
					perfMeasured=x.performance.prepare(perfKey,x.stereoWorld && x.stereoVisible &&
						x.state==XR_SESSION_STATE_FOCUSED && !x.arranging && XrGameBoot_CanAdjustWorld());
					const auto gpu=x.gpuTimer.poll(x.performance.epoch,x.performance.enabled && !world.multiviewStereo);
					// OVR_multiview makes elapsed GPU queries undefined; CPU/FPS remain valid.
					if(perfMeasured && !world.multiviewStereo){x.performance.gpu.sum+=gpu.totalMs;x.performance.gpu.count+=gpu.count;x.gpuTimer.begin(x.performance.epoch);}
					const double perfEngineStart=xrPerfNow();
					bool loadingConsumed=false;
					{
						XrLoadingPresenter loading(x,frameState.predictedDisplayTime,quit);
						if(!XrGameBoot_Frame()) {XR_LOG("game requested quit, leaving VR loop");quit=true;}
						loadingConsumed=loading.frame.consumed;
					}
					perfEngine=xrPerfNow()-perfEngineStart;
					x.gpuTimer.end(!loadingConsumed && perfEngine<1000);
					if(loadingConsumed) {
						XR_LOG("P11.1 loading returned; next frame reacquires gameplay poses");
						continue; // The callback already ended this outer frame.
					}
					// Movies/dialogs can start during the native frame. Never use
					// the previous frame's crop or show stale stereo captures.
					x.recoveryVisible=xrResolveCapturedView(x,XrGameBoot_SplitReady(),
						XrGameBoot_StereoTexture(0)!=0 && XrGameBoot_StereoTexture(1)!=0,
						XrGameBoot_WorldTexture()!=0,XrGameBoot_GameTexture()!=0);
					if(x.recoveryVisible) {
						d3d8gles_RequireXRFullWorld();x.controlsArmed=false;x.inputArmed=false;
						x.rayVisible=x.hoverVisible=false;
						XR_LOG("P17 late capture loss: suppress incomplete image, full world requested");
					}
					// GeneralsX @feature Muse 16/09/2026 Match-result card: poll
					// the read-only end-state latch, pose the head-yaw
					// billboard, dismiss on any controller press. Debug chords
					// fire retail end actions for short controlled scenarios
					// (absent from release builds); their ordinary input side
					// effects are irrelevant once the match ends.
					XrGameBoot_PollMatchResult();
					x.resultVisible=XrGameBoot_MatchResult()!=XrEndgameResult::None;
					if(x.resultVisible) {
						float fx=0,fz=-1;yawForwardFromQuat(views[0].pose.orientation,&fx,&fz);
						const auto head=xrScale(xrAdd(views[0].pose.position,views[1].pose.position),.5f);
						const auto card=xrEndgameCardPose(head.x,head.y,head.z,fx,fz);
						x.resultSurface.pose.position={card.x,card.y,card.z};
						x.resultSurface.pose.orientation=xrAxisAngle({0,1,0},card.yaw);
						x.resultSurface.width=card.width;
					}
					const bool pressed=controls.select || controls.secondary || controls.back ||
						controls.tilt || controls.buttonsHeld;
					if(x.resultVisible && pressed && !x.resultPressHeld) {
						XrGameBoot_DismissMatchResult();x.resultVisible=false;
					}
					x.resultPressHeld=pressed;
#if defined(RTS_DEBUG) || defined(_ALLOW_DEBUG_CHEATS_IN_RELEASE)
					if(x.gameBooted && x.interactiveGame && controls.grip[0] && controls.grip[1]) {
						if(controls.arrange)XrGameBoot_DebugEndgame(XrDebugEndgame::Victory);
						else if(controls.preset)XrGameBoot_DebugEndgame(XrDebugEndgame::Defeat);
						else if(controls.homeBase)XrGameBoot_DebugEndgame(XrDebugEndgame::QuickVictory);
						else if(controls.upright)XrGameBoot_DebugEndgame(XrDebugEndgame::LocalDefeat);
					}
#endif
					if(!x.stereoVisible || perfEngine>=1000){perfMeasured=false;x.performance.invalidate();}
					if(x.frame%120==0)XR_LOG("P17 presentation: %s requested=%d upright=%d split=%d quality=%s",
						XrGameBoot_PresentationStatus(x.stereoVisible,x.stereoWorld).c_str(),int(x.stereoWorld),int(x.uprightGame),int(x.splitVisible),x.layout.resolutionTier==2 ? "ultra+":x.layout.resolutionTier==1 ? "high":"balanced");
					updateMenuTextures(x,frameState.predictedDisplayTime);
				}
				bool ok = true;
				const double perfEyeStart=xrPerfNow();
				for (int eye = 0; eye < 2 && ok; eye++)
					ok = renderEye(x, eye, views[eye].pose, views[eye].fov);
				perfEyes=xrPerfNow()-perfEyeStart;
				if(!ok)perfMeasured=false;
				// The quad rebound program/VAO/FBO behind the pipeline's
				// back -- drop its GL caches before the next game frame.
				if (x.gameBooted && ok)
					d3d8gles_InvalidateCachedState();
				if (ok) {
					for (int eye = 0; eye < 2; eye++) {
						layerViews[eye].type = XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW;
						layerViews[eye].next = nullptr;
						layerViews[eye].pose = views[eye].pose;
						layerViews[eye].fov = views[eye].fov;
						layerViews[eye].subImage.swapchain = x.swapchains[eye].handle;
						layerViews[eye].subImage.imageRect.offset.x = 0;
						layerViews[eye].subImage.imageRect.offset.y = 0;
						layerViews[eye].subImage.imageRect.extent.width = (int32_t)x.viewWidth;
						layerViews[eye].subImage.imageRect.extent.height = (int32_t)x.viewHeight;
						layerViews[eye].subImage.imageArrayIndex = 0;
					}
					layer.space = x.localSpace;
					layer.viewCount = 2;
					layer.views = layerViews;
					layerCount = layerBase + 1;
				}
			} else if (x.gameBooted) {
				if(!x.headTrackingLost)XR_LOG("P20.2 head tracking lost flags=%llu; input suspended",(unsigned long long)viewState.viewStateFlags);
				x.headTrackingLost=true;XrGameBoot_CancelTarget();x.buildRotation={};
				x.menu.click.cancel();x.commands.input.click.cancel();x.inputArmed=false;
				x.performance.invalidate();
				updateControls(x,XrControllerState{},frameState.predictedDisplayTime);
				x.grab.cancel(); x.controlsArmed=false; x.previousInputTime=0;
			}
		}
		XrFrameEndInfo endInfo = { XR_TYPE_FRAME_END_INFO, nullptr };
		endInfo.displayTime = frameState.predictedDisplayTime;
		// Meta Quest requires OPAQUE even with an XR_FB_passthrough layer;
		// passthrough visibility comes from layer order and source alpha.
		endInfo.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
		endInfo.layerCount = layerCount;
		endInfo.layers = layers + (x.passthroughActive ? 0 : 1);
		if (!XR_SUCCEEDED(xrEndFrame(x.session, &endInfo)))
			break;
		if(perfMeasured) {
			auto &p=x.performance;const double now=xrPerfNow();
			p.engine.add(perfEngine);p.eyes.add(perfEyes);p.wait.add(perfWait);p.frame.add(now-perfFrameStart);
			if(!p.started)p.started=now;
			if(now-p.started>=2000 && p.frame.count) {
				char gpu[96];
				if(p.gpu.count)snprintf(gpu,sizeof(gpu),"GPU %.1f ms",p.gpu.mean());
				else snprintf(gpu,sizeof(gpu),"%s",xrTr(x.performance.multiviewStereo ? "GPU-Zeit: bei Multiview gesperrt":x.gpuTimer.supported ? "GPU wartet / verworfen":"GPU nicht verfügbar"));
				char report[192];snprintf(report,sizeof(report),"%s · CPU %.1f ms · %s",p.volumeShadows ? "A":"B",p.engine.mean(),gpu);p.report=report;
				XR_LOG("P12 perf %s n=%u engineCPU=%.2f eyeCPU=%.2f xrWait=%.2f frame=%.2f maxFrame=%.2f GPUengine=%.2f gpuN=%u gpuSupported=%d disjoint=%u dropped=%u",
					p.key.c_str(),p.frame.count,p.engine.mean(),p.eyes.mean(),p.wait.mean(),p.frame.mean(),p.frame.maximum,
					p.gpu.count ? p.gpu.mean():-1.0,p.gpu.count,int(x.gpuTimer.supported),x.gpuTimer.disjoints,x.gpuTimer.dropped);
				p.engine={};p.eyes={};p.wait={};p.frame={};p.gpu={};p.started=now;
			}
		}
		x.frame++;
		if ((x.frame % 300) == 0) {
			// Panel-center NDC for eye 0: exactly where the panel lands
			// on screen with the live matrices (0,0 = centered).
			float ndcX = 0.0f, ndcY = 0.0f, ndcW = 0.0f;
			if (x.gameBooted && x.lastViewCount == 2) {
				float proj[16], view[16], vm[16], mvp[16];
				matPerspectiveFromFov(proj, views[0].fov, 0.05f, 100.0f);
				matViewFromPose(view, views[0].pose);
				matMultiply(vm, view, x.panelModel);
				matMultiply(mvp, proj, vm);
				ndcW = mvp[3] * 0.0f + mvp[7] * 0.0f + mvp[11] * 0.0f + mvp[15];
				ndcX = (mvp[12] / ndcW);
				ndcY = (mvp[13] / ndcW);
			}
			const XrQuaternionf &q = views[0].pose.orientation;
			// Eye separation sanity: must read ~IPD (0.058-0.070m);
			// ~0 would mean both eyes render the same image (no stereo).
			const float edx = views[0].pose.position.x - views[1].pose.position.x;
			const float edy = views[0].pose.position.y - views[1].pose.position.y;
			const float edz = views[0].pose.position.z - views[1].pose.position.z;
			const float eyeSep = sqrtf(edx * edx + edy * edy + edz * edz);
			XR_LOG("frame %u state=%d fsShouldRender=%d locate=%d views=%u eye0=(%.2f,%.2f,%.2f) q=(%.2f,%.2f,%.2f,%.2f) panel=(%.2f,%.2f,%.2f) ndc=(%.2f,%.2f) eyesep=%.3f",
				x.frame, (int)x.state, (int)x.lastFrameShouldRender,
				(int)x.lastLocateResult, x.lastViewCount,
				views[0].pose.position.x, views[0].pose.position.y,
				views[0].pose.position.z, q.x, q.y, q.z, q.w,
				x.panelPos[0], x.panelPos[1], x.panelPos[2], ndcX, ndcY, eyeSep);
		}
	}
}

static void shutdownXr(XrHello &x)
{
	x.gpuTimer.shutdown(); // Delete queries while the owning EGL context is current.
	saveLayout(x);
	if (x.sessionRunning) {
		xrEndSession(x.session);
		x.sessionRunning = false;
	}
	if (x.vbo) xr_glDeleteBuffers(1, &x.vbo);
	if (x.program) xr_glDeleteProgram(x.program);
	if (x.quadVBO) xr_glDeleteBuffers(1, &x.quadVBO);
	if (x.quadVAO) xr_glDeleteVertexArrays(1, &x.quadVAO);
	if (x.quadProgram) xr_glDeleteProgram(x.quadProgram);
	if (x.dioramaVBO) xr_glDeleteBuffers(1,&x.dioramaVBO);
	if (x.dioramaVAO) xr_glDeleteVertexArrays(1,&x.dioramaVAO);
	if (x.dioramaProgram) xr_glDeleteProgram(x.dioramaProgram);
	if (x.fbo) xr_glDeleteFramebuffers(1, &x.fbo);
	if (x.panelDepth) xr_glDeleteRenderbuffers(1,&x.panelDepth);
	for (int eye = 0; eye < 2; eye++) {
		if (x.swapchains[eye].handle != XR_NULL_HANDLE) {
			xrDestroySwapchain(x.swapchains[eye].handle);
			x.swapchains[eye].handle = XR_NULL_HANDLE;
		}
	}
	if (x.localSpace != XR_NULL_HANDLE) {
		xrDestroySpace(x.localSpace);
		x.localSpace = XR_NULL_HANDLE;
	}
	x.scene.clear();
	for(int i=0;i<2;++i) if(x.controls.aimSpace[i]!=XR_NULL_HANDLE) xrDestroySpace(x.controls.aimSpace[i]);
	if (x.uiButtonTexture) xr_glDeleteTextures(1,&x.uiButtonTexture);
	if (x.settingsTexture) xr_glDeleteTextures(1,&x.settingsTexture);
	if (x.hoverTexture) xr_glDeleteTextures(1,&x.hoverTexture);
	if (x.sceneTexture) xr_glDeleteTextures(1,&x.sceneTexture);
	if (x.recoveryTexture) xr_glDeleteTextures(1,&x.recoveryTexture);
	if (x.resultTexture) xr_glDeleteTextures(1,&x.resultTexture);
	if (x.commandsTexture) xr_glDeleteTextures(1,&x.commandsTexture);
	if (x.commandButtonTexture) xr_glDeleteTextures(1,&x.commandButtonTexture);
	for (int i=0;i<2;++i) if(x.controls.gripSpace[i]!=XR_NULL_HANDLE) xrDestroySpace(x.controls.gripSpace[i]);
	if (x.controls.set != XR_NULL_HANDLE) xrDestroyActionSet(x.controls.set);
	// Passthrough teardown (before the session dies). Entry points are
	// fetched again -- cheap, and keeps shutdown independent of init.
	if (x.passthroughLayer != XR_NULL_HANDLE || x.passthrough != XR_NULL_HANDLE) {
		PFN_xrPassthroughLayerPauseFB pLayerPause = nullptr;
		PFN_xrDestroyPassthroughLayerFB pLayerDestroy = nullptr;
		PFN_xrPassthroughPauseFB pPause = nullptr;
		PFN_xrDestroyPassthroughFB pDestroy = nullptr;
		xrGetInstanceProcAddr(x.instance, "xrPassthroughLayerPauseFB",
			reinterpret_cast<PFN_xrVoidFunction *>(&pLayerPause));
		xrGetInstanceProcAddr(x.instance, "xrDestroyPassthroughLayerFB",
			reinterpret_cast<PFN_xrVoidFunction *>(&pLayerDestroy));
		xrGetInstanceProcAddr(x.instance, "xrPassthroughPauseFB",
			reinterpret_cast<PFN_xrVoidFunction *>(&pPause));
		xrGetInstanceProcAddr(x.instance, "xrDestroyPassthroughFB",
			reinterpret_cast<PFN_xrVoidFunction *>(&pDestroy));
		if (x.passthroughLayer != XR_NULL_HANDLE) {
			if (pLayerPause) pLayerPause(x.passthroughLayer);
			if (pLayerDestroy) pLayerDestroy(x.passthroughLayer);
			x.passthroughLayer = XR_NULL_HANDLE;
		}
		if (x.passthrough != XR_NULL_HANDLE) {
			if (pPause) pPause(x.passthrough);
			if (pDestroy) pDestroy(x.passthrough);
			x.passthrough = XR_NULL_HANDLE;
		}
		x.passthroughActive = false;
	}
	if (x.session != XR_NULL_HANDLE) {
		xrDestroySession(x.session);
		x.session = XR_NULL_HANDLE;
	}
	if (x.instance != XR_NULL_HANDLE) {
		xrDestroyInstance(x.instance);
		x.instance = XR_NULL_HANDLE;
	}
	if (x.eglDisplay != EGL_NO_DISPLAY) {
		eglMakeCurrent(x.eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
		if (x.eglContext != EGL_NO_CONTEXT)
			eglDestroyContext(x.eglDisplay, x.eglContext);
		if (x.eglSurface != EGL_NO_SURFACE)
			eglDestroySurface(x.eglDisplay, x.eglSurface);
		eglTerminate(x.eglDisplay);
		x.eglDisplay = EGL_NO_DISPLAY;
	}
	XR_LOG("XR shutdown");
}

extern "C" {

JNIEXPORT void JNICALL
Java_com_generalsx_zerohour_XrHelloActivity_runHello(JNIEnv *env, jclass, jobject activity,jint initialLanguage)
{
	XR_LOG("runHello: enter, build QTR-P17 conditional ordinary-world draw elision");
	JavaVM *vm = nullptr;
	if (env->GetJavaVM(&vm) != JNI_OK || vm == nullptr) {
		XR_LOGE("GetJavaVM failed");
		return;
	}
	jobject activityRef = env->NewGlobalRef(activity);
	g_stopFlag.store(false);
	if (!selfCheckMath()) {
		XR_LOGE("math self-check failed, nothing to run");
		env->DeleteGlobalRef(activityRef);
		return;
	}
	XrHello x;
	x.activityRef=activityRef;
	x.panelEnv=env;x.panelPainter=env->FindClass("com/generalsx/zerohour/XrPanelPainter");
	if(env->ExceptionCheck()) {env->ExceptionClear();x.panelPainter=nullptr;}
	if (initXr(x, vm, activityRef)) {
		// Phase 1.0: boot the game on the XR thread (same thread, same
		// GL context as the loop below). Synchronous and slow (~a
		// minute of asset loading with the runtime's loading space
		// showing); failure falls back to the triangle loop so "XR
		// works but the engine didn't boot" stays diagnosable.
		x.gameBooted = XrGameBoot_Init(env, activityRef, x.eglDisplay, x.eglContext);
		if(x.gameBooted)XrGameBoot_ConfigureMultiview([](const char *name)->void * {return reinterpret_cast<void *>(eglGetProcAddress(name));});
		bool restored=x.layout.load(XrGameBoot_LayoutPath());
		if(!restored) restored=x.layout.load(XrGameBoot_LegacyLayoutPath());
		x.layout.initializeLanguage(restored,initialLanguage);
		// Versioned requested adjustments run once, retaining other custom choices.
		const bool upgraded=x.layout.upgradeDefaults();
		// P20: room surfaces are session-owned and there is no persistent spatial
		// anchor. Restore preferences, never yesterday's room-relative geometry.
		// placePanel() composes this safe arrangement with the first valid HMD pose.
		x.layout.applyFreeStandingStart();
		x.layoutDirty=upgraded || !restored;
		g_xrLanguage=x.layout.language;
		x.worldZoom=x.layout.worldZoom;
		XR_LOG("P20 display: free-standing session geometry; preferences=%s",restored ? "restored/migrated":"defaults");
		if (x.gameBooted && !buildQuadResources(x)) {
			XR_LOGE("quad build failed, shutting engine down for triangle fallback");
			XrGameBoot_Shutdown();
			x.gameBooted = false;
		}
		if(x.gameBooted) {
			x.dioramaReady=buildDioramaResources(x);
			if(!x.dioramaReady) XR_LOGE("P6 unavailable; existing game presentation preserved");
			d3d8gles_InvalidateCachedState();
		}
		runLoop(x);
		if (x.gameBooted)
			XrGameBoot_Shutdown();
	} else {
		XR_LOGE("initXr failed, nothing to run");
	}
	shutdownXr(x);
	env->DeleteGlobalRef(activityRef);
	XR_LOG("runHello: exit");
}

JNIEXPORT void JNICALL
Java_com_generalsx_zerohour_XrHelloActivity_stopHello(JNIEnv *, jclass)
{
	XR_LOG("stopHello");
	g_stopFlag.store(true);
}

} // extern "C"

#else // !__ANDROID__
#error "XrHello.cpp is Android-only"
#endif
