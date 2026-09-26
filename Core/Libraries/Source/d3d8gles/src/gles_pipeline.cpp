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
** gles_pipeline.cpp - D3D8 fixed function on WebGL2.
** GeneralsX @build web-port 05/07/2026 - Web port Phase 2
**
** See gles_pipeline.h. Correctness-first: GL state is (re)applied per draw,
** uniforms re-uploaded per draw; programs and texture objects are cached.
*/

// NOTE: this file is #included at the bottom of d3d8gles.cpp (single TU) so
// it can access the device/resource class internals defined there.
#include "gles_pipeline.h"
#include "gles_dispatch.h"
#include "XRStereoShader.h"
#include "XRDecorationShader.h"
#include "XRStereoPolicy.h"
#include "XRStereoRestore.h"
#include <chrono>

#include <SDL3/SDL.h>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#if defined(__ANDROID__)
#include <android/log.h>
#include <android/native_window.h>
#endif

// ---------------------------------------------------------------------------
// Small helpers
// ---------------------------------------------------------------------------

static bool g_glTrace = false;

// GeneralsX @build Android port GLES experiment 08/30/2026 Uniform Buffer
// Object binding point for the camera (view+proj) block -- see its use in
// getProgram()/applyUniforms() below for why this exists: a plain
// glUniformMatrix4fv upload is per-PROGRAM state (GL forgets it across a
// glUseProgram switch), so a scene that alternates shader programs a lot
// (different FVF/lighting/material/fog/texture-stage combos for terrain vs.
// units vs. particles vs. decals) was forced to re-upload the camera on
// every single program switch even when the camera hadn't moved at all --
// confirmed via a real device log's per-block uniform-cache breakdown
// dropping to ~50% mid-battle. A UBO's DATA lives independently of which
// program is currently bound; only the bound *buffer* at this binding
// point matters, and that's set once here, not per-draw or per-program.
// So the camera now only needs a real re-upload when its actual value
// changes (once per frame in the common case), never merely because the
// active program changed.
static const GLuint kViewProjUBOBinding = 0;

#define GLTRACE(...)                              \
	do {                                          \
		if (g_glTrace) {                          \
			fprintf(stderr, "[d3d8gles.gl] ");   \
			fprintf(stderr, __VA_ARGS__);         \
			fprintf(stderr, "\n");                \
		}                                         \
	} while (0)

// Log an unsupported state combination once per session.
#define WARN_ONCE(flagvar, ...)                       \
	do {                                              \
		static bool flagvar = false;                  \
		if (!flagvar) {                               \
			flagvar = true;                           \
			fprintf(stderr, "[d3d8gles] WARN: ");    \
			fprintf(stderr, __VA_ARGS__);             \
			fprintf(stderr, "\n");                    \
		}                                             \
	} while (0)

// Same, but keyed on a runtime value instead of a call site: warns once per
// distinct value (up to 8 of them), so one unimplemented enum does not drown
// out the next. Used for the texture-stage op fallback, where the whole point
// is to name WHICH op is missing.
#define WARN_ONCE_ARG(val, ...)                                          \
	do {                                                                  \
		static unsigned s_seen[8] = {0, 0, 0, 0, 0, 0, 0, 0};             \
		static int s_seenCount = 0;                                       \
		bool s_already = false;                                           \
		for (int s_i = 0; s_i < s_seenCount; s_i++) {                     \
			if (s_seen[s_i] == (unsigned)(val)) { s_already = true; break; } \
		}                                                                 \
		if (!s_already && s_seenCount < 8) {                              \
			s_seen[s_seenCount++] = (unsigned)(val);                      \
			fprintf(stderr, "[d3d8gles] WARN: ");                        \
			fprintf(stderr, __VA_ARGS__);                                 \
			fprintf(stderr, "\n");                                        \
		}                                                                 \
	} while (0)

static float dwordToFloat(DWORD v)
{
	float f;
	memcpy(&f, &v, sizeof(f));
	return f;
}

static void argbToFloats(uint32_t argb, float out[4])
{
	out[0] = ((argb >> 16) & 0xFF) / 255.0f;
	out[1] = ((argb >> 8) & 0xFF) / 255.0f;
	out[2] = (argb & 0xFF) / 255.0f;
	out[3] = ((argb >> 24) & 0xFF) / 255.0f;
}

static GLenum d3dCmpToGL(DWORD cmp)
{
	switch (cmp) {
	case D3DCMP_NEVER: return GL_NEVER;
	case D3DCMP_LESS: return GL_LESS;
	case D3DCMP_EQUAL: return GL_EQUAL;
	case D3DCMP_LESSEQUAL: return GL_LEQUAL;
	case D3DCMP_GREATER: return GL_GREATER;
	case D3DCMP_NOTEQUAL: return GL_NOTEQUAL;
	case D3DCMP_GREATEREQUAL: return GL_GEQUAL;
	case D3DCMP_ALWAYS: default: return GL_ALWAYS;
	}
}

static GLenum d3dBlendToGL(DWORD b)
{
	switch (b) {
	case D3DBLEND_ZERO: return GL_ZERO;
	case D3DBLEND_ONE: return GL_ONE;
	case D3DBLEND_SRCCOLOR: return GL_SRC_COLOR;
	case D3DBLEND_INVSRCCOLOR: return GL_ONE_MINUS_SRC_COLOR;
	case D3DBLEND_SRCALPHA: return GL_SRC_ALPHA;
	case D3DBLEND_INVSRCALPHA: return GL_ONE_MINUS_SRC_ALPHA;
	case D3DBLEND_DESTALPHA: return GL_DST_ALPHA;
	case D3DBLEND_INVDESTALPHA: return GL_ONE_MINUS_DST_ALPHA;
	case D3DBLEND_DESTCOLOR: return GL_DST_COLOR;
	case D3DBLEND_INVDESTCOLOR: return GL_ONE_MINUS_DST_COLOR;
	case D3DBLEND_SRCALPHASAT: return GL_SRC_ALPHA_SATURATE;
	default: return GL_ONE;
	}
}

static GLenum d3dStencilOpToGL(DWORD op)
{
	switch (op) {
	case D3DSTENCILOP_KEEP: return GL_KEEP;
	case D3DSTENCILOP_ZERO: return GL_ZERO;
	case D3DSTENCILOP_REPLACE: return GL_REPLACE;
	case D3DSTENCILOP_INCRSAT: return GL_INCR;
	case D3DSTENCILOP_DECRSAT: return GL_DECR;
	case D3DSTENCILOP_INVERT: return GL_INVERT;
	case D3DSTENCILOP_INCR: return GL_INCR_WRAP;
	case D3DSTENCILOP_DECR: return GL_DECR_WRAP;
	default: return GL_KEEP;
	}
}

// FVF layout description.
struct FVFLayout {
	bool xyzrhw = false;
	bool hasNormal = false;
	bool hasPSize = false;
	bool hasDiffuse = false;
	bool hasSpecular = false;
	int texCount = 0;
	int texSize[8] = {2, 2, 2, 2, 2, 2, 2, 2}; // floats per set
	int posOffset = 0;
	int normalOffset = -1;
	int diffuseOffset = -1;
	int specularOffset = -1;
	int texOffset[8] = {-1, -1, -1, -1, -1, -1, -1, -1};
	int stride = 0;
};

static bool parseFVF(unsigned fvf, FVFLayout *out)
{
	FVFLayout l;
	const unsigned pos = fvf & D3DFVF_POSITION_MASK;
	int off = 0;
	l.posOffset = 0;
	if (pos == D3DFVF_XYZ) {
		off = 12;
	} else if (pos == D3DFVF_XYZRHW) {
		l.xyzrhw = true;
		off = 16;
	} else {
		// XYZB1-5 blend weights unused by the engine's FF paths.
		return false;
	}
	if (fvf & D3DFVF_NORMAL) {
		l.hasNormal = true;
		l.normalOffset = off;
		off += 12;
	}
	if (fvf & D3DFVF_PSIZE) {
		l.hasPSize = true;
		off += 4;
	}
	if (fvf & D3DFVF_DIFFUSE) {
		l.hasDiffuse = true;
		l.diffuseOffset = off;
		off += 4;
	}
	if (fvf & D3DFVF_SPECULAR) {
		l.hasSpecular = true;
		l.specularOffset = off;
		off += 4;
	}
	l.texCount = (fvf & D3DFVF_TEXCOUNT_MASK) >> D3DFVF_TEXCOUNT_SHIFT;
	if (l.texCount > 8) l.texCount = 8;
	for (int i = 0; i < l.texCount; i++) {
		const unsigned fmt = (fvf >> (16 + i * 2)) & 0x3;
		int size = 2;
		switch (fmt) {
		case 0: size = 2; break; // D3DFVF_TEXTUREFORMAT2
		case 1: size = 3; break; // D3DFVF_TEXTUREFORMAT3
		case 2: size = 4; break; // D3DFVF_TEXTUREFORMAT4
		case 3: size = 1; break; // D3DFVF_TEXTUREFORMAT1
		}
		l.texSize[i] = size;
		l.texOffset[i] = off;
		off += size * 4;
	}
	l.stride = off;
	*out = l;
	return true;
}

// ---------------------------------------------------------------------------
// Program cache
// ---------------------------------------------------------------------------

struct WebGLPipeline::ProgramInfo {
	GLuint prog = 0;
	XRUniformCache xrUniforms;
	// uniforms
	GLint uWorld = -1;
	GLint uXrActive=-1,uXrEyeClip=-1,uXrBoard=-1,uXrAspect=-1,uXrOpaque=-1;
	GLint uXrCamera=-1,uXrViewSpace=-1;
	GLint uShroudTex=-1,uShroudST=-1;
	// uView/uProj moved into the ViewProjBlock UBO (kViewProjUBOBinding) --
	// no per-program location for them anymore, see getProgram()'s shader
	// declaration and the post-link glUniformBlockBinding() call below.
	GLint uViewportPos = -1;
	GLint uYFlip = -1;
	GLint uTex0 = -1, uTex1 = -1;
	GLint uTexMat0 = -1, uTexMat1 = -1;
	GLint uTFactor = -1;
	GLint uAlphaRef = -1;
	GLint uFogColor = -1, uFogParams = -1;
	GLint uMatDiffuse = -1, uMatAmbient = -1, uMatEmissive = -1;
	GLint uGlobalAmbient = -1;
	GLint uNumLights = -1;
	GLint uLightType = -1, uLightDir = -1, uLightPos = -1;
	GLint uLightDiffuse = -1, uLightAmbient = -1, uLightAtten = -1;
	// key fields needed at bind time
	int stageTci[2] = {0, 0};
	bool stageXform[2] = {false, false};
	int stagesUsed = 0;
};

// Stage portion of the program key.
struct StageKey {
	// GeneralsX @bugfix Android port 09/05/2026 arg0 is D3D8's THIRD combiner
	// argument (D3DTSS_COLORARG0/D3DTSS_ALPHAARG0), used only by the
	// multiply-accumulate and lerp ops. It was missing entirely, so
	// D3DTOP_MULTIPLYADD fell through combinerOp()'s silent `default:` and
	// was emitted as a plain modulate -- see the D3DTOP_MULTIPLYADD case
	// there for the UI bug that caused.
	unsigned colorOp, colorArg0, colorArg1, colorArg2;
	unsigned alphaOp, alphaArg0, alphaArg1, alphaArg2;
	unsigned tci;
	unsigned texgen; // 0=vertex uv set, 1=camera-space position
	bool xform;
};

static void getStageKey(WebGLDevice *dev, int stage, StageKey *k);
static std::string combinerArg(unsigned arg, const char *texExpr, int *usesTex);
static std::string combinerOp(unsigned op, const std::string &a0, const std::string &a1,
                              const std::string &a2, const char *texAlphaExpr);

WebGLPipeline *WebGLPipeline::get()
{
	static WebGLPipeline *s_instance = nullptr;
	if (!s_instance) {
		s_instance = new WebGLPipeline();
		g_glTrace = getenv("D3D8GLES_TRACE") != nullptr;
	}
	return s_instance;
}

bool WebGLPipeline::initContextExternal(int w, int h)
{
	if (m_ctxReady) {
		resize(w, h);
		return true;
	}

	// GeneralsX @feature XR port Phase 1.0 - the caller's EGL context is
	// already current (XR loop made it so); no SDL window exists, so no
	// context creation, swap interval, or surface queries happen here.
	m_window = nullptr;
	m_glContext = nullptr;
	m_xrMode = true;
	m_xrElision={};
	// GeneralsX @feature Quest tabletop recovery 13/09/2026 Keep the costly
	// glReadPixels path available for explicit diagnostics, but never run it
	// in a normal XR session. Any non-empty value enables the periodic PPM.
	m_xrCaptureEnabled = getenv("GX_XR_CAPTURE_PPM") != nullptr;

	// Dispatch MUST be system GLES: the XR context comes from system EGL,
	// and ANGLE entry points against a system context would be garbage.
	if (!d3d8gles_LoadGLESDispatch("libGLESv3.so")) {
		fprintf(stderr, "[d3d8gles] FATAL: XR mode requires system libGLESv3.so dispatch\n");
		return false;
	}
	fprintf(stderr, "[d3d8gles] XR mode: GLES backend libGLESv3.so (forced), diagnostic capture=%s\n",
		m_xrCaptureEnabled ? "enabled" : "disabled");

	const char *extensions = (const char *)glGetString(GL_EXTENSIONS);
	m_hasS3TC = extensions != nullptr &&
		(strstr(extensions, "GL_EXT_texture_compression_s3tc") != nullptr ||
		 strstr(extensions, "GL_EXT_texture_compression_dxt1") != nullptr);

	m_fbWidth = w;
	m_fbHeight = h;
	m_curRTWidth = w;
	m_curRTHeight = h;

	glGenBuffers(1, &m_upVBO);
	glGenBuffers(1, &m_upIBO);

	glGenBuffers(1, &m_viewProjUBO);
	glBindBuffer(GL_UNIFORM_BUFFER, m_viewProjUBO);
	glBufferData(GL_UNIFORM_BUFFER, sizeof(float) * 32, nullptr, GL_DYNAMIC_DRAW);
	glBindBufferBase(GL_UNIFORM_BUFFER, kViewProjUBOBinding, m_viewProjUBO);

	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glDisable(GL_DITHER);

	m_ctxReady = true;
	fprintf(stderr, "[d3d8gles] XR mode: GLES3 context ready %dx%d (s3tc=%d), rendering redirects to owned FBO\n",
		w, h, (int)m_hasS3TC);
	return true;
}

bool WebGLPipeline::initContext(int w, int h, SDL_Window *window)
{
	// GeneralsX @feature XR port Phase 1.0 - external-context mode takes
	// over before any window handling (focusWindow may be null here).
	if (d3d8gles_GetXRConfig() != nullptr) {
		return initContextExternal(w, h);
	}

	if (m_ctxReady) {
		resize(w, h);
		return true;
	}

	m_window = window;

	// GeneralsX @build Android port GLES experiment - native GLES3 context via
	// SDL3 instead of an Emscripten/WebGL2 canvas context. SDL3 wraps EGL on
	// Android (SDL_GL_CreateContext/MakeCurrent/SwapWindow), the same way the
	// Vulkan path already leans on SDL3's Vulkan WSI instead of raw
	// vkCreateSwapchainKHR. The SDL_GL_SetAttribute calls that pick the
	// GLES3/depth/stencil config live in SDL3Main.cpp, BEFORE
	// SDL_CreateWindow -- SDL only applies them to windows created after the
	// call, so they can't live here (this runs well after the window exists).
	m_glContext = SDL_GL_CreateContext(window);
	if (!m_glContext) {
		fprintf(stderr, "[d3d8gles] FATAL: SDL_GL_CreateContext failed: %s\n", SDL_GetError());
		return false;
	}
	if (!SDL_GL_MakeCurrent(window, (SDL_GLContext)m_glContext)) {
		fprintf(stderr, "[d3d8gles] FATAL: SDL_GL_MakeCurrent failed: %s\n", SDL_GetError());
		return false;
	}
	// GeneralsX @bugfix Android port 09/04/2026 Diagnostic: a real-device
	// GL_VIEWPORT dump at present() time already confirmed our own viewport
	// bookkeeping (w/h passed in here, from _PresentParameters) is applied
	// correctly to GL every frame, yet a persistent edge strip remains --
	// meaning the ACTUAL EGL window surface's real pixel size may not match
	// w/h at all. eglCreateWindowSurface() (called inside SDL_GL_CreateContext
	// above) sizes the surface from the ANativeWindow directly, independent
	// of any width/height we pass anywhere -- if the window hadn't fully
	// settled by the time THIS call ran (a separate race from the
	// resolution-detection debounce in SDL3Main.cpp, which only guards
	// xres/yres computed much earlier, before device/context creation), the
	// EGL surface could be locked in at a stale size for its whole lifetime.
	// Query the window's size again right here, at the exact moment the EGL
	// surface actually gets created, to see whether it agrees with w/h.
	{
		int freshW = 0, freshH = 0;
		SDL_GetWindowSizeInPixels(window, &freshW, &freshH);
		fprintf(stderr, "[d3d8gles-diag] initContext(): requested w=%d h=%d, "
			"SDL_GetWindowSizeInPixels() right after SDL_GL_CreateContext=%dx%d\n",
			w, h, freshW, freshH);
	}
#if defined(__ANDROID__)
	// GeneralsX @bugfix Android port 09/04/2026 EXPERIMENT (low confidence):
	// force the underlying ANativeWindow's pixel format to RGBX_8888
	// (opaque -- the 4th byte is padding, explicitly ignored by
	// SurfaceFlinger/the compositor) rather than whatever format the chosen
	// EGL config implied. This is the Android-native equivalent of Vulkan's
	// VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR, which DXVK's swapchain likely
	// declares explicitly -- EGL has no equivalent explicit knob, so on
	// Android the compositor's opaque-vs-blended decision is otherwise
	// inferred from the EGL config's alpha bits, which is comparatively
	// fragile. Explicitly setting RGBX_8888 here removes that inference
	// entirely, regardless of what SDL_GL_ALPHA_SIZE was requested and
	// regardless of whether any of this engine's own rendering incidentally
	// writes a non-1.0 alpha into the framebuffer somewhere. width/height=0
	// means "keep the ANativeWindow's current size", only the format
	// changes. Real-device diagnostics already ruled out every
	// application-level cause (viewport application, EGL surface size at
	// creation time) for a persistent edge strip seen only on GLES/ANGLE,
	// never Vulkan -- worth testing whether this is a compositor-level
	// alpha-blending artifact instead.
	{
		void *nativeWindowPtr = SDL_GetPointerProperty(SDL_GetWindowProperties(window),
			SDL_PROP_WINDOW_ANDROID_WINDOW_POINTER, nullptr);
		if (nativeWindowPtr) {
			int rc = ANativeWindow_setBuffersGeometry((ANativeWindow *)nativeWindowPtr, 0, 0, WINDOW_FORMAT_RGBX_8888);
			fprintf(stderr, "[d3d8gles-diag] ANativeWindow_setBuffersGeometry(RGBX_8888) -> %d\n", rc);
		} else {
			fprintf(stderr, "[d3d8gles-diag] ANativeWindow_setBuffersGeometry skipped: no native window pointer\n");
		}
	}
#endif
	// GeneralsX @bugfix Android port 09/04/2026 EXPERIMENT (low confidence):
	// this call's return value was never checked. Reconsidering the reported
	// edge artifact from scratch: it's variable in *which* edge (right, then
	// also bottom), only shows on scenes with real on-screen motion (menu
	// water/ships, gameplay units, video playback), sometimes coincides with
	// UI briefly not appearing at all, and is completely absent on Vulkan/
	// DXVK -- all consistent with plain screen TEARING (GL swap running
	// without real vsync) rather than a rendering-geometry bug, which every
	// other diagnostic this branch has already ruled out (GL_VIEWPORT and
	// the EGL surface's actual size both confirmed correct on a real
	// device). DXVK's own present path is Vulkan-native (vkQueuePresentKHR
	// with its own FIFO/vsync semantics) and was never touched by this
	// SDL_GL_SetSwapInterval() call at all, so a vsync failure specific to
	// the GLES/EGL path would explain the Vulkan/GLES split directly. Log
	// the actual result and the interval SDL reports back afterward, since
	// silent failure here would otherwise be invisible.
	{
		bool intervalOk = SDL_GL_SetSwapInterval(1);
		int actualInterval = 0;
		bool queryOk = SDL_GL_GetSwapInterval(&actualInterval);
		fprintf(stderr, "[d3d8gles-diag] SDL_GL_SetSwapInterval(1) -> %d, "
			"SDL_GL_GetSwapInterval() -> ok=%d interval=%d\n",
			(int)intervalOk, (int)queryOk, actualInterval);
	}

	// GeneralsX @build Android port ANGLE experiment - resolve every gl*
	// entry point this module calls via gles_dispatch.cpp instead of relying
	// on direct DT_NEEDED linking against system libGLESv3.so (see
	// CMakeLists.txt for why). Must run after MakeCurrent (a context needs to
	// be current for the chosen implementation to hand back valid function
	// pointers) and before the very first gl* call below. Mirrors the
	// SDL_HINT_EGL_LIBRARY choice made in SDL3Main.cpp before SDL_InitSubSystem
	// -- both must agree on ANGLE vs. system GLES, or the EGL context and the
	// GL entry points come from two different, incompatible implementations.
	{
		// GeneralsX @build Android port render-backend picker 07/09/2026 -
		// must resolve ANGLE-vs-system exactly the way SDL3Main.cpp's
		// UseANGLE() (and dx8wrapper.cpp's Vulkan check) do -- delegates to
		// the single shared implementation, d3d8gles_ShouldUseANGLE() just
		// above in this same translation unit (d3d8gles.cpp #includes this
		// file), instead of re-reading render_backend.cfg here too. See
		// d3d8gles.h's comment on why three separate copies of this check
		// caused a real bug.
		const bool useANGLE = d3d8gles_ShouldUseANGLE();
		const char *libName = useANGLE ? "libGLESv2_angle.so" : "libGLESv3.so";
		if (!d3d8gles_LoadGLESDispatch(libName)) {
			if (useANGLE) {
				fprintf(stderr, "[d3d8gles] ANGLE GLES dispatch unavailable, falling back to system libGLESv3.so\n");
				libName = "libGLESv3.so";
			}
			if (!d3d8gles_LoadGLESDispatch(libName)) {
				fprintf(stderr, "[d3d8gles] FATAL: could not resolve any GLES implementation (tried %s)\n", libName);
				return false;
			}
		}
		fprintf(stderr, "[d3d8gles] GLES backend: %s\n", libName);
	}

	const char *extensions = (const char *)glGetString(GL_EXTENSIONS);
	m_hasS3TC = extensions != nullptr &&
		(strstr(extensions, "GL_EXT_texture_compression_s3tc") != nullptr ||
		 strstr(extensions, "GL_EXT_texture_compression_dxt1") != nullptr);

	m_fbWidth = w;
	m_fbHeight = h;
	m_curRTWidth = w;
	m_curRTHeight = h;

	glGenBuffers(1, &m_upVBO);
	glGenBuffers(1, &m_upIBO);

	// GeneralsX @build Android port GLES experiment 08/30/2026 See
	// kViewProjUBOBinding's comment: allocate the camera UBO once here and
	// bind it to its binding point for the whole session. std140 layout of
	// two consecutive mat4 members needs no padding (each is a 64-byte,
	// 16-byte-aligned block) -- 128 bytes total, matching ViewProjKey's
	// `float view[16], proj[16]` byte-for-byte, same raw column-major
	// layout already assumed everywhere else this codebase uploads a D3D
	// matrix via glUniformMatrix4fv(..., GL_FALSE, ...).
	glGenBuffers(1, &m_viewProjUBO);
	glBindBuffer(GL_UNIFORM_BUFFER, m_viewProjUBO);
	glBufferData(GL_UNIFORM_BUFFER, sizeof(float) * 32, nullptr, GL_DYNAMIC_DRAW);
	glBindBufferBase(GL_UNIFORM_BUFFER, kViewProjUBOBinding, m_viewProjUBO);

	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glDisable(GL_DITHER);

	m_ctxReady = true;
	// GeneralsX @build Android port 09/05/2026 Report the DEFAULT framebuffer's
	// actual bit depths, not the ones we asked SDL for. Stencil is the one that
	// matters: volumetric (stencil) shadows are enabled only at High detail and
	// above, which is exactly when the reported helicopter artifact appears, and
	// a config without stencil bits makes every stencil test pass silently --
	// which would draw W3DVolumetricShadowManager::renderStencilShadows()'s
	// darkening quad unmasked instead of only inside the shadow. SDL is free to
	// hand back a config that does not satisfy every requested attribute.
	{
		GLint rb = 0, gb = 0, bb = 0, ab = 0, db = 0, sb = 0;
		glGetIntegerv(GL_RED_BITS, &rb);
		glGetIntegerv(GL_GREEN_BITS, &gb);
		glGetIntegerv(GL_BLUE_BITS, &bb);
		glGetIntegerv(GL_ALPHA_BITS, &ab);
		glGetIntegerv(GL_DEPTH_BITS, &db);
		glGetIntegerv(GL_STENCIL_BITS, &sb);
		fprintf(stderr, "[d3d8gles] default framebuffer bits: r=%d g=%d b=%d a=%d depth=%d stencil=%d\n",
			(int)rb, (int)gb, (int)bb, (int)ab, (int)db, (int)sb);
	}
	fprintf(stderr, "[d3d8gles] GLES3 context ready %dx%d (s3tc=%d)\n", w, h, (int)m_hasS3TC);
	return true;
}

void WebGLPipeline::resize(int w, int h)
{
	if (w == m_fbWidth && h == m_fbHeight) return;
	// The native window surface already tracks the real size on its own
	// (unlike a browser canvas, which needed an explicit element-size call);
	// nothing to resize here beyond our own bookkeeping.
	m_fbWidth = w;
	m_fbHeight = h;
	if (m_curFBO == 0) {
		m_curRTWidth = w;
		m_curRTHeight = h;
	}
	fprintf(stderr, "[d3d8gles] window resized to %dx%d\n", w, h);
}

// ---------------------------------------------------------------------------
// Shader generation
// ---------------------------------------------------------------------------

static void getStageKey(WebGLDevice *dev, int stage, StageKey *k)
{
	// Keep the FULL argument values: the low nibble selects the source and
	// bits 0x10/0x20 are the COMPLEMENT/ALPHAREPLICATE modifiers (the road
	// noise pass uses DIFFUSE|ALPHAREPLICATE to synthesize white).
	k->colorOp = dev->getStageState(stage, D3DTSS_COLOROP);
	k->colorArg1 = dev->getStageState(stage, D3DTSS_COLORARG1) & 0x3F;
	k->colorArg2 = dev->getStageState(stage, D3DTSS_COLORARG2) & 0x3F;
	k->alphaOp = dev->getStageState(stage, D3DTSS_ALPHAOP);
	k->alphaArg1 = dev->getStageState(stage, D3DTSS_ALPHAARG1) & 0x3F;
	k->alphaArg2 = dev->getStageState(stage, D3DTSS_ALPHAARG2) & 0x3F;
	// D3D8's documented default for ARG0 is D3DTA_CURRENT, not D3DTA_DIFFUSE
	// (which is what a never-set 0 would otherwise decode to here).
	const DWORD rawColorArg0 = dev->getStageState(stage, D3DTSS_COLORARG0);
	const DWORD rawAlphaArg0 = dev->getStageState(stage, D3DTSS_ALPHAARG0);
	k->colorArg0 = rawColorArg0 ? (unsigned)(rawColorArg0 & 0x3F) : (unsigned)D3DTA_CURRENT;
	k->alphaArg0 = rawAlphaArg0 ? (unsigned)(rawAlphaArg0 & 0x3F) : (unsigned)D3DTA_CURRENT;
	const DWORD tciRaw = dev->getStageState(stage, D3DTSS_TEXCOORDINDEX);
	k->texgen = 0;
	if (tciRaw & 0xFFFF0000u) {
		switch (tciRaw & 0xFFFF0000u) {
		case D3DTSS_TCI_CAMERASPACEPOSITION:
			// Terrain macro/cloud layers: uv = texture matrix * view-space pos.
			k->texgen = 1;
			break;
		case D3DTSS_TCI_CAMERASPACEREFLECTIONVECTOR:
			// Environment maps: uv = texture matrix * reflect(eye, view normal).
			k->texgen = 2;
			break;
		case D3DTSS_TCI_CAMERASPACENORMAL:
			k->texgen = 3;
			break;
		default:
			WARN_ONCE(s_texgen, "texgen TEXCOORDINDEX flags 0x%x not implemented (stage %d)", (unsigned)tciRaw, stage);
			break;
		}
	}
	k->tci = tciRaw & 0x1;
	const DWORD ttf = dev->getStageState(stage, D3DTSS_TEXTURETRANSFORMFLAGS);
	k->xform = (ttf & 0xFF) != 0; // COUNT1..4 -> apply the stage matrix
	// Defaults per D3D8 when never set: stage0 MODULATE tex*diffuse, stage1 DISABLE.
	if (k->colorOp == 0) k->colorOp = (stage == 0) ? D3DTOP_MODULATE : D3DTOP_DISABLE;
	if (k->alphaOp == 0) k->alphaOp = (stage == 0) ? D3DTOP_SELECTARG1 : D3DTOP_DISABLE;
	if (dev->getStageState(stage, D3DTSS_COLORARG1) == 0) k->colorArg1 = 2; // TEXTURE
	if (dev->getStageState(stage, D3DTSS_COLORARG2) == 0) k->colorArg2 = 0; // (CURRENT->DIFFUSE for st0)
	if (dev->getStageState(stage, D3DTSS_ALPHAARG1) == 0) k->alphaArg1 = 2;
}

// GeneralsX @bugfix Android port 09/05/2026 D3D8 still RUNS a texture stage
// whose op is not D3DTOP_DISABLE when no texture is bound at that stage, as
// long as the op's arguments do not source D3DTA_TEXTURE. This backend used to
// disable the entire stage on "no texture", which silently deleted a real pass.
//
// The case that exposed it: Render2DClass::Render()'s greyscale path (disabled
// command-bar buttons) puts D3DTOP_DOTPRODUCT3 on stage 1 with COLORARG1 =
// D3DTA_CURRENT and COLORARG2 = D3DTA_TFACTOR -- neither touches a texture, and
// the 2D path only ever binds stage 0. So the desaturation never ran, and the
// button came out as the bare stage-0 result (a MULTIPLYADD that brightens the
// icon by +0.25 -- reported as "the building icons became too light and I can
// no longer tell what is available"). Worse, it was intermittent: whenever a
// previous 3D pass happened to leave a texture bound on stage 1, the stage did
// run and every icon went greyscale at once -- which is the all-colour /
// all-grey flicker seen between otherwise identical frames.
//
// Collapse per channel, and only for the channel that actually reads the
// missing texture.
static bool stageChannelReadsTexture(unsigned op, unsigned arg0, unsigned arg1, unsigned arg2)
{
	if (op == D3DTOP_DISABLE) return false;
	// BLENDTEXTUREALPHA takes the texture's alpha implicitly, not through an
	// argument, so it reads the texture no matter what its args say.
	if (op == D3DTOP_BLENDTEXTUREALPHA) return true;
	auto isTex = [](unsigned a) { return (a & 0xF) == (unsigned)D3DTA_TEXTURE; };
	if (isTex(arg1) || isTex(arg2)) return true;
	// arg0 is only consulted by the two ops that take a third argument.
	if ((op == D3DTOP_MULTIPLYADD || op == D3DTOP_LERP) && isTex(arg0)) return true;
	return false;
}

static void collapseStageWithoutTexture(StageKey *k, int stage)
{
	if (stageChannelReadsTexture(k->colorOp, k->colorArg0, k->colorArg1, k->colorArg2)) {

		if (stage == 0) {
			k->colorOp = D3DTOP_SELECTARG2;
			k->colorArg2 = 0; // DIFFUSE
		} else {
			k->colorOp = D3DTOP_DISABLE;
		}
	}
	if (stageChannelReadsTexture(k->alphaOp, k->alphaArg0, k->alphaArg1, k->alphaArg2)) {
		if (stage == 0) {
			k->alphaOp = D3DTOP_SELECTARG2;
			k->alphaArg2 = 0;
		} else {
			k->alphaOp = D3DTOP_DISABLE;
		}
	}
}

uint64_t WebGLPipeline::computeProgramKey(WebGLDevice *dev, unsigned fvf) const
{
	FVFLayout l;
	parseFVF(fvf, &l);

	// FNV-1a over the full state values: argument MODIFIER bits (COMPLEMENT/
	// ALPHAREPLICATE) must differentiate programs and no longer fit a packed
	// 64-bit layout.
	uint64_t key = 0xcbf29ce484222325ull;
	auto put = [&](uint64_t v, int /*bits*/) {
		key ^= v + 0x9E37;
		key *= 0x100000001b3ull;
	};

	put(l.xyzrhw ? 1 : 0, 1);
	put(l.hasNormal ? 1 : 0, 1);
	put(l.hasDiffuse ? 1 : 0, 1);
	put(l.texCount > 2 ? 2 : l.texCount, 2);

	const bool lighting = dev->getRenderState(D3DRS_LIGHTING) != 0 && l.hasNormal && !l.xyzrhw;
	put(lighting ? 1 : 0, 1);

	// Material color sources (VertexMaterialClass::Apply drives these; the
	// W3D default is MATERIAL - skinned meshes carry diffuse=0 in the VB).
	if (lighting) {
		const bool cv = dev->getRenderState(D3DRS_COLORVERTEX) != 0 && l.hasDiffuse;
		put((cv && dev->getRenderState(D3DRS_DIFFUSEMATERIALSOURCE) == 1 /*COLOR1*/) ? 1 : 0, 1);
		put((cv && dev->getRenderState(D3DRS_AMBIENTMATERIALSOURCE) == 1) ? 1 : 0, 1);
		put((cv && dev->getRenderState(D3DRS_EMISSIVEMATERIALSOURCE) == 1) ? 1 : 0, 1);
	} else {
		put(0, 3);
	}

	const bool fog = dev->getRenderState(D3DRS_FOGENABLE) != 0 && !l.xyzrhw;
	put(fog ? 1 : 0, 1);
	put(shroudFoldFor(l.xyzrhw) ? 1 : 0, 1);

	const bool alphaTest = dev->getRenderState(D3DRS_ALPHATESTENABLE) != 0;
	put(alphaTest ? 1 : 0, 1);
	put(alphaTest ? (dev->getRenderState(D3DRS_ALPHAFUNC) & 0x7) : 0, 3);

	for (int s = 0; s < 2; s++) {
		StageKey sk;
		getStageKey(dev, s, &sk);
		// No texture bound: only the channels that source TEXTURE collapse.
		if (!dev->getTexture2D(s)) collapseStageWithoutTexture(&sk, s);
		put(sk.colorOp, 5);
		put(sk.colorArg0, 6);
		put(sk.colorArg1, 6);
		put(sk.colorArg2, 6);
		put(sk.alphaOp, 5);
		put(sk.alphaArg0, 6);
		put(sk.alphaArg1, 6);
		put(sk.alphaArg2, 6);
		put(sk.tci, 1);
		put(sk.texgen, 1);
		put(sk.xform ? 1 : 0, 1);
	}
	return key;
}

// Selector (arg & 0xF): 0=DIFFUSE 1=CURRENT 2=TEXTURE 3=TFACTOR 4=SPECULAR.
// Modifiers: D3DTA_COMPLEMENT (0x10) = 1-x, D3DTA_ALPHAREPLICATE (0x20) = x.aaaa
// (the road noise pass relies on DIFFUSE|ALPHAREPLICATE to build white).
static std::string combinerArg(unsigned arg, const char *texExpr, int *usesTex)
{
	const char *base;
	switch (arg & 0xF) {
	case 0: base = "vCol"; break;
	case 1: base = "cur"; break;
	case 2: *usesTex = 1; base = texExpr; break;
	case 3: base = "uTFactor"; break;
	case 4: base = "vSpec"; break;
	default: base = "vCol"; break;
	}
	std::string e = base;
	if (arg & 0x20) e = "vec4(" + e + ".a)";          // ALPHAREPLICATE first
	if (arg & 0x10) e = "(vec4(1.0) - " + e + ")";    // then COMPLEMENT
	return e;
}

static std::string combinerOp(unsigned op, const std::string &a0, const std::string &a1,
                              const std::string &a2, const char *texAlphaExpr)
{
	char buf[768];
	switch (op) {
	case D3DTOP_SELECTARG1: return a1;
	case D3DTOP_SELECTARG2: return a2;
	case D3DTOP_MODULATE:
		snprintf(buf, sizeof(buf), "(%s * %s)", a1.c_str(), a2.c_str());
		break;
	case D3DTOP_MODULATE2X:
		snprintf(buf, sizeof(buf), "min((%s * %s) * 2.0, vec4(1.0))", a1.c_str(), a2.c_str());
		break;
	case D3DTOP_MODULATE4X:
		snprintf(buf, sizeof(buf), "min((%s * %s) * 4.0, vec4(1.0))", a1.c_str(), a2.c_str());
		break;
	case D3DTOP_ADD:
		snprintf(buf, sizeof(buf), "min(%s + %s, vec4(1.0))", a1.c_str(), a2.c_str());
		break;
	case D3DTOP_ADDSIGNED:
		snprintf(buf, sizeof(buf), "clamp(%s + %s - 0.5, 0.0, 1.0)", a1.c_str(), a2.c_str());
		break;
	case D3DTOP_ADDSIGNED2X:
		snprintf(buf, sizeof(buf), "clamp((%s + %s - 0.5) * 2.0, 0.0, 1.0)", a1.c_str(), a2.c_str());
		break;
	case D3DTOP_SUBTRACT:
		snprintf(buf, sizeof(buf), "max(%s - %s, vec4(0.0))", a1.c_str(), a2.c_str());
		break;
	case D3DTOP_ADDSMOOTH:
		snprintf(buf, sizeof(buf), "(%s + %s - %s * %s)", a1.c_str(), a2.c_str(), a1.c_str(), a2.c_str());
		break;
	case D3DTOP_BLENDTEXTUREALPHA:
		snprintf(buf, sizeof(buf), "mix(%s, %s, %s)", a2.c_str(), a1.c_str(), texAlphaExpr);
		break;
	case D3DTOP_BLENDDIFFUSEALPHA:
		snprintf(buf, sizeof(buf), "mix(%s, %s, vCol.a)", a2.c_str(), a1.c_str());
		break;
	case D3DTOP_BLENDCURRENTALPHA:
		snprintf(buf, sizeof(buf), "mix(%s, %s, cur.a)", a2.c_str(), a1.c_str());
		break;
	case D3DTOP_BLENDFACTORALPHA:
		snprintf(buf, sizeof(buf), "mix(%s, %s, uTFactor.a)", a2.c_str(), a1.c_str());
		break;
	case D3DTOP_DOTPRODUCT3:
		snprintf(buf, sizeof(buf),
			"vec4(vec3(clamp(dot(%s.rgb - 0.5, %s.rgb - 0.5) * 4.0, 0.0, 1.0)), 1.0)",
			a1.c_str(), a2.c_str());
		break;
	// GeneralsX @bugfix Android port 09/09/2026 D3D8: SRGBA = Arg0 + Arg1 * Arg2,
	// i.e. ARG0 is the ADDEND and ARG1/ARG2 are the two multiplicands.
	//
	// This was implemented on 09/05/2026 as "Arg1 + Arg2 * Arg0" -- the operands
	// the wrong way round. It looked right on the one case it was written for
	// (Render2DClass::Render()'s greyscale command-bar buttons, which are bright)
	// and it is catastrophically wrong on anything dark. Both call sites in the
	// engine use the same pair of stages:
	//
	//     stage 0: MULTIPLYADD  ARG0 = ARG2 = TFACTOR|ALPHAREPLICATE, ARG1 = TEXTURE
	//     stage 1: DOTPRODUCT3  ARG1 = CURRENT, ARG2 = TFACTOR
	//     D3DRS_TEXTUREFACTOR = 0x80A5CA8E
	//
	// The constant is the proof of which order is real. TFACTOR.a = 0x80 = 0.502,
	// so the correct stage 0 is 0.502 + 0.502*tex, which lands in [0.5, 1.0]; the
	// DOT3 then subtracts 0.5 and scales by 4, giving 4*0.502*(tf.rgb - 0.5) . tex.
	// With tf.rgb = (0xA5, 0xCA, 0x8E)/255 those weights come out as
	// (0.295, 0.586, 0.114) -- Rec.601 luminance to within the rounding of a byte,
	// on all three channels. Westwood picked the constant by inverting exactly this
	// formula; no other operand order reproduces it.
	//
	// The old order gave tex + 0.502*0.502 = tex + 0.25, so the DOT3 computed
	// luminance(tex) - 0.49 instead: every texel darker than mid-grey clamped to
	// zero. Bright button icons survived (too dark, but visible), which is why it
	// passed as fixed; a whole night scene did not, and the scripted black-and-white
	// cinematic (ScreenBWFilterDOT3, W3DShaderManager.cpp) rendered SOLID BLACK.
	//
	// Cross-checked against DXVK's fixed-function translation, which emits
	// fma(arg[1], arg[2], arg[0]) with arg[0] = D3DTSS_COLORARG0 (d3d9_fixed_function
	// .cpp, D3DTOP_MULTIPLYADD), and saturates the result.
	case D3DTOP_MULTIPLYADD:
		snprintf(buf, sizeof(buf), "clamp(%s + %s * %s, vec4(0.0), vec4(1.0))",
			a0.c_str(), a1.c_str(), a2.c_str());
		break;
	// D3D8: SRGBA = Arg1 * Arg0 + Arg2 * (1 - Arg0), i.e. mix() with Arg0 as
	// the interpolant. Added alongside MULTIPLYADD because it is the other op
	// that reads ARG0 -- without it, ARG0 would still be write-only state.
	case D3DTOP_LERP:
		snprintf(buf, sizeof(buf), "mix(%s, %s, %s)",
			a2.c_str(), a1.c_str(), a0.c_str());
		break;
	default:
		// Silence here is how MULTIPLYADD survived: an unimplemented op used
		// to render as a modulate that looks plausible in most content. Say so
		// once per op instead, so the next missing one shows up in the device
		// log rather than as a mystery visual bug.
		// D3DTOP_DISABLE can reach here for a disabled stage 0 that sits below
		// an enabled stage 1; that is a known, harmless shape, not a gap.
		if (op != D3DTOP_DISABLE) {
			WARN_ONCE_ARG(op, "texture stage COLOROP/ALPHAOP %u not implemented, "
				"approximating as MODULATE", op);
		}
		snprintf(buf, sizeof(buf), "(%s * %s)", a1.c_str(), a2.c_str());
		break;
	}
	return buf;
}

static GLuint compileShader(GLenum type, const std::string &src)
{
	GLuint sh = glCreateShader(type);
	const char *cs = src.c_str();
	glShaderSource(sh, 1, &cs, nullptr);
	glCompileShader(sh);
	GLint ok = 0;
	glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
	if (!ok) {
		char log[2048];
		glGetShaderInfoLog(sh, sizeof(log), nullptr, log);
		fprintf(stderr, "[d3d8gles] shader compile FAILED:\n%s\n--- source ---\n%s\n", log, cs);
		glDeleteShader(sh);
		return 0;
	}
	return sh;
}

WebGLPipeline::ProgramInfo *WebGLPipeline::getProgram(WebGLDevice *dev, unsigned fvf,bool multiview)
{
	const uint64_t key = computeProgramKey(dev, fvf);
	for (int i = 0; i < m_programCount; i++) {
		if (m_programs[i].key == key && m_programs[i].multiview==multiview) return m_programs[i].prog;
	}

	if(m_programCount>=kMaxPrograms)return nullptr;
	FVFLayout l;
	parseFVF(fvf, &l);
	const bool lighting = dev->getRenderState(D3DRS_LIGHTING) != 0 && l.hasNormal && !l.xyzrhw;
	// D3DMCS_COLOR1 == 1; VertexMaterialClass::Apply drives these states and
	// the W3D default is MATERIAL (skinned meshes carry diffuse=0 in the VB).
	const bool cvOn = dev->getRenderState(D3DRS_COLORVERTEX) != 0 && l.hasDiffuse;
	const bool diffFromVertex = lighting && cvOn && dev->getRenderState(D3DRS_DIFFUSEMATERIALSOURCE) == 1;
	const bool ambFromVertex = lighting && cvOn && dev->getRenderState(D3DRS_AMBIENTMATERIALSOURCE) == 1;
	const bool emisFromVertex = lighting && cvOn && dev->getRenderState(D3DRS_EMISSIVEMATERIALSOURCE) == 1;
	const bool fog = dev->getRenderState(D3DRS_FOGENABLE) != 0 && !l.xyzrhw;
	const bool shroud = shroudFoldFor(l.xyzrhw);
	const bool alphaTest = dev->getRenderState(D3DRS_ALPHATESTENABLE) != 0;
	const unsigned alphaFunc = dev->getRenderState(D3DRS_ALPHAFUNC) ? dev->getRenderState(D3DRS_ALPHAFUNC) : D3DCMP_ALWAYS;

	StageKey st[2];
	int stagesUsed = 0;
	for (int s = 0; s < 2; s++) {
		getStageKey(dev, s, &st[s]);
		// Must stay identical to computeProgramKey's handling above, or the
		// cache key and the generated shader describe different pipelines.
		if (!dev->getTexture2D(s)) collapseStageWithoutTexture(&st[s], s);
		if (st[s].colorOp != D3DTOP_DISABLE) stagesUsed = s + 1;
	}

	// ---------------- vertex shader ----------------
	std::string vs;
	vs += "#version 300 es\n";
	if(multiview)vs += GX_XR_MULTIVIEW_PREAMBLE;
	vs += "precision highp float;\n";
	vs += l.xyzrhw ? "layout(location=0) in vec4 aPos;\n" : "layout(location=0) in vec3 aPos;\n";
	if (l.hasNormal) vs += "layout(location=1) in vec3 aNormal;\n";
	if (l.hasDiffuse) vs += "layout(location=2) in vec4 aColor0;\n";
	if (l.hasSpecular) vs += "layout(location=3) in vec4 aColor1;\n";
	const int texIn = l.texCount > 2 ? 2 : l.texCount;
	for (int i = 0; i < texIn; i++) {
		char b[64];
		snprintf(b, sizeof(b), "layout(location=%d) in vec4 aUV%d;\n", 4 + i, i);
		vs += b;
	}
	vs += "uniform mat4 uWorld;\n";
	if(m_xrMode) vs += multiview ? GX_XR_MULTIVIEW_VERTEX_DECL:GX_XR_STEREO_VERTEX_DECL;
	// GeneralsX @build Android port GLES experiment 08/30/2026 view/proj as a
	// uniform block (see kViewProjUBOBinding's comment) instead of two plain
	// uniforms -- block member names (uView/uProj) are referenced exactly
	// the same as before in the shader body below, no other change needed
	// there.
	vs += "uniform ViewProjBlock { mat4 uView; mat4 uProj; };\n";
	vs += "uniform vec4 uViewportPos;\n"; // x, y, w, h
	vs += "uniform float uYFlip;\n"; // +1 backbuffer, -1 render-to-texture
	if (shroud) vs += "uniform vec4 uShroudST;\nout highp vec2 vShroudUV;\n";
	vs += "uniform mat4 uTexMat0, uTexMat1;\n";
	vs += "out vec4 vCol;\nout vec4 vSpec;\nout vec2 vUV0;\nout vec2 vUV1;\nout float vFogDepth;\n";
	if (lighting) {
		vs += "uniform vec4 uMatDiffuse, uMatAmbient, uMatEmissive, uGlobalAmbient;\n";
		vs += "uniform int uNumLights;\n";
		vs += "uniform int uLightType[4];\nuniform vec3 uLightDir[4];\nuniform vec3 uLightPos[4];\n";
		vs += "uniform vec4 uLightDiffuse[4];\nuniform vec4 uLightAmbient[4];\nuniform vec4 uLightAtten[4];\n"; // atten: range, a0, a1, a2
	}
	vs += "void main() {\n";
	if(m_xrMode) vs += "  vXrBoard=vec3(0.0);\n";
	if (l.xyzrhw) {
		vs += "  vec4 vpos = vec4(0.0);\n";
		vs += "  float nx = ((aPos.x - uViewportPos.x - 0.5) / uViewportPos.z) * 2.0 - 1.0;\n";
		vs += "  float ny = 1.0 - ((aPos.y - uViewportPos.y - 0.5) / uViewportPos.w) * 2.0;\n";
		vs += "  gl_Position = vec4(nx, ny * uYFlip, aPos.z * 2.0 - 1.0, 1.0);\n";
		vs += "  vFogDepth = 0.0;\n";
	} else {
		vs += "  vec4 wpos = uWorld * vec4(aPos, 1.0);\n";
			vs += "  vec4 vpos = uView * wpos;\n";
			vs += "  vec4 cpos = uProj * vpos;\n";
			// GeneralsX @build Android port GLES experiment - this used to be
			// "-cpos.y * uYFlip" (an unconditional negation). D3D's clip.y=+1
			// already means "top of screen" by construction of D3D's own
			// viewport transform (verified with concrete NDC/window-coordinate
			// arithmetic), and GL's NDC.y=+1 independently also means "top of
			// screen" once GL's own viewport transform + display scanout are
			// accounted for -- so feeding D3D's clip.y into GL as-is needs NO
			// extra negation. The old blanket "-cpos.y" silently flipped every
			// draw through this path, which includes both real 3D camera
			// content AND Render2DClass's 2D UI trick (identity world/view/proj
			// matrices, with Y already pre-flipped screen-to-NDC on the CPU
			// side in Render2DClass::Convert_Vert, GeneralsMD/Code/.../
			// render2d.cpp) -- confirmed via a real device screenshot showing
			// the ENTIRE frame (menu buttons, logos, and the 3D background
			// scene) upside down in reversed top-to-bottom order versus a
			// known-correct reference.
			vs += "  gl_Position = vec4(cpos.x, cpos.y * uYFlip, cpos.z * 2.0 - cpos.w, cpos.w);\n";
			vs += "  vFogDepth = -vpos.z;\n";
		if(m_xrMode) vs += multiview ? GX_XR_MULTIVIEW_VERTEX_BODY:GX_XR_STEREO_VERTEX_BODY;
		// Shroud UV from the world position, as the D3D shroud pass derived it
		// from camera-space position times (inverse view * offset * scale).
		if (shroud) vs += "  vShroudUV = (wpos.xy + uShroudST.zw) * uShroudST.xy;\n";
	}
	// Diffuse color: vertex color (BGRA attribute swizzle) / lighting / white.
	if (lighting) {
		vs += "  vec3 wnrm = normalize(mat3(uWorld) * aNormal);\n";
		// Material color sources per D3DRS_*MATERIALSOURCE (COLOR1 = vertex).
		vs += diffFromVertex ? "  vec4 matDiff = aColor0.zyxw;\n"
		                     : "  vec4 matDiff = uMatDiffuse;\n";
		vs += ambFromVertex ? "  vec3 matAmb = aColor0.zyx;\n"
		                    : "  vec3 matAmb = uMatAmbient.rgb;\n";
		vs += emisFromVertex ? "  vec3 matEmis = aColor0.zyx;\n"
		                     : "  vec3 matEmis = uMatEmissive.rgb;\n";
		vs += "  vec3 accum = matEmis + uGlobalAmbient.rgb * matAmb;\n";
		vs += "  for (int i = 0; i < uNumLights; i++) {\n";
		vs += "    vec3 L; float atten = 1.0;\n";
		vs += "    if (uLightType[i] == 1) {\n"; // POINT
		vs += "      vec3 d = uLightPos[i] - wpos.xyz; float dist = length(d);\n";
		vs += "      if (dist > uLightAtten[i].x) { continue; }\n";
		vs += "      L = d / max(dist, 0.0001);\n";
		vs += "      atten = 1.0 / (uLightAtten[i].y + uLightAtten[i].z * dist + uLightAtten[i].w * dist * dist);\n";
		vs += "    } else { L = -uLightDir[i]; }\n";
		vs += "    float ndl = max(dot(wnrm, L), 0.0);\n";
		vs += "    accum += uLightAmbient[i].rgb * matAmb * atten;\n";
		vs += "    accum += uLightDiffuse[i].rgb * matDiff.rgb * ndl * atten;\n";
		vs += "  }\n";
		vs += "  vCol = vec4(clamp(accum, 0.0, 1.0), matDiff.a);\n";
	} else if (l.hasDiffuse) {
		vs += "  vCol = aColor0.zyxw;\n";
	} else {
		vs += "  vCol = vec4(1.0);\n";
	}
	vs += l.hasSpecular ? "  vSpec = aColor1.zyxw;\n" : "  vSpec = vec4(0.0);\n";

	// Per-stage texcoords (selected input set / texgen + optional transform).
	for (int s = 0; s < 2; s++) {
		char b[256];
		const int tci = (int)st[s].tci < texIn ? (int)st[s].tci : 0;
		if (st[s].texgen == 1 && !l.xyzrhw) {
			// D3DTSS_TCI_CAMERASPACEPOSITION: coordinates are the view-space
			// position run through the stage texture matrix.
			snprintf(b, sizeof(b), "  vUV%d = (uTexMat%d * vec4(vpos.xyz, 1.0)).xy;\n", s, s);
		} else if (st[s].texgen == 2 && !l.xyzrhw && l.hasNormal) {
			// D3DTSS_TCI_CAMERASPACEREFLECTIONVECTOR: environment mapping.
			snprintf(b, sizeof(b),
				"  vUV%d = (uTexMat%d * vec4(reflect(normalize(vpos.xyz), "
				"normalize(mat3(uView) * mat3(uWorld) * aNormal)), 1.0)).xy;\n", s, s);
		} else if (st[s].texgen == 3 && !l.xyzrhw && l.hasNormal) {
			// D3DTSS_TCI_CAMERASPACENORMAL.
			snprintf(b, sizeof(b),
				"  vUV%d = (uTexMat%d * vec4(normalize(mat3(uView) * mat3(uWorld) * aNormal), 1.0)).xy;\n", s, s);
		} else if (texIn == 0) {
			snprintf(b, sizeof(b), "  vUV%d = vec2(0.0);\n", s);
		} else if (st[s].xform) {
			snprintf(b, sizeof(b), "  vUV%d = (uTexMat%d * vec4(aUV%d.xy, 0.0, 1.0)).xy;\n", s, s, tci);
		} else {
			snprintf(b, sizeof(b), "  vUV%d = aUV%d.xy;\n", s, tci);
		}
		vs += b;
	}
	vs += "}\n";

	// ---------------- fragment shader ----------------
	std::string fs;
	fs += "#version 300 es\nprecision mediump float;\n";
	fs += "uniform sampler2D uTex0;\nuniform sampler2D uTex1;\n";
	fs += "uniform vec4 uTFactor;\nuniform float uAlphaRef;\n";
	fs += "uniform vec4 uFogColor;\nuniform vec2 uFogParams;\n"; // start, end
	fs += "in vec4 vCol;\nin vec4 vSpec;\nin vec2 vUV0;\nin vec2 vUV1;\nin float vFogDepth;\n";
	if (shroud) fs += "uniform sampler2D uShroudTex;\nin highp vec2 vShroudUV;\n";
	fs += "layout(location=0) out vec4 fragColor;\n";
	if(m_xrMode) fs += "layout(location=1) out vec4 xrUI;\n";
	if(m_xrMode) fs += GX_XR_STEREO_FRAGMENT_DECL;
	fs += "void main() {\n";
	if(m_xrMode) fs += GX_XR_STEREO_FRAGMENT_BODY;
	fs += "  vec4 cur = vCol;\n";
	for (int s = 0; s < stagesUsed; s++) {
		char texv[32], texa[32];
		snprintf(texv, sizeof(texv), "tex%d", s);
		snprintf(texa, sizeof(texa), "tex%d.a", s);
		char b[640];
		snprintf(b, sizeof(b), "  vec4 tex%d = texture(uTex%d, vUV%d);\n", s, s, s);
		fs += b;
		int usesTex = 0;
		std::string c0 = combinerArg(st[s].colorArg0, texv, &usesTex);
		std::string c1 = combinerArg(st[s].colorArg1, texv, &usesTex);
		std::string c2 = combinerArg(st[s].colorArg2, texv, &usesTex);
		std::string a0 = combinerArg(st[s].alphaArg0, texv, &usesTex);
		std::string a1 = combinerArg(st[s].alphaArg1, texv, &usesTex);
		std::string a2 = combinerArg(st[s].alphaArg2, texv, &usesTex);
		// At stage 0, D3DTA_CURRENT reads DIFFUSE.
		std::string colorExpr = combinerOp(st[s].colorOp, c0, c1, c2, texa);
		std::string alphaExpr =
			st[s].alphaOp == D3DTOP_DISABLE ? "cur" : combinerOp(st[s].alphaOp, a0, a1, a2, texa);
		snprintf(b, sizeof(b), "  cur = vec4((%s).rgb, (%s).a);\n", colorExpr.c_str(), alphaExpr.c_str());
		fs += b;
	}
	if (alphaTest) {
		const char *cmp = nullptr;
		switch (alphaFunc) {
		case D3DCMP_NEVER: cmp = "true"; break; // discard always
		case D3DCMP_LESS: cmp = "cur.a >= uAlphaRef"; break;
		case D3DCMP_EQUAL: cmp = "cur.a != uAlphaRef"; break;
		case D3DCMP_LESSEQUAL: cmp = "cur.a > uAlphaRef"; break;
		case D3DCMP_GREATER: cmp = "cur.a <= uAlphaRef"; break;
		case D3DCMP_NOTEQUAL: cmp = "cur.a == uAlphaRef"; break;
		case D3DCMP_GREATEREQUAL: cmp = "cur.a < uAlphaRef"; break;
		default: cmp = nullptr; break;
		}
		if (cmp) {
			fs += std::string("  if (") + cmp + ") discard;\n";
		}
	}
	if (fog) {
		fs += "  float f = clamp((uFogParams.y - vFogDepth) / max(uFogParams.y - uFogParams.x, 0.0001), 0.0, 1.0);\n";
		fs += "  cur.rgb = mix(uFogColor.rgb, cur.rgb, f);\n";
	}
	// The original second pass multiplied the finished (fogged) opaque pixel
	// by the shroud texel; folding applies the same product to this fragment.
	if (shroud) fs += "  cur.rgb *= texture(uShroudTex, vShroudUV).rgb;\n";
	if(m_xrMode) fs += GX_XR_STEREO_COVERAGE_BODY;
	fs += "  fragColor = cur;\n";
	if(m_xrMode) fs += "  xrUI = cur;\n";
	fs += "}\n";

	// GeneralsX @feature Android port 09/09/2026 One-shot, release-included dump of
	// the combiner this backend actually generates for the two-stage greyscale pair
	// (stage 0 MULTIPLYADD, stage 1 DOTPRODUCT3). It is used by exactly two places
	// -- Render2DClass::Render()'s disabled command-bar buttons and
	// ScreenBWFilterDOT3 (the scripted black-and-white cinematics) -- and both have
	// already been mis-rendered once by an operand-order bug in that combiner that
	// no log would have shown. Printed at most once per launch, at shader-build
	// time, never per frame. Safe to delete once the black-and-white shots are
	// confirmed good on a device.
	if (st[1].colorOp == D3DTOP_DOTPRODUCT3) {
		static bool s_dumpedDot3 = false;
		if (!s_dumpedDot3) {
			s_dumpedDot3 = true;
			fprintf(stderr, "[GX-FILTER] dot3 program: stage0 op=%u arg0=0x%x arg1=0x%x arg2=0x%x | "
				"stage1 op=%u arg1=0x%x arg2=0x%x | stagesUsed=%d fvf=0x%x\n",
				st[0].colorOp, st[0].colorArg0, st[0].colorArg1, st[0].colorArg2,
				st[1].colorOp, st[1].colorArg1, st[1].colorArg2, stagesUsed, fvf);
			fprintf(stderr, "[GX-FILTER] dot3 fragment shader:\n%s", fs.c_str());
			fflush(stderr);
		}
	}

	// ---------------- link ----------------
	const std::chrono::steady_clock::time_point buildStart = std::chrono::steady_clock::now();
	GLuint vsh = compileShader(GL_VERTEX_SHADER, vs);
	GLuint fsh = compileShader(GL_FRAGMENT_SHADER, fs);
	ProgramInfo *info = new ProgramInfo();
	if (vsh && fsh) {
		GLuint p = glCreateProgram();
		glAttachShader(p, vsh);
		glAttachShader(p, fsh);
		glLinkProgram(p);
		GLint ok = 0;
		glGetProgramiv(p, GL_LINK_STATUS, &ok);
		if (!ok) {
			char log[2048];
			glGetProgramInfoLog(p, sizeof(log), nullptr, log);
			fprintf(stderr, "[d3d8gles] program link FAILED: %s\n", log);
			glDeleteProgram(p);
			p = 0;
		}
		info->prog = p;
	}
	if (vsh) glDeleteShader(vsh);
	if (fsh) glDeleteShader(fsh);
	m_perfProgramBuilds++;
	m_perfProgramBuildUs += std::chrono::duration<double, std::micro>(
		std::chrono::steady_clock::now() - buildStart).count();

	if (info->prog) {
		GLuint p = info->prog;
		info->uWorld = glGetUniformLocation(p, "uWorld");
		info->uShroudTex = glGetUniformLocation(p, "uShroudTex");info->uShroudST = glGetUniformLocation(p, "uShroudST");
		if(m_xrMode) {
			info->uXrActive=glGetUniformLocation(p,"uXrActive");info->uXrEyeClip=glGetUniformLocation(p,multiview ? "uXrEyes[0]":"uXrEyeClip");
			info->uXrCamera=glGetUniformLocation(p,"uXrCamera");info->uXrViewSpace=glGetUniformLocation(p,"uXrViewSpace");
			info->uXrBoard=glGetUniformLocation(p,"uXrBoard");info->uXrAspect=glGetUniformLocation(p,"uXrAspect");
			info->uXrOpaque=glGetUniformLocation(p,"uXrOpaque");
		}
		// GeneralsX @build Android port GLES experiment 08/30/2026 Bind this
		// program's ViewProjBlock to the shared UBO binding point once,
		// here at link time -- not per-draw. A program whose xyzrhw (2D)
		// vertex path never references uView/uProj (see the xyzrhw branch
		// just above, which only uses uYFlip/uViewportPos) has the whole
		// block optimized out entirely, so blockIdx is GL_INVALID_INDEX
		// there -- expected, not an error.
		{
			GLuint blockIdx = glGetUniformBlockIndex(p, "ViewProjBlock");
			if (blockIdx != GL_INVALID_INDEX) {
				glUniformBlockBinding(p, blockIdx, kViewProjUBOBinding);
			}
		}
		info->uViewportPos = glGetUniformLocation(p, "uViewportPos");
		info->uYFlip = glGetUniformLocation(p, "uYFlip");
		info->uTex0 = glGetUniformLocation(p, "uTex0");
		info->uTex1 = glGetUniformLocation(p, "uTex1");
		info->uTexMat0 = glGetUniformLocation(p, "uTexMat0");
		info->uTexMat1 = glGetUniformLocation(p, "uTexMat1");
		info->uTFactor = glGetUniformLocation(p, "uTFactor");
		info->uAlphaRef = glGetUniformLocation(p, "uAlphaRef");
		info->uFogColor = glGetUniformLocation(p, "uFogColor");
		info->uFogParams = glGetUniformLocation(p, "uFogParams");
		info->uMatDiffuse = glGetUniformLocation(p, "uMatDiffuse");
		info->uMatAmbient = glGetUniformLocation(p, "uMatAmbient");
		info->uMatEmissive = glGetUniformLocation(p, "uMatEmissive");
		info->uGlobalAmbient = glGetUniformLocation(p, "uGlobalAmbient");
		info->uNumLights = glGetUniformLocation(p, "uNumLights");
		info->uLightType = glGetUniformLocation(p, "uLightType[0]");
		info->uLightDir = glGetUniformLocation(p, "uLightDir[0]");
		info->uLightPos = glGetUniformLocation(p, "uLightPos[0]");
		info->uLightDiffuse = glGetUniformLocation(p, "uLightDiffuse[0]");
		info->uLightAmbient = glGetUniformLocation(p, "uLightAmbient[0]");
		info->uLightAtten = glGetUniformLocation(p, "uLightAtten[0]");
	}
	info->stageTci[0] = st[0].tci;
	info->stageTci[1] = st[1].tci;
	info->stageXform[0] = st[0].xform;
	info->stageXform[1] = st[1].xform;
	info->stagesUsed = stagesUsed;

	if (m_programCount < kMaxPrograms) {
		m_programs[m_programCount].key = key;
		m_programs[m_programCount].multiview=multiview;
		m_programs[m_programCount].prog = info;
		m_programCount++;
		GLTRACE("program cached (%d total), key=%llx", m_programCount, (unsigned long long)key);
	} else {
		WARN_ONCE(s_progOverflow, "program cache overflow (>%d)", kMaxPrograms);
	}
	return info;
}

// ---------------------------------------------------------------------------
// Texture upload / sampler state
// ---------------------------------------------------------------------------

// Converts one level's shadow bits into a GL-uploadable buffer.
// Returns internalFormat/format/type and (possibly converted) pixels.
struct UploadDesc {
	GLenum internalFormat, format, type;
	bool compressed;
	const uint8_t *pixels;
	uint32_t compressedSize;
	std::vector<uint8_t> converted;
};

// ---------------------------------------------------------------------------
// Software DXT/BC1-3 decompression fallback. Most Android GPUs (Mali,
// Adreno, PowerVR) don't expose GL_EXT_texture_compression_s3tc -- without
// this, every DXT-compressed asset (chiefly terrain .dds, not the mostly-
// uncompressed-TGA menu art) fell through to prepareLevelUpload's "unknown
// format" case and got uploaded as solid magenta (see uploadTexture below).
// This decodes one whole compressed level into a tightly packed RGBA8
// buffer, block by block per the standard S3TC/BC1-BC3 bit layout, so it
// can be uploaded via the ordinary uncompressed glTexImage2D path instead.
// One-time cost per texture level at upload time (uploadTexture runs on
// dirty, not per frame), so a straightforward scalar decode is fine.
// ---------------------------------------------------------------------------

static inline void UnpackRGB565(uint16_t c, uint8_t *r, uint8_t *g, uint8_t *b)
{
	*r = (uint8_t)(((c >> 11) & 0x1F) * 255 / 31);
	*g = (uint8_t)(((c >> 5)  & 0x3F) * 255 / 63);
	*b = (uint8_t)(( c        & 0x1F) * 255 / 31);
}

// Decodes one 8-byte BC1/DXT1-style color block (also the RGB half of
// BC2/BC3) into a 4x4 RGBA8 array (row-major, 16 texels, 4 bytes each).
// hasAlpha selects DXT1's punch-through-alpha special case (color0 <=
// color1 numerically => palette entries 2/3 collapse to a 50% blend +
// transparent black). BC2/BC3 callers pass hasAlpha=false since those
// formats carry alpha in a separate 8-byte block and always treat this
// block as opaque 4-color; the alpha channel written here gets overwritten
// by the caller afterwards.
static void DecodeBC1Block(const uint8_t block[8], bool hasAlpha, uint8_t outRGBA[16 * 4])
{
	const uint16_t c0 = (uint16_t)(block[0] | (block[1] << 8));
	const uint16_t c1 = (uint16_t)(block[2] | (block[3] << 8));
	uint8_t r0, g0, b0, r1, g1, b1;
	UnpackRGB565(c0, &r0, &g0, &b0);
	UnpackRGB565(c1, &r1, &g1, &b1);

	uint8_t pal[4][4]; // [index][RGBA]
	pal[0][0] = r0; pal[0][1] = g0; pal[0][2] = b0; pal[0][3] = 255;
	pal[1][0] = r1; pal[1][1] = g1; pal[1][2] = b1; pal[1][3] = 255;
	const bool punchThrough = hasAlpha && c0 <= c1;
	if (!punchThrough) {
		pal[2][0] = (uint8_t)((2 * r0 + r1) / 3);
		pal[2][1] = (uint8_t)((2 * g0 + g1) / 3);
		pal[2][2] = (uint8_t)((2 * b0 + b1) / 3);
		pal[2][3] = 255;
		pal[3][0] = (uint8_t)((r0 + 2 * r1) / 3);
		pal[3][1] = (uint8_t)((g0 + 2 * g1) / 3);
		pal[3][2] = (uint8_t)((b0 + 2 * b1) / 3);
		pal[3][3] = 255;
	} else {
		pal[2][0] = (uint8_t)((r0 + r1) / 2);
		pal[2][1] = (uint8_t)((g0 + g1) / 2);
		pal[2][2] = (uint8_t)((b0 + b1) / 2);
		pal[2][3] = 255;
		pal[3][0] = 0; pal[3][1] = 0; pal[3][2] = 0; pal[3][3] = 0; // transparent
	}

	const uint32_t indices = (uint32_t)block[4] | ((uint32_t)block[5] << 8) |
	                          ((uint32_t)block[6] << 16) | ((uint32_t)block[7] << 24);
	for (int i = 0; i < 16; i++) {
		const int idx = (indices >> (i * 2)) & 0x3;
		outRGBA[i * 4 + 0] = pal[idx][0];
		outRGBA[i * 4 + 1] = pal[idx][1];
		outRGBA[i * 4 + 2] = pal[idx][2];
		outRGBA[i * 4 + 3] = pal[idx][3];
	}
}

// Decodes one 8-byte BC3/DXT5-style interpolated alpha block into 16
// 8-bit alpha values (row-major, matches DecodeBC1Block's texel order).
static void DecodeBC3AlphaBlock(const uint8_t block[8], uint8_t outAlpha[16])
{
	const uint8_t a0 = block[0];
	const uint8_t a1 = block[1];
	uint8_t pal[8];
	pal[0] = a0;
	pal[1] = a1;
	if (a0 > a1) {
		for (int i = 1; i <= 6; i++)
			pal[1 + i] = (uint8_t)(((7 - i) * a0 + i * a1) / 7);
	} else {
		for (int i = 1; i <= 4; i++)
			pal[1 + i] = (uint8_t)(((5 - i) * a0 + i * a1) / 5);
		pal[6] = 0;
		pal[7] = 255;
	}
	// 6 bytes = 48 bits = 16 * 3-bit indices, little-endian bit-packed.
	uint64_t bits = 0;
	for (int i = 0; i < 6; i++)
		bits |= (uint64_t)block[2 + i] << (8 * i);
	for (int i = 0; i < 16; i++) {
		const int idx = (int)((bits >> (i * 3)) & 0x7);
		outAlpha[i] = pal[idx];
	}
}

// Decodes one whole DXT1/2/3/4/5-compressed level into a tightly packed
// w*h*4 RGBA8 buffer. Reads full 4x4 blocks from src (compressed data is
// always stored in whole blocks, even for the last row/column of a
// non-multiple-of-4 or sub-4x4 mip) but only writes the w x h texels that
// are actually in bounds into out. Returns false (leaving out alone) if
// srcSize is too small for the implied block count, so the caller can
// still fall back safely instead of reading out of bounds.
static bool DecodeDXTLevel(D3DFORMAT fmt, unsigned w, unsigned h,
                           const uint8_t *src, size_t srcSize,
                           std::vector<uint8_t> *out)
{
	const bool isDXT1 = (fmt == D3DFMT_DXT1);
	const unsigned blockBytes = isDXT1 ? 8 : 16;
	const unsigned blocksWide = (w + 3) / 4;
	const unsigned blocksHigh = (h + 3) / 4;
	const size_t needed = (size_t)blocksWide * blocksHigh * blockBytes;
	if (needed > srcSize) return false; // truncated/corrupt data, bail to magenta

	out->resize((size_t)w * h * 4);
	uint8_t *dst = out->data();

	for (unsigned by = 0; by < blocksHigh; by++) {
		for (unsigned bx = 0; bx < blocksWide; bx++) {
			const uint8_t *blockSrc = src + ((size_t)by * blocksWide + bx) * blockBytes;
			uint8_t rgba[16 * 4];
			uint8_t alpha[16]; // only used for BC2/BC3

			if (isDXT1) {
				DecodeBC1Block(blockSrc, /*hasAlpha=*/true, rgba);
			} else if (fmt == D3DFMT_DXT2 || fmt == D3DFMT_DXT3) {
				// Explicit 4-bit-per-texel alpha: 8 bytes, 2 nibbles/byte,
				// texel order matches the RGB block's row-major layout.
				DecodeBC1Block(blockSrc + 8, /*hasAlpha=*/false, rgba);
				for (int i = 0; i < 16; i++) {
					const uint8_t nibble = (blockSrc[i / 2] >> ((i & 1) * 4)) & 0xF;
					alpha[i] = (uint8_t)(nibble * 17); // 4-bit -> 8-bit (0,17,...,255)
				}
				for (int i = 0; i < 16; i++) rgba[i * 4 + 3] = alpha[i];
			} else { // DXT4 / DXT5: interpolated alpha block
				DecodeBC3AlphaBlock(blockSrc, alpha);
				DecodeBC1Block(blockSrc + 8, /*hasAlpha=*/false, rgba);
				for (int i = 0; i < 16; i++) rgba[i * 4 + 3] = alpha[i];
			}

			// Scatter the 4x4 block into the w x h output, clipping at
			// the right/bottom edge for non-multiple-of-4 dimensions.
			const unsigned baseX = bx * 4, baseY = by * 4;
			const unsigned maxX = (baseX + 4 <= w) ? 4 : (w - baseX);
			const unsigned maxY = (baseY + 4 <= h) ? 4 : (h - baseY);
			for (unsigned ty = 0; ty < maxY; ty++) {
				for (unsigned tx = 0; tx < maxX; tx++) {
					const uint8_t *srcTexel = &rgba[(ty * 4 + tx) * 4];
					uint8_t *dstTexel = &dst[((size_t)(baseY + ty) * w + (baseX + tx)) * 4];
					dstTexel[0] = srcTexel[0];
					dstTexel[1] = srcTexel[1];
					dstTexel[2] = srcTexel[2];
					dstTexel[3] = srcTexel[3];
				}
			}
		}
	}
	return true;
}

static bool prepareLevelUpload(D3DFORMAT fmt, unsigned w, unsigned h,
                               const uint8_t *src, size_t srcSize, bool hasS3TC, UploadDesc *out)
{
	out->compressed = false;
	out->compressedSize = 0;
	out->pixels = src;
	switch (fmt) {
	case D3DFMT_A8R8G8B8:
	case D3DFMT_X8R8G8B8: {
		// BGRA bytes -> RGBA
		out->converted.resize((size_t)w * h * 4);
		const bool forceOpaque = (fmt == D3DFMT_X8R8G8B8);
		for (size_t i = 0; i < (size_t)w * h; i++) {
			out->converted[i * 4 + 0] = src[i * 4 + 2];
			out->converted[i * 4 + 1] = src[i * 4 + 1];
			out->converted[i * 4 + 2] = src[i * 4 + 0];
			out->converted[i * 4 + 3] = forceOpaque ? 255 : src[i * 4 + 3];
		}
		out->pixels = out->converted.data();
		out->internalFormat = GL_RGBA;
		out->format = GL_RGBA;
		out->type = GL_UNSIGNED_BYTE;
		return true;
	}
	case D3DFMT_R5G6B5:
		out->internalFormat = GL_RGB565;
		out->format = GL_RGB;
		out->type = GL_UNSIGNED_SHORT_5_6_5;
		return true;
	case D3DFMT_A4R4G4B4: {
		// ARGB4444 -> RGBA4444 (per-short component rotate)
		out->converted.resize((size_t)w * h * 2);
		const uint16_t *s = (const uint16_t *)src;
		uint16_t *d = (uint16_t *)out->converted.data();
		for (size_t i = 0; i < (size_t)w * h; i++) {
			const uint16_t v = s[i];
			const uint16_t a = (v >> 12) & 0xF, r = (v >> 8) & 0xF, g = (v >> 4) & 0xF, b = v & 0xF;
			d[i] = (uint16_t)((r << 12) | (g << 8) | (b << 4) | a);
		}
		out->pixels = out->converted.data();
		out->internalFormat = GL_RGBA4;
		out->format = GL_RGBA;
		out->type = GL_UNSIGNED_SHORT_4_4_4_4;
		return true;
	}
	case D3DFMT_A1R5G5B5:
	case D3DFMT_X1R5G5B5: {
		out->converted.resize((size_t)w * h * 2);
		const uint16_t *s = (const uint16_t *)src;
		uint16_t *d = (uint16_t *)out->converted.data();
		const bool opaque = (fmt == D3DFMT_X1R5G5B5);
		for (size_t i = 0; i < (size_t)w * h; i++) {
			const uint16_t v = s[i];
			const uint16_t a = opaque ? 1 : ((v >> 15) & 0x1);
			const uint16_t r = (v >> 10) & 0x1F, g = (v >> 5) & 0x1F, b = v & 0x1F;
			d[i] = (uint16_t)((r << 11) | (g << 6) | (b << 1) | a);
		}
		out->pixels = out->converted.data();
		out->internalFormat = GL_RGB5_A1;
		out->format = GL_RGBA;
		out->type = GL_UNSIGNED_SHORT_5_5_5_1;
		return true;
	}
	case D3DFMT_L8:
		out->internalFormat = GL_LUMINANCE;
		out->format = GL_LUMINANCE;
		out->type = GL_UNSIGNED_BYTE;
		return true;
	case D3DFMT_A8:
		out->internalFormat = GL_ALPHA;
		out->format = GL_ALPHA;
		out->type = GL_UNSIGNED_BYTE;
		return true;
	case D3DFMT_A8L8:
		out->internalFormat = GL_LUMINANCE_ALPHA;
		out->format = GL_LUMINANCE_ALPHA;
		out->type = GL_UNSIGNED_BYTE;
		return true;
	case D3DFMT_DXT1:
	case D3DFMT_DXT2:
	case D3DFMT_DXT3:
	case D3DFMT_DXT4:
	case D3DFMT_DXT5: {
		if (!hasS3TC) {
			if (DecodeDXTLevel(fmt, w, h, src, srcSize, &out->converted)) {
				WARN_ONCE(s_dxtSoftDecode, "DXT texture but WEBGL_compressed_texture_s3tc "
					"missing; using software BC1-3 decode fallback");
				out->pixels = out->converted.data();
				out->internalFormat = GL_RGBA;
				out->format = GL_RGBA;
				out->type = GL_UNSIGNED_BYTE;
				return true;
			}
			WARN_ONCE(s_noS3tc, "DXT texture but WEBGL_compressed_texture_s3tc missing "
				"and software decode failed (truncated data?)");
			return false;
		}
		out->compressed = true;
		out->compressedSize = (uint32_t)srcSize;
		switch (fmt) {
		case D3DFMT_DXT1: out->internalFormat = 0x83F1; break; // COMPRESSED_RGBA_S3TC_DXT1_EXT
		case D3DFMT_DXT2:
		case D3DFMT_DXT3: out->internalFormat = 0x83F2; break; // DXT3
		default: out->internalFormat = 0x83F3; break;          // DXT5
		}
		return true;
	}
	default: {
		// Repeat (capped) so the offender survives the console ring buffer.
		static int s_fmtLogs = 0;
		if (s_fmtLogs < 20) {
			s_fmtLogs++;
			fprintf(stderr, "[d3d8gles] MAGENTA: texture format %d (0x%x) not implemented %ux%u\n",
				(int)fmt, (unsigned)fmt, w, h);
		}
		return false;
	}
	}
}

void WebGLPipeline::uploadTexture(WebGLTexture *tex)
{
	GLTextureState &g = tex->m_gl;
	if (g.name == 0) {
		glGenTextures(1, &g.name);
		g_texturesCreated++;
	}
	glBindTexture(GL_TEXTURE_2D, g.name);
	const int levels = (int)tex->m_levels.size();
	const bool isDXT = FormatIsDXT(tex->m_format);

	int uploaded = 0;
	for (int lvl = 0; lvl < levels; lvl++) {
		WebGLSurface *s = tex->m_levels[lvl];
		UploadDesc up;
		if (!prepareLevelUpload(tex->m_format, s->m_width, s->m_height,
		                        s->m_bits.data(), s->m_bits.size(), m_hasS3TC, &up)) {
			// Unknown format: upload magenta so it is visible, not crashy.
			std::vector<uint8_t> mag((size_t)s->m_width * s->m_height * 4);
			for (size_t i = 0; i < mag.size(); i += 4) {
				mag[i] = 255; mag[i + 1] = 0; mag[i + 2] = 255; mag[i + 3] = 255;
			}
			glTexImage2D(GL_TEXTURE_2D, lvl, GL_RGBA, s->m_width, s->m_height, 0,
			             GL_RGBA, GL_UNSIGNED_BYTE, mag.data());
			uploaded = lvl + 1;
			continue;
		}
		if (up.compressed) {
			glCompressedTexImage2D(GL_TEXTURE_2D, lvl, up.internalFormat,
			                       s->m_width, s->m_height, 0, up.compressedSize, up.pixels);
			const GLenum cerr = glGetError();
			if (cerr != GL_NO_ERROR) {
				fprintf(stderr, "[d3d8gles] DXT upload error 0x%x lvl=%d %ux%u fmt=0x%x size=%u\n",
					cerr, lvl, s->m_width, s->m_height, (unsigned)tex->m_format, up.compressedSize);
			}
			uploaded = lvl + 1;
		} else {
			glTexImage2D(GL_TEXTURE_2D, lvl, up.internalFormat, s->m_width, s->m_height, 0,
			             up.format, up.type, up.pixels);
			uploaded = lvl + 1;
			if (!isDXT && levels > 1) {
				// The engine frequently fills only level 0 of uncompressed
				// textures; GPU-generate the chain instead of sampling the
				// empty (transparent black) shadow mips. DXT textures
				// (including software-decoded ones, which land in this same
				// branch) always come with a full pre-authored mip chain, so
				// this must keep looping for them instead of stopping after
				// level 0.
				break;
			}
		}
	}
	if (!isDXT && levels > 1) {
		glGenerateMipmap(GL_TEXTURE_2D);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 1000);
	} else {
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, uploaded > 0 ? uploaded - 1 : 0);
	}
	// GeneralsX @build Android port 09/05/2026 A texture that received no
	// glTexImage2D at all is INCOMPLETE in GL and samples as opaque black --
	// silently, with no GL error. That is the exact signature of the reported
	// "half the textures are gone after restarting on Medium detail", and
	// nothing in this function would otherwise say it happened. Capped.
	if (uploaded == 0) {
		static int s_emptyUploads = 0;
		if (s_emptyUploads < 12) {
			s_emptyUploads++;
			fprintf(stderr, "[gxtex] EMPTY upload: texture %ux%u fmt=%d levels=%d "
				"-- incomplete, will sample as black\n",
				levels > 0 ? tex->m_levels[0]->m_width : 0,
				levels > 0 ? tex->m_levels[0]->m_height : 0,
				(int)tex->m_format, levels);
		}
	}
	g.dirty = false;
	g.samplerKey = ~0u; // force sampler reapply
	GLTRACE("texture %u uploaded (%dx%d fmt=%d levels=%d)", g.name,
	        tex->m_levels[0]->m_width, tex->m_levels[0]->m_height, (int)tex->m_format, levels);
}

void WebGLPipeline::applySamplerState(WebGLDevice *dev, unsigned stage, WebGLTexture *tex)
{
	const DWORD minf = dev->getStageState(stage, D3DTSS_MINFILTER);
	const DWORD magf = dev->getStageState(stage, D3DTSS_MAGFILTER);
	const DWORD mipf = dev->getStageState(stage, D3DTSS_MIPFILTER);
	const DWORD au = dev->getStageState(stage, D3DTSS_ADDRESSU);
	const DWORD av = dev->getStageState(stage, D3DTSS_ADDRESSV);
	const uint32_t key = (uint32_t)((minf & 7) | ((magf & 7) << 3) | ((mipf & 7) << 6) |
	                                ((au & 7) << 9) | ((av & 7) << 12));
	if (tex->m_gl.samplerKey == key) return;
	tex->m_gl.samplerKey = key;

	const bool hasMips = tex->m_levels.size() > 1;
	GLenum glMin;
	if (!hasMips || mipf == D3DTEXF_NONE) {
		glMin = (minf == D3DTEXF_POINT) ? GL_NEAREST : GL_LINEAR;
	} else if (mipf == D3DTEXF_POINT) {
		glMin = (minf == D3DTEXF_POINT) ? GL_NEAREST_MIPMAP_NEAREST : GL_LINEAR_MIPMAP_NEAREST;
	} else {
		glMin = (minf == D3DTEXF_POINT) ? GL_NEAREST_MIPMAP_LINEAR : GL_LINEAR_MIPMAP_LINEAR;
	}
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, glMin);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
	                magf == D3DTEXF_POINT ? GL_NEAREST : GL_LINEAR);

	auto addr = [](DWORD m) -> GLenum {
		switch (m) {
		case D3DTADDRESS_MIRROR: return GL_MIRRORED_REPEAT;
		case D3DTADDRESS_CLAMP:
		case D3DTADDRESS_BORDER: return GL_CLAMP_TO_EDGE;
		case D3DTADDRESS_WRAP:
		default: return GL_REPEAT;
		}
	};
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, addr(au ? au : D3DTADDRESS_WRAP));
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, addr(av ? av : D3DTADDRESS_WRAP));
}

void WebGLPipeline::invalidateTextureBinding(GLuint name)
{
	if (name == 0) return; // 0 already means "no texture" to the cache, never a real object
	for (int s = 0; s < 2; s++) {
		if (m_lastBoundTex[s] == name) m_lastBoundTex[s] = ~0u;
	}
}

void WebGLPipeline::bindTextures(WebGLDevice *dev, ProgramInfo *prog)
{
	for (int s = 0; s < 2; s++) {
		WebGLTexture *tex = dev->getTexture2D(s);
		glActiveTexture(GL_TEXTURE0 + s);
		if (tex) {
			if (tex->m_gl.dirty || tex->m_gl.name == 0) {
				uploadTexture(tex); // binds tex->m_gl.name as a side effect
				m_lastBoundTex[s] = tex->m_gl.name;
			} else if (m_lastBoundTex[s] != tex->m_gl.name) {
				glBindTexture(GL_TEXTURE_2D, tex->m_gl.name);
				m_lastBoundTex[s] = tex->m_gl.name;
			}
			applySamplerState(dev, s, tex);
		} else if (m_lastBoundTex[s] != 0) {
			glBindTexture(GL_TEXTURE_2D, 0);
			m_lastBoundTex[s] = 0;
		}
	}
	if (prog->uTex0 >= 0) glUniform1i(prog->uTex0, 0);
	if (prog->uTex1 >= 0) glUniform1i(prog->uTex1, 1);
	if (prog->uShroudTex >= 0) bindShroud(prog);
}

// GeneralsX @performance Claude 26/09/2026 Texture unit 5 is reserved for the
// folded shroud (units 0-1 are D3D stages, 2-4 belong to XR panels/keyboard).
// A sampler object keeps its filter/wrap independent of the terrain shroud
// pass, which binds the same texture through the D3D stage path.
static const int kShroudUnit=5;
void WebGLPipeline::bindShroud(ProgramInfo *prog)
{
	if (!m_shroudTex) return;
	glActiveTexture(GL_TEXTURE0 + kShroudUnit);
	if (m_shroudTex->m_gl.dirty || m_shroudTex->m_gl.name == 0) {
		uploadTexture(m_shroudTex); // binds as a side effect
		m_lastShroudBound = m_shroudTex->m_gl.name;
	} else if (m_lastShroudBound != m_shroudTex->m_gl.name) {
		glBindTexture(GL_TEXTURE_2D, m_shroudTex->m_gl.name);
		m_lastShroudBound = m_shroudTex->m_gl.name;
	}
	if (!m_shroudSampler) {
		glGenSamplers(1, &m_shroudSampler);
		glSamplerParameteri(m_shroudSampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glSamplerParameteri(m_shroudSampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glSamplerParameteri(m_shroudSampler, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glSamplerParameteri(m_shroudSampler, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	}
	glBindSampler(kShroudUnit, m_shroudSampler);
	glActiveTexture(GL_TEXTURE0);
	glUniform1i(prog->uShroudTex, kShroudUnit);
	if (prog->uShroudST >= 0) glUniform4fv(prog->uShroudST, 1, m_shroudST);
}

void WebGLPipeline::setShroudTexture(WebGLTexture *tex,const float *worldToUV)
{
	if (m_xrMode) pollShroudMarker();
	if (tex != m_shroudTex) {
		if (tex) tex->AddRef();
		if (m_shroudTex) m_shroudTex->Release();
		m_shroudTex = tex;
		m_lastShroudBound = ~0u;
	}
	if (worldToUV) memcpy(m_shroudST, worldToUV, sizeof(m_shroudST));
}

// A/B and rollback switch: gx_shroud_twopass.txt in the game's working
// directory restores the original two-pass shroud. The marker is polled from
// setShroudTexture, i.e. once per rendered scene frame (every 60th frame),
// independent of how many fogged meshes are in view.
void WebGLPipeline::pollShroudMarker()
{
	if ((m_shroudMarkerPoll++ % 60) != 0) return;
	FILE *marker = fopen("gx_shroud_twopass.txt", "r");
	const bool disabled = marker != nullptr;
	if (marker) fclose(marker);
	if (disabled != m_shroudFoldDisabled || m_shroudMarkerPoll == 1)
		fprintf(stderr, "[d3d8gles] shroud %s\n", disabled ? "two-pass (marker)" : "single-pass fold");
	m_shroudFoldDisabled = disabled;
}

bool WebGLPipeline::shroudFoldAvailable()
{
	return m_xrMode && !m_shroudFoldDisabled;
}

// ---------------------------------------------------------------------------
// Fixed state + uniforms
// ---------------------------------------------------------------------------

void WebGLPipeline::applyFixedState(WebGLDevice *dev)
{
	const D3DVIEWPORT8 &vpKey = dev->getViewport();
	FixedStateKey key{};
	key.zEnable = dev->getRenderState(D3DRS_ZENABLE);
	key.zWrite = dev->getRenderState(D3DRS_ZWRITEENABLE);
	key.zFunc = dev->getRenderState(D3DRS_ZFUNC);
	key.zBias = dev->getRenderState(D3DRS_ZBIAS);
	key.alphaBlend = dev->getRenderState(D3DRS_ALPHABLENDENABLE);
	key.srcBlend = dev->getRenderState(D3DRS_SRCBLEND);
	key.destBlend = dev->getRenderState(D3DRS_DESTBLEND);
	key.cullMode = dev->getRenderState(D3DRS_CULLMODE);
	key.colorWrite = dev->getRenderState(D3DRS_COLORWRITEENABLE);
	key.stencilEnable = dev->getRenderState(D3DRS_STENCILENABLE);
	key.stencilFunc = dev->getRenderState(D3DRS_STENCILFUNC);
	key.stencilRef = dev->getRenderState(D3DRS_STENCILREF);
	key.stencilMask = dev->getRenderState(D3DRS_STENCILMASK);
	key.stencilFail = dev->getRenderState(D3DRS_STENCILFAIL);
	key.stencilZFail = dev->getRenderState(D3DRS_STENCILZFAIL);
	key.stencilPass = dev->getRenderState(D3DRS_STENCILPASS);
	key.stencilWriteMask = dev->getRenderState(D3DRS_STENCILWRITEMASK);
	key.vpX = vpKey.X;
	key.vpY = vpKey.Y;
	key.vpW = vpKey.Width;
	key.vpH = vpKey.Height;
	key.vpMinZ = vpKey.MinZ;
	key.vpMaxZ = vpKey.MaxZ;

	if (m_haveFixedStateKey && key == m_lastFixedStateKey) {
		m_perfStateCacheHits++;
		return; // Nothing this function sets has changed since the last draw.
	}
	m_perfStateCacheMisses++;
	m_lastFixedStateKey = key;
	m_haveFixedStateKey = true;

	// Depth
	const DWORD zEnable = dev->getRenderState(D3DRS_ZENABLE);
	if (zEnable) glEnable(GL_DEPTH_TEST);
	else glDisable(GL_DEPTH_TEST);
	glDepthMask(dev->getRenderState(D3DRS_ZWRITEENABLE) ? GL_TRUE : GL_FALSE);
	const DWORD zfunc = dev->getRenderState(D3DRS_ZFUNC);
	glDepthFunc(d3dCmpToGL(zfunc ? zfunc : D3DCMP_LESSEQUAL));

	// Blend
	if (dev->getRenderState(D3DRS_ALPHABLENDENABLE)) {
		glEnable(GL_BLEND);
		const DWORD sb = dev->getRenderState(D3DRS_SRCBLEND);
		const DWORD db = dev->getRenderState(D3DRS_DESTBLEND);
		if(m_xrUI && m_curFBO==0) {
			// RGB remains exactly the D3D blend, on BOTH outputs. Alpha records
			// coverage for the isolated UI; ordinary source-alpha draws yield
			// premultiplied RGB over its transparent clear (no squared alpha).
			glBlendFuncSeparate(d3dBlendToGL(sb),d3dBlendToGL(db),GL_ONE,GL_ONE_MINUS_SRC_ALPHA);
		} else glBlendFunc(d3dBlendToGL(sb ? sb : D3DBLEND_ONE), d3dBlendToGL(db ? db : D3DBLEND_ZERO));
	} else {
		glDisable(GL_BLEND);
	}

	// Cull. GeneralsX @build Android port GLES experiment - this used to
	// assume the vertex shader's clip-space y-negate flipped winding once
	// relative to D3D screen space (hence the swapped GL_FRONT/GL_BACK
	// below), compensating for that flip. Now that the y-negate is gone
	// (see the vertex shader's cpos.y comment), winding order matches D3D's
	// own directly with glFrontFace at its GL default (GL_CCW), so the
	// mapping no longer needs the swap: D3DCULL_CW -> GL_BACK,
	// D3DCULL_CCW -> GL_FRONT. The swapped mapping was silently culling
	// nearly everything (terrain, video quads) after the y-negate fix,
	// since front/back faces were now backwards relative to what D3D
	// intended.
	switch (dev->getRenderState(D3DRS_CULLMODE)) {
	case D3DCULL_CW:
		glEnable(GL_CULL_FACE);
		glCullFace(GL_BACK);
		break;
	case D3DCULL_CCW:
		glEnable(GL_CULL_FACE);
		glCullFace(GL_FRONT);
		break;
	default:
		glDisable(GL_CULL_FACE);
		break;
	}

	// Depth bias (D3D8 ZBIAS 0..16 pulls towards the viewer)
	const DWORD zbias = dev->getRenderState(D3DRS_ZBIAS);
	if (zbias) {
		glEnable(GL_POLYGON_OFFSET_FILL);
		glPolygonOffset(-1.0f, -(float)zbias * 2.0f);
	} else {
		glDisable(GL_POLYGON_OFFSET_FILL);
	}

	// Color mask. Zero is a real value: stencil shadow volumes render with
	// COLORWRITEENABLE=0 (stencil-only) - mapping it to "write everything"
	// made every shadow volume a visible black silhouette.
	const DWORD cw = dev->getRenderState(D3DRS_COLORWRITEENABLE);
	glColorMask((cw & 1) != 0, (cw & 2) != 0, (cw & 4) != 0, (cw & 8) != 0);

	// Stencil
	if (dev->getRenderState(D3DRS_STENCILENABLE)) {
		glEnable(GL_STENCIL_TEST);
		// GeneralsX @bugfix Android port 09/05/2026 Two deviations from D3D8
		// semantics used to live in these three calls, and together they broke
		// stencil shadow volumes on GLES while leaving DXVK unaffected.
		//
		// 1. The reference value was cast straight to a signed GLint.
		//    W3DVolumetricShadow's fill passes set D3DRS_STENCILREF =
		//    0x80808080, which as a GLint is -2139062144. GLES 3.0 4.1.4 says
		//    ref is clamped to [0, 2^s - 1], so a negative value clamps to 0.
		// 2. A D3DRS_STENCILMASK of 0 was treated as "never set" and replaced
		//    with 0xFFFFFFFF. Zero is a real D3D value meaning "compare no
		//    bits", i.e. the test always passes -- and it is exactly what the
		//    engine asks for here, since TheW3DShadowManager's stencil shadow
		//    mask is 0 (W3DShadow.cpp) when the player-colour occluder feature
		//    is off. Substituting a full mask turned a guaranteed pass into a
		//    real comparison.
		//
		// Together those made the fill passes run as
		// glStencilFunc(GL_GEQUAL, 0, 0xFF) -- "pass only where the stencil is
		// still 0" -- instead of "always pass". The INCR pass then raised the
		// whole volume silhouette to 1, and the DECR pass, meeting a buffer
		// that now held 1, failed the stencil test everywhere it had just
		// incremented (STENCILFAIL is KEEP, so nothing decremented). The far
		// half never cancelled the near half, the entire silhouette stayed at
		// 1, and the darkening quad painted all of it -- the dark slab
		// reported around aircraft.
		//
		// This also explains the two results that made no sense before:
		// inverting the cull face changed nothing (whichever half draws first
		// paints the silhouette to 1 and locks the other half out through the
		// stencil test, and both halves share a screen footprint), and the
		// occlusion probe still saw samples in the DECR pass (fringe pixels
		// and depth-failed regions were left at 0 and did survive).
		//
		// The masks are now passed through verbatim; the device seeds D3D8's
		// documented stencil defaults so a zero here really does mean the game
		// asked for zero (see WebGLDevice's constructor).
		glStencilFunc(d3dCmpToGL(dev->getRenderState(D3DRS_STENCILFUNC)),
		              (GLint)(dev->getRenderState(D3DRS_STENCILREF) & 0xFFu),
		              (GLuint)dev->getRenderState(D3DRS_STENCILMASK));
		glStencilOp(d3dStencilOpToGL(dev->getRenderState(D3DRS_STENCILFAIL)),
		            d3dStencilOpToGL(dev->getRenderState(D3DRS_STENCILZFAIL)),
		            d3dStencilOpToGL(dev->getRenderState(D3DRS_STENCILPASS)));
		glStencilMask((GLuint)dev->getRenderState(D3DRS_STENCILWRITEMASK));
	} else {
		glDisable(GL_STENCIL_TEST);
	}

	// GeneralsX @build Android port GLES experiment - glViewport's y is
	// measured from the BOTTOM of the render target in GL (always, this is
	// not related to the vertex shader's clip-space convention at all --
	// that's a separate, already-correct concern, see the vertex shader's
	// cpos.y comment); D3D's vp.Y is measured from the TOP. Converting via
	// RTH - vp.Y - vp.Height (the standard D3D->GL viewport translation)
	// gives the correct physical rectangle for both. Passing vp.Y through
	// unconverted only happens to work for a fullscreen viewport (Y=0,
	// Height=RT, where the formula reduces to 0 either way) -- exactly why
	// menus (which Render2DClass always renders through a fullscreen
	// viewport) looked fine while the actual in-game partial 3D viewport
	// (top black band, picking offset by the same amount) did not: confirmed
	// on a real device screenshot during live gameplay.
	const D3DVIEWPORT8 &vp = dev->getViewport();
	const GLint glViewportY = (GLint)(m_curRTHeight - (int)vp.Y - (int)vp.Height);
	glViewport((GLint)vp.X, glViewportY, (GLsizei)vp.Width, (GLsizei)vp.Height);
	glDepthRangef(vp.MinZ, vp.MaxZ);

}

void WebGLPipeline::applyUniforms(WebGLDevice *dev, ProgramInfo *prog, unsigned fvf)
{
	if (prog->prog != m_lastProgram) {
		glUseProgram(prog->prog);
		m_lastProgram = prog->prog;
		// Uniform locations are per-program: a cache hit against a key
		// computed for the PREVIOUS program would wrongly skip uploading to
		// this one, leaving its uniforms unset. Force every sub-block below
		// to treat this draw as a first upload for the newly bound program.
		// GeneralsX @build Android port GLES experiment 08/30/2026
		// m_haveViewProjKey is deliberately NOT reset here anymore -- the
		// camera now lives in a UBO (kViewProjUBOBinding), whose DATA is
		// independent of which program is bound, so a program switch alone
		// no longer means the GPU forgot the camera. This was the single
		// biggest source of uniform-cache misses on a real device (a
		// per-block breakdown showed the combined rate collapsing to ~50%
		// mid-battle purely from program switches between terrain/unit/
		// particle/decal draws, even though the camera itself hadn't
		// changed at all).
		m_haveTexMatKey = m_haveMiscKey = m_haveMaterialKey = m_haveLightingKey = false;
	}

	// GeneralsX @performance Codex 14/09/2026 Uniforms belong to programs,
	// not the current binding. Non-XR keeps its existing single-program cache.
	auto &cache=prog->xrUniforms;
	if(m_xrMode && cache.epoch!=m_xrUniformEpoch) {
		cache={};cache.epoch=m_xrUniformEpoch;
	}
	bool &haveTexMatKey=m_xrMode ? cache.haveTexMatKey:m_haveTexMatKey;
	auto &lastTexMatKey=m_xrMode ? cache.lastTexMatKey:m_lastTexMatKey;
	bool &haveMiscKey=m_xrMode ? cache.haveMiscKey:m_haveMiscKey;
	auto &lastMiscKey=m_xrMode ? cache.lastMiscKey:m_lastMiscKey;
	bool &haveMaterialKey=m_xrMode ? cache.haveMaterialKey:m_haveMaterialKey;
	auto &lastMaterialKey=m_xrMode ? cache.lastMaterialKey:m_lastMaterialKey;
	bool &haveLightingKey=m_xrMode ? cache.haveLightingKey:m_haveLightingKey;
	auto &lastLightingKey=m_xrMode ? cache.lastLightingKey:m_lastLightingKey;

	// D3D row-major memory uploaded untransposed IS the transpose GL wants
	// for column-vector math (see plan notes). uWorld is deliberately never
	// cached -- it changes on nearly every draw in real battlefield
	// rendering (each object has its own transform) -- see ViewProjKey's
	// declaration for why the rest of this function's blocks are cached.
	if (prog->uWorld >= 0)
		glUniformMatrix4fv(prog->uWorld, 1, GL_FALSE, (const float *)&dev->getTransform(D3DTS_WORLD));

	// GeneralsX @build Android port GLES experiment 08/30/2026 view/proj now
	// live in a UBO (kViewProjUBOBinding), not per-program uniform
	// locations -- unconditional (no prog->uView/uProj gate; those fields
	// are gone from ProgramInfo, see the struct's comment) since the
	// buffer's binding is fixed for the whole session and every vertex
	// shader variant declares the same ViewProjBlock regardless of FVF.
	// Cheap to run even for xyzrhw (2D) draws that don't reference the
	// block at all in their shader body -- worst case a redundant upload
	// when content alternates between 3D and 2D, never a correctness
	// issue. Real re-upload (glBufferSubData) only happens when the value
	// actually differs from what's already on the GPU.
	{
		ViewProjKey key{};
		memcpy(key.view, &dev->getTransform(D3DTS_VIEW), sizeof(key.view));
		memcpy(key.proj, &dev->getTransform(D3DTS_PROJECTION), sizeof(key.proj));
		// GeneralsX @spike XR port Phase 0.3 - camera proof: with the marker
		// present, shift the VIEW matrix laterally (along its own right
		// vector) on a slow sine before upload. The game's stored matrix is
		// untouched -- only rendering moves -- which models exactly the
		// Phase-3 stereo mechanism (same frame, re-rendered with an offset
		// view). A visibly swaying scene = external view control works.
		{
			static unsigned s_xrCamCalls = 0;
			static bool s_xrCamOn = false;
			if ((s_xrCamCalls++ % 600) == 0) {
				FILE *mk = fopen("/sdcard/Download/GeneralsZH/gx_xr_camoffset.txt", "rb");
				if (mk) fclose(mk);
				if ((mk != nullptr) != s_xrCamOn) {
					s_xrCamOn = (mk != nullptr);
					fprintf(stderr, "[d3d8gles] XR spike: camera wobble %s\n", s_xrCamOn ? "ON" : "OFF");
				}
			}
			if (s_xrCamOn) {
				const float t = SDL_GetTicks() / 1000.0f;
				const float k = 12.0f * sinf(t * 0.6f);
				float *v = key.view;
				v[12] += k * v[0];
				v[13] += k * v[1];
				v[14] += k * v[2];
			}
		}
		if (m_haveViewProjKey && key == m_lastViewProjKey) {
			m_perfUniformCacheHits++;
			m_perfUniformViewProjHits++;
		} else {
			m_perfUniformCacheMisses++;
			m_perfUniformViewProjMisses++;
			glBindBuffer(GL_UNIFORM_BUFFER, m_viewProjUBO);
			glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(key), &key);
			m_lastViewProjKey = key;
			m_haveViewProjKey = true;
		}
	}

	if (prog->uTexMat0 >= 0 || prog->uTexMat1 >= 0) {
		TexMatKey key{};
		memcpy(key.texMat0, &dev->getTransform(D3DTS_TEXTURE0), sizeof(key.texMat0));
		memcpy(key.texMat1, &dev->getTransform((D3DTRANSFORMSTATETYPE)(D3DTS_TEXTURE0 + 1)), sizeof(key.texMat1));
		if (haveTexMatKey && key == lastTexMatKey) {
			m_perfUniformCacheHits++;
			m_perfUniformTexMatHits++;
		} else {
			m_perfUniformCacheMisses++;
			m_perfUniformTexMatMisses++;
			if (prog->uTexMat0 >= 0) glUniformMatrix4fv(prog->uTexMat0, 1, GL_FALSE, key.texMat0);
			if (prog->uTexMat1 >= 0) glUniformMatrix4fv(prog->uTexMat1, 1, GL_FALSE, key.texMat1);
			lastTexMatKey = key;
			haveTexMatKey = true;
		}
	}

	if (prog->uViewportPos >= 0 || prog->uYFlip >= 0 || prog->uTFactor >= 0 ||
	    prog->uAlphaRef >= 0 || prog->uFogColor >= 0) {
		const D3DVIEWPORT8 &vp = dev->getViewport();
		MiscUniformKey key{};
		key.vpX = (float)vp.X; key.vpY = (float)vp.Y; key.vpW = (float)vp.Width; key.vpH = (float)vp.Height;
		key.yFlip = m_yFlip;
		argbToFloats(dev->getRenderState(D3DRS_TEXTUREFACTOR), key.tFactor);
		key.alphaRef = (float)(dev->getRenderState(D3DRS_ALPHAREF) & 0xFF) / 255.0f;
		argbToFloats(dev->getRenderState(D3DRS_FOGCOLOR), key.fogColor);
		key.fogStart = dwordToFloat(dev->getRenderState(D3DRS_FOGSTART));
		key.fogEnd = dwordToFloat(dev->getRenderState(D3DRS_FOGEND));

		if (haveMiscKey && key == lastMiscKey) {
			m_perfUniformCacheHits++;
			m_perfUniformMiscHits++;
		} else {
			m_perfUniformCacheMisses++;
			m_perfUniformMiscMisses++;
			if (prog->uViewportPos >= 0) glUniform4f(prog->uViewportPos, key.vpX, key.vpY, key.vpW, key.vpH);
			if (prog->uYFlip >= 0) glUniform1f(prog->uYFlip, key.yFlip);
			if (prog->uTFactor >= 0) glUniform4fv(prog->uTFactor, 1, key.tFactor);
			if (prog->uAlphaRef >= 0) glUniform1f(prog->uAlphaRef, key.alphaRef);
			if (prog->uFogColor >= 0) {
				glUniform4fv(prog->uFogColor, 1, key.fogColor);
				glUniform2f(prog->uFogParams, key.fogStart, key.fogEnd);
			}
			lastMiscKey = key;
			haveMiscKey = true;
		}
	}

	// NOTE: each uniform can be optimized out independently (a program whose
	// material sources are all vertex colors has NO uMat* uniforms but still
	// needs its lights). Never gate the light upload on a material location.
	if (prog->uMatDiffuse >= 0 || prog->uMatAmbient >= 0 || prog->uMatEmissive >= 0) {
		const D3DMATERIAL8 &m = dev->getMaterial();
		MaterialKey key{};
		memcpy(key.diffuse, &m.Diffuse, sizeof(key.diffuse));
		memcpy(key.ambient, &m.Ambient, sizeof(key.ambient));
		memcpy(key.emissive, &m.Emissive, sizeof(key.emissive));
		if (haveMaterialKey && key == lastMaterialKey) {
			m_perfUniformCacheHits++;
			m_perfUniformMaterialHits++;
		} else {
			m_perfUniformCacheMisses++;
			m_perfUniformMaterialMisses++;
			if (prog->uMatDiffuse >= 0) glUniform4fv(prog->uMatDiffuse, 1, key.diffuse);
			if (prog->uMatAmbient >= 0) glUniform4fv(prog->uMatAmbient, 1, key.ambient);
			if (prog->uMatEmissive >= 0) glUniform4fv(prog->uMatEmissive, 1, key.emissive);
			lastMaterialKey = key;
			haveMaterialKey = true;
		}
	}
	if (prog->uGlobalAmbient >= 0 || prog->uNumLights >= 0) {
		LightingKey key{};
		argbToFloats(dev->getRenderState(D3DRS_AMBIENT), key.globalAmbient);
		int n = 0;
		for (unsigned i = 0; i < WebGLDevice::kMaxLights && n < 4; i++) {
			if (!dev->isLightEnabled(i)) continue;
			const D3DLIGHT8 &L = dev->getLight(i);
			key.types[n] = (L.Type == D3DLIGHT_POINT) ? 1 : 0;
			key.dirs[n * 3 + 0] = L.Direction.x;
			key.dirs[n * 3 + 1] = L.Direction.y;
			key.dirs[n * 3 + 2] = L.Direction.z;
			key.poss[n * 3 + 0] = L.Position.x;
			key.poss[n * 3 + 1] = L.Position.y;
			key.poss[n * 3 + 2] = L.Position.z;
			memcpy(&key.diff[n * 4], &L.Diffuse, 16);
			memcpy(&key.amb[n * 4], &L.Ambient, 16);
			key.att[n * 4 + 0] = L.Range;
			key.att[n * 4 + 1] = L.Attenuation0 > 0 ? L.Attenuation0 : 1.0f;
			key.att[n * 4 + 2] = L.Attenuation1;
			key.att[n * 4 + 3] = L.Attenuation2;
			n++;
		}
		key.numLights = n;

		if (haveLightingKey && key == lastLightingKey) {
			m_perfUniformCacheHits++;
			m_perfUniformLightingHits++;
		} else {
			m_perfUniformCacheMisses++;
			m_perfUniformLightingMisses++;
			if (prog->uGlobalAmbient >= 0) glUniform4fv(prog->uGlobalAmbient, 1, key.globalAmbient);
			if (prog->uNumLights >= 0) {
				glUniform1i(prog->uNumLights, key.numLights);
				glUniform1iv(prog->uLightType, 4, key.types);
				glUniform3fv(prog->uLightDir, 4, key.dirs);
				glUniform3fv(prog->uLightPos, 4, key.poss);
				glUniform4fv(prog->uLightDiffuse, 4, key.diff);
				glUniform4fv(prog->uLightAmbient, 4, key.amb);
				glUniform4fv(prog->uLightAtten, 4, key.att);
			}
			lastLightingKey = key;
			haveLightingKey = true;
		}
	}
}

// ---------------------------------------------------------------------------
// Draw paths
// ---------------------------------------------------------------------------

static GLenum primModeGL(unsigned primType)
{
	switch (primType) {
	case D3DPT_POINTLIST: return GL_POINTS;
	case D3DPT_LINELIST: return GL_LINES;
	case D3DPT_LINESTRIP: return GL_LINE_STRIP;
	case D3DPT_TRIANGLESTRIP: return GL_TRIANGLE_STRIP;
	case D3DPT_TRIANGLEFAN: return GL_TRIANGLE_FAN;
	case D3DPT_TRIANGLELIST:
	default: return GL_TRIANGLES;
	}
}

static unsigned primVertexCount(unsigned primType, unsigned primCount)
{
	switch (primType) {
	case D3DPT_POINTLIST: return primCount;
	case D3DPT_LINELIST: return primCount * 2;
	case D3DPT_LINESTRIP: return primCount + 1;
	case D3DPT_TRIANGLESTRIP:
	case D3DPT_TRIANGLEFAN: return primCount + 2;
	case D3DPT_TRIANGLELIST:
	default: return primCount * 3;
	}
}

// GeneralsX @build Android port GLES experiment - perf pass, split from a
// single setupAttribs() after a real device log showed the VAO cache
// growing by thousands of entries within seconds in ordinary gameplay: the
// engine draws a lot of its content (see DX8Wrapper's BUFFER_TYPE_DYNAMIC_DX8
// pool) through one shared vertex buffer with a *base-vertex offset* that
// advances practically every draw, so folding `base` into the VAO's cached
// attribute pointers (as the first version of this cache did) meant that
// class of content got a brand-new VAO -- and GL object -- almost every
// single call, defeating the cache and leaking VAOs for the session's
// lifetime. Splitting the two concerns fixes both: attribute
// enable/disable state is genuinely per-(FVF layout) and only needs
// setting once when a VAO is first created (a fresh VAO starts with every
// attribute disabled, so enableAttribs() only ever turns bits on).
// Attribute *pointers* additionally encode `base` and DO need reissuing
// whenever it changes -- but that's a handful of glVertexAttribPointer
// calls against an already-bound, otherwise-unchanged VAO, not a new GL
// object plus the full disable/enable/pointer dance every time.
static void enableAttribs(const FVFLayout &l)
{
	for (int i = 0; i < 8; i++) glDisableVertexAttribArray(i);
	glEnableVertexAttribArray(0);
	if (l.hasNormal) glEnableVertexAttribArray(1);
	if (l.hasDiffuse) glEnableVertexAttribArray(2);
	if (l.hasSpecular) glEnableVertexAttribArray(3);
	const int texIn = l.texCount > 2 ? 2 : l.texCount;
	for (int i = 0; i < texIn; i++) glEnableVertexAttribArray(4 + i);
}

// Sets attribute pointers (format + base-relative offset) for the currently
// bound ARRAY_BUFFER and VAO. Safe to call on an already-enabled attribute
// -- glVertexAttribPointer only ever touches format/pointer state, never
// enabled/disabled state, regardless of how many times it's reissued.
static void setAttribPointers(const FVFLayout &l, unsigned stride, intptr_t base)
{
	glVertexAttribPointer(0, l.xyzrhw ? 4 : 3, GL_FLOAT, GL_FALSE, stride, (const void *)(base + l.posOffset));
	if (l.hasNormal)
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (const void *)(base + l.normalOffset));
	if (l.hasDiffuse)
		glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, stride, (const void *)(base + l.diffuseOffset));
	if (l.hasSpecular)
		glVertexAttribPointer(3, 4, GL_UNSIGNED_BYTE, GL_TRUE, stride, (const void *)(base + l.specularOffset));
	const int texIn = l.texCount > 2 ? 2 : l.texCount;
	for (int i = 0; i < texIn; i++)
		glVertexAttribPointer(4 + i, l.texSize[i], GL_FLOAT, GL_FALSE, stride, (const void *)(base + l.texOffset[i]));
}

// Byte-wise FNV-1a over VAOKey's raw bytes. Safe: VAOKey is four 4-byte POD
// members (GLuint/unsigned), so there's no padding to worry about hashing
// garbage from. Same collision-tolerant precedent as computeProgramKey()
// -- see kMaxVAOs's declaration in gles_pipeline.h.
uint64_t WebGLPipeline::hashVAOKey(const VAOKey &k)
{
	uint64_t h = 0xcbf29ce484222325ull;
	const unsigned char *p = reinterpret_cast<const unsigned char *>(&k);
	for (size_t i = 0; i < sizeof(k); i++) {
		h ^= p[i];
		h *= 0x100000001b3ull;
	}
	return h;
}

void WebGLPipeline::evictVAOsForBuffer(GLuint name)
{
	bool evictedAny = false;
	for (auto it = m_vaoCache.begin(); it != m_vaoCache.end(); ) {
		if (it->second.key.vbo == name || it->second.key.ibo == name) {
			glDeleteVertexArrays(1, &it->second.vao);
			it = m_vaoCache.erase(it);
			evictedAny = true;
		} else {
			++it;
		}
	}
	// The "same key as last draw, skip everything" fast path in
	// bindVertexLayout() below trusts that the previously bound VAO is still
	// valid; if it just got deleted here, that trust would be wrong.
	if (evictedAny) m_haveLastVAOKey = false;
}

void WebGLPipeline::bindVertexLayout(const FVFLayout &l, GLuint vbo, GLuint ibo,
                                     unsigned fvf, unsigned stride, int base)
{
	VAOKey key{};
	key.vbo = vbo;
	key.ibo = ibo;
	key.fvf = fvf;
	key.stride = stride;
	// `base` is deliberately NOT part of the key -- see enableAttribs()'s
	// comment. It's still tracked below (m_lastVAOBase / VAOCacheEntry::
	// lastBase) since a VAO's *pointers* do encode it and must be kept
	// current even when the VAO object itself is being reused.

	if (m_haveLastVAOKey && key == m_lastVAOKey && base == m_lastVAOBase) {
		m_perfVAOCacheHits++;
		return; // the correct VAO -- attribs, pointers, and element-buffer binding alike -- is already bound
	}
	m_lastVAOKey = key;
	m_lastVAOBase = base;
	m_haveLastVAOKey = true;

	const uint64_t hash = hashVAOKey(key);
	auto it = m_vaoCache.find(hash);
	if (it != m_vaoCache.end()) {
		// GeneralsX @build Android port GLES experiment - this counts as a
		// hit, not a miss: the VAO *object* is reused either way, which is
		// the expensive part this cache exists to avoid (glGenVertexArrays,
		// plus the old per-draw glDisableVertexAttribArray x8 dance this
		// object now only ever pays once). A base change below is real but
		// comparatively cheap -- a handful of glVertexAttribPointer calls,
		// tracked separately (m_perfVAOPointerRefresh) so the perf log can
		// show how much of that residual cost is still coming from content
		// that draws through a shared/dynamic buffer pool at a
		// constantly-advancing offset, distinct from a true cache miss.
		m_perfVAOCacheHits++;
		glBindVertexArray(it->second.vao);
		if (it->second.lastBase != base) {
			// GL_ARRAY_BUFFER must be the right buffer for
			// glVertexAttribPointer to capture it correctly;
			// GL_ELEMENT_ARRAY_BUFFER is untouched since it doesn't encode
			// `base` at all.
			m_perfVAOPointerRefresh++;
			bindArrayBuffer(vbo);
			setAttribPointers(l, stride, base);
			it->second.lastBase = base;
		}
		return;
	}

	// True miss: this (vbo, ibo, fvf, stride) combination has never been
	// seen before. Build (or, past the sanity-backstop cap, temporarily
	// fall back to an uncached bind against) a VAO for it.
	m_perfVAOCacheMisses++;
	if (m_vaoCache.size() >= kMaxVAOs) {
		WARN_ONCE(s_vaoOverflow, "VAO cache at its %zu-entry sanity cap, no longer "
		          "caching new combinations this session", kMaxVAOs);
		glBindVertexArray(0);
		bindArrayBuffer(vbo);
		enableAttribs(l);
		setAttribPointers(l, stride, base);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
		m_haveLastVAOKey = false; // unknown/uncached state -- always re-decide next draw
		return;
	}

	GLuint vao = 0;
	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);
	bindArrayBuffer(vbo); // GL_ARRAY_BUFFER is not VAO state; safe to route through the skip cache
	enableAttribs(l);
	setAttribPointers(l, stride, base);
	// GL_ELEMENT_ARRAY_BUFFER, unlike GL_ARRAY_BUFFER, IS part of the
	// currently-bound VAO's state -- bind unconditionally (not through a
	// skip cache; see m_lastArrayBuffer's comment for why one would be
	// unsafe here) so it's captured into the VAO just created above. 0 is
	// the correct, valid binding for the non-indexed draw() path.
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);

	m_vaoCache.emplace(hash, VAOCacheEntry{key, vao, base});
}

// GeneralsX @perf Android port 09/05/2026 Draw-call breakdown by subsystem.
// Real-device logs show ~1200-2500 draws/frame in gameplay at 13-15 fps, i.e.
// roughly 27k draws/sec -- about what a mobile GLES driver sustains when each
// draw carries state changes, so we are draw-call bound. But "reduce draw
// calls" is useless without knowing WHERE they come from: the instancing
// attempt earlier only ever collapsed ~10% of them, because it targeted the
// rigid-model path while most draws apparently come from somewhere else.
//
// Engine code tags the few passes it can identify cheaply (see
// d3d8gles_SetDrawCategory's callers); everything else lands in OTHER
// (terrain, shadows, skinned meshes, ...). Callers save/restore the previous
// value, so nesting works.
enum GeneralsXDrawCategory {
	GX_DRAWCAT_OTHER = 0,
	GX_DRAWCAT_MODELS,   // DX8TextureCategoryClass::Render -- rigid HLod meshes
	GX_DRAWCAT_SORTED,   // SortingRendererClass::Flush -- particles, decals
	GX_DRAWCAT_2D,       // Render2DClass::Render -- all UI and video
	GX_DRAWCAT_TERRAIN,  // HeightMapRenderObjClass::Render
	GX_DRAWCAT_SHADOWS,  // W3DProjectedShadowManager::renderShadows
	GX_DRAWCAT_SKIN,     // DX8SkinFVFCategoryContainer::Render
	GX_DRAWCAT_COUNT
};
static int s_gxDrawCategory = GX_DRAWCAT_OTHER;
static unsigned s_gxDrawsByCategory[GX_DRAWCAT_COUNT] = {0, 0, 0, 0, 0, 0, 0};

// GeneralsX @perf Android port 09/05/2026 UI cost buckets, filled by engine
// code that times itself (see d3d8gles_AddUiTiming's callers). Reported next to
// the draw split so one log line shows both where the draws and where the CPU
// time go.
static double s_gxUiTimeUs[3] = {0.0, 0.0, 0.0};


extern "C" void d3d8gles_AddUiTiming(int bucket, double microseconds)
{
	if (bucket >= 0 && bucket < 3) s_gxUiTimeUs[bucket] += microseconds;
}

extern "C" int d3d8gles_SetDrawCategory(int category)
{
	const int prev = s_gxDrawCategory;
	s_gxDrawCategory = (category >= 0 && category < GX_DRAWCAT_COUNT) ? category : GX_DRAWCAT_OTHER;
	return prev;
}

void WebGLPipeline::drawCommon(WebGLDevice *dev, unsigned primType, unsigned primCount,
                               GLuint vbo, unsigned stride, unsigned fvf,
                               GLuint ibo, unsigned indexFormat,
                               unsigned startIndex, int baseVertexBytes, unsigned /*vertexCount*/)
{
	FVFLayout l;
	if (!parseFVF(fvf, &l)) {
		WARN_ONCE(s_fvf, "unsupported FVF 0x%x", fvf);
		return;
	}
	if (stride == 0) stride = l.stride;

	ProgramInfo *prog = getProgram(dev, fvf);
	if (!prog || !prog->prog) return;

	applyFixedState(dev);
	applyUniforms(dev, prog, fvf);
	bindTextures(dev, prog);

	bindVertexLayout(l, vbo, ibo, fvf, stride, baseVertexBytes);

	const GLenum mode = primModeGL(primType);
	const unsigned count = primVertexCount(primType, primCount);

	auto submit=[&]() {
	if (indexFormat != 0) {
		const GLenum itype = (indexFormat == D3DFMT_INDEX32) ? GL_UNSIGNED_INT : GL_UNSIGNED_SHORT;
		const unsigned isize = (indexFormat == D3DFMT_INDEX32) ? 4 : 2;
		glDrawElements(mode, count, itype, (const void *)(intptr_t)(startIndex * isize));
	} else {
		glDrawArrays(mode, startIndex, count);
	}
	};
	// GeneralsX @feature Codex 13/09/2026 P7.0 reuses each uploaded draw now,
	// before dynamic buffers/textures can change. Engine callbacks run once.
	// P8 includes depth-tested world effects and the complete stencil-shadow
	// sequence. Ordinary screen-space UI remains on the detached panel.
	const bool shadow=s_gxDrawCategory==GX_DRAWCAT_SHADOWS;
	const bool effect=s_gxDrawCategory==GX_DRAWCAT_SORTED || s_gxDrawCategory==GX_DRAWCAT_OTHER;
	const bool category=shadow || effect || s_gxDrawCategory==GX_DRAWCAT_MODELS || s_gxDrawCategory==GX_DRAWCAT_TERRAIN || s_gxDrawCategory==GX_DRAWCAT_SKIN;
	const DWORD xrColorWrite=dev->getRenderState(D3DRS_COLORWRITEENABLE);
	const bool xrColorPass=gxXrWorldColorPass(xrColorWrite,dev->getRenderState(D3DRS_STENCILENABLE)!=0,
		dev->getRenderState(D3DRS_STENCILFUNC)==D3DCMP_ALWAYS);
	const bool perspective=fabsf(((const float *)&dev->getTransform(D3DTS_PROJECTION))[15])<.001f;
	const bool worldDraw=!l.xyzrhw && perspective && dev->getRenderState(D3DRS_ZENABLE);
	const bool shadowFill=shadow && l.xyzrhw && dev->getRenderState(D3DRS_STENCILENABLE);
	const bool stereoDraw=m_xrStereoActive && m_curFBO==0 && category && (worldDraw || shadowFill) && (xrColorPass || shadow);
	// GeneralsX @performance Codex 14/09/2026 Omit only an immediate, eligible
	// world draw with current targets, certified coverage and existing UI MRT.
	// First allocation/probe, ordinary UI, RTT and non-XR retain their full draw.
	const bool omit=m_xrElision.canOmit(stereoDraw && !m_xrUI && !m_xrCaptureEnabled,m_xrStereoCoverage,
		m_xrWorldFBO!=0 && m_xrUITex!=0,m_xrStereoMultiview && !m_xrMultiviewFailed);
	if(omit)m_xrElision.omit();else submit();
	if(stereoDraw) {
		// GeneralsX @performance Codex 14/09/2026 Submit both array layers now.
		// A failed variant falls back per draw to ordinary layer FBOs, never
		// re-traverses the scene. Abandon multiview allocation next frame.
		ProgramInfo *ordinary=prog;
		bool multiview=m_xrStereoMultiview && !m_xrMultiviewFailed;
		if(multiview) {
			auto *stereo=getProgram(dev,fvf,true);
			if(stereo && stereo->prog) {prog=stereo;applyUniforms(dev,prog,fvf);bindTextures(dev,prog);}
			else {multiview=false;m_xrMultiviewFailed=true;fprintf(stderr,"[d3d8gles] P15 multiview shader failed; reference fallback\n");}
		}
		// P8 stencil volume geometry must NOT be clipped to board height:
		// clipping only one face leaves an unbalanced stencil count. Its fill
		// preserves destination alpha, so no shadow can create room coverage.
		const bool colorless=!(xrColorWrite&7) || (dev->getRenderState(D3DRS_ALPHABLENDENABLE) &&
			dev->getRenderState(D3DRS_SRCBLEND)==D3DBLEND_ZERO && dev->getRenderState(D3DRS_DESTBLEND)==D3DBLEND_ONE);
		glUniform1i(prog->uXrActive,shadowFill ? 2:shadow && colorless ? 3:1);
		// Normal buildings/units write stencil tags with ALWAYS. Preserve their
		// color/depth draw without importing central-camera stencil into XR.
		if(!shadow) glDisable(GL_STENCIL_TEST);
		// GeneralsX @bugfix Codex 13/09/2026 Alpha is coverage in the eye
		// target, independent of D3D RGB-only masks/material alpha. Keep RGB
		// masks; reject alpha-only/depth-only passes above. Restore below.
		glUniform1i(prog->uXrOpaque,!shadow && !dev->getRenderState(D3DRS_ALPHABLENDENABLE));
		glColorMask((xrColorWrite&1)!=0,(xrColorWrite&2)!=0,(xrColorWrite&4)!=0,shadow ? GL_FALSE:GL_TRUE);
		// PointGroup particles bake positions into view space and submit with
		// identity view/world. Undo that using the actual head-centered camera.
		const float *view=(const float *)&dev->getTransform(D3DTS_VIEW);
		bool identity=true;for(int i=0;i<16;++i) identity=identity && fabsf(view[i]-(i%5==0 ? 1.0f:0.0f))<.00001f;
		glUniform1i(prog->uXrViewSpace,effect && identity);
		glUniformMatrix4fv(prog->uXrCamera,1,GL_FALSE,m_xrCamera);
		if(m_xrStereoDraws==0 && !m_xrStereoCoverage && m_xrStereoProbeWait==0)
			fprintf(stderr,"[d3d8gles] P7.1 first draw category=%d colorMask=%u blend=%u cull=%u depth=%u\n",
				s_gxDrawCategory,xrColorWrite,dev->getRenderState(D3DRS_ALPHABLENDENABLE),
				dev->getRenderState(D3DRS_CULLMODE),dev->getRenderState(D3DRS_ZFUNC));
		glUniformMatrix4fv(prog->uXrBoard,1,GL_FALSE,m_xrBoard);glUniform1f(prog->uXrAspect,m_xrStereoAspect);
		// RGB retains the engine blend equation; alpha records coverage for MR.
		if(dev->getRenderState(D3DRS_ALPHABLENDENABLE)) glBlendFuncSeparate(
			d3dBlendToGL(dev->getRenderState(D3DRS_SRCBLEND)),d3dBlendToGL(dev->getRenderState(D3DRS_DESTBLEND)),GL_ONE,GL_ONE_MINUS_SRC_ALPHA);
		// GeneralsX @performance Codex 14/09/2026 An atlas keeps both eyes
		// on one target, without deferring draws that use mutable buffers.
		glDepthRangef(0,1);
		if(m_xrStereoAtlas) {glBindFramebuffer(GL_FRAMEBUFFER,m_xrStereoFBO[0]);glEnable(GL_SCISSOR_TEST);}
		else glViewport(0,0,m_xrStereoW,m_xrStereoH);
		if(multiview) {
			glBindFramebuffer(GL_FRAMEBUFFER,m_xrMultiviewFBO);
			glUniformMatrix4fv(prog->uXrEyeClip,2,GL_FALSE,m_xrEyeClip[0]);submit();
		} else for(int eye=0;eye<2;++eye) {
			if(!m_xrStereoAtlas) glBindFramebuffer(GL_FRAMEBUFFER,m_xrStereoFBO[eye]);
			const int eyeX=m_xrStereoAtlas ? eye*m_xrStereoW:0;
			if(m_xrStereoAtlas) {glViewport(eyeX,0,m_xrStereoW,m_xrStereoH);glScissor(eyeX,0,m_xrStereoW,m_xrStereoH);}
			glUniformMatrix4fv(prog->uXrEyeClip,1,GL_FALSE,m_xrEyeClip[eye]);submit();
		}
		if(m_xrStereoAtlas) glDisable(GL_SCISSOR_TEST);
		glUniform1i(prog->uXrActive,0);
		if(prog!=ordinary) {applyUniforms(dev,ordinary,fvf);bindTextures(dev,ordinary);}
		glBindFramebuffer(GL_FRAMEBUFFER,m_offFBO);
		// Restore the exact pre-eye state without reissuing untouched depth,
		// cull, bias and stencil parameters. The full cache remains valid.
		m_xrRestoreCalls+=gxXrRestoreDrawState(m_lastFixedStateKey,m_curRTHeight,shadow,m_xrUI && m_curFBO==0,d3dBlendToGL);
		++m_xrRestores;++m_xrStereoDraws;
		if(shadow) ++m_xrShadowDraws;else if(effect) ++m_xrEffectDraws;
		else if(s_gxDrawCategory==GX_DRAWCAT_TERRAIN) ++m_xrTerrainDraws;else ++m_xrModelDraws;
	}
	// GeneralsX @build Android port 09/05/2026 Dump the fixed-function state
	// actually in effect for the first terrain draw and the first model draw of
	// each log window. Every remaining theory for the black geometry on a
	// Low-detail start predicts a specific difference in these values between a
	// Low run and a High run, and NO counter in the build reports any of them --
	// which is why two logs of the same scene look identical everywhere the
	// fault has to live. Read from the device-side state, one line per category
	// per window, so it costs nothing.
	m_perfDrawsThisFrame++;
	s_gxDrawsByCategory[s_gxDrawCategory]++;
}

// Buffer objects (device-side shadow -> GL) helpers.
void WebGLPipeline::invalidateBufferBinding(GLuint name)
{
	if (name == 0) return; // 0 already means "no buffer" to the cache, never a real object
	if (m_lastArrayBuffer == name) m_lastArrayBuffer = ~0u;
	evictVAOsForBuffer(name);
}

void WebGLPipeline::bindArrayBuffer(GLuint name)
{
	if (m_lastArrayBuffer == name) return;
	glBindBuffer(GL_ARRAY_BUFFER, name);
	m_lastArrayBuffer = name;
}

// GeneralsX @build Android port GLES experiment - perf pass. GL_COPY_WRITE_BUFFER
// (GLES3 core) is a generic bind point no VAO or vertex-attrib state is ever
// defined in terms of, unlike GL_ARRAY_BUFFER/GL_ELEMENT_ARRAY_BUFFER --
// using it here for the actual data upload means a dirty VB/IB's glBufferData
// can never disturb whatever VAO bindVertexLayout() left bound from the
// previous draw (see that function's comment for why GL_ELEMENT_ARRAY_BUFFER
// specifically would be unsafe to touch mid-VAO otherwise).
// GeneralsX @perf Android port 09/05/2026 This used to respecify the ENTIRE
// buffer with glBufferData on every update. For the engine's shared dynamic VB
// (DEFAULT_VB_SIZE = 5000 vertices, ~220 KB at dynamic_fvf_type's 44
// bytes/vertex) that meant a full 220 KB re-upload for callers that wrote only
// a handful of quads into it -- Render2DClass::Render() does exactly that, and
// real-device timings measured it at 16-59 ms/frame across just 24-60 UI draws
// (~0.5-1 ms per draw, about half the frame). SortingRendererClass::Flush
// streams particles through the same buffer.
//
// Now: allocate storage once, then push only the byte range Lock/Unlock
// actually recorded (GLBufferState::markRange). Deliberately NOT orphaning
// (glBufferData with nullptr) before each glBufferSubData: this is a
// ring-style dynamic buffer where earlier sub-ranges belong to draws already
// submitted but not necessarily consumed yet, and orphaning would discard
// them. That matches D3D8's own NOOVERWRITE semantics for this usage.
void WebGLPipeline::ensureVBUploaded(WebGLVertexBuffer *vb)
{
	if (vb->m_gl.name == 0) {
		glGenBuffers(1, &vb->m_gl.name);
		vb->m_gl.allocated = false;
	}
	if (vb->m_gl.dirty) {
		glBindBuffer(GL_COPY_WRITE_BUFFER, vb->m_gl.name);
		const bool haveRange = vb->m_gl.dirtyBegin < vb->m_gl.dirtyEnd;
		// GeneralsX @bugfix Android port 09/05/2026 D3DLOCK_DISCARD does a FULL
		// upload rather than orphaning with glBufferData(..., nullptr) and then
		// pushing only the fresh range. Orphaning leaves every byte outside
		// that range undefined, and the engine does not treat the ring's older
		// contents as dead: SortingRendererClass draws sorted translucent
		// geometry that can still reference vertex ranges written earlier in
		// the frame. Those ranges survive in m_bits but not in the newly
		// orphaned GL storage, so they rasterized as garbage -- confirmed on a
		// real device as stray grids and long diagonal lines across the scene.
		// glBufferData WITH data respecifies the storage too (so it does not
		// wait on the GPU either), it just also refills it. This only happens
		// when the ring wraps, so the ordinary NOOVERWRITE appends below stay
		// cheap.
		const bool fullUpload = !vb->m_gl.allocated || !haveRange || vb->m_gl.pendingDiscard;
		if (fullUpload) {
			// First use, or an update with no recorded range: full upload,
			// which also (re)allocates the GL storage.
			glBufferData(GL_COPY_WRITE_BUFFER, vb->m_bits.size(), vb->m_bits.data(), GL_DYNAMIC_DRAW);
			vb->m_gl.allocated = true;
		} else {
			// GeneralsX @perf Android port 09/05/2026 This is the
			// D3DLOCK_NOOVERWRITE case (the engine's ring buffer appending
			// past offset 0). glBufferSubData here makes the driver
			// synchronize against the GPU still reading earlier ranges of the
			// same buffer, and real-device timings showed exactly that: a flat
			// ~1 ms per Render2DClass::Render() call, tracking UI draw count
			// 1:1 (50.6 draws -> 49.3 ms, 65.2 -> 62.5) and independent of how
			// much data was written -- the signature of waiting, not working.
			// GL_MAP_UNSYNCHRONIZED_BIT is the exact translation of what
			// NOOVERWRITE promises: the caller guarantees it is not touching
			// bytes the GPU may still read, so no wait is needed.
			const GLintptr off = (GLintptr)vb->m_gl.dirtyBegin;
			const GLsizeiptr len = (GLsizeiptr)(vb->m_gl.dirtyEnd - vb->m_gl.dirtyBegin);
			void *mapped = glMapBufferRange(GL_COPY_WRITE_BUFFER, off, len,
				GL_MAP_WRITE_BIT | GL_MAP_UNSYNCHRONIZED_BIT);
			if (mapped) {
				memcpy(mapped, vb->m_bits.data() + vb->m_gl.dirtyBegin, (size_t)len);
				glUnmapBuffer(GL_COPY_WRITE_BUFFER);
			} else {
				// Mapping can legitimately fail (driver refusal, lost
				// context); the synchronizing path is slower but correct.
				glBufferSubData(GL_COPY_WRITE_BUFFER, off, len,
					vb->m_bits.data() + vb->m_gl.dirtyBegin);
			}
		}
		vb->m_gl.dirty = false;
		vb->m_gl.pendingDiscard = false;
		vb->m_gl.clearRange();
	}
}

// GeneralsX @perf Android port 09/05/2026 Same range-only upload as
// ensureVBUploaded above -- see that comment for the reasoning and the
// measurements.
void WebGLPipeline::ensureIBUploaded(WebGLIndexBuffer *ib)
{
	if (ib->m_gl.name == 0) {
		glGenBuffers(1, &ib->m_gl.name);
		ib->m_gl.allocated = false;
	}
	if (ib->m_gl.dirty) {
		glBindBuffer(GL_COPY_WRITE_BUFFER, ib->m_gl.name);
		const bool haveRange = ib->m_gl.dirtyBegin < ib->m_gl.dirtyEnd;
		// GeneralsX @bugfix Android port 09/05/2026 D3DLOCK_DISCARD does a FULL
		// upload rather than orphaning with glBufferData(..., nullptr) and then
		// pushing only the fresh range. Orphaning leaves every byte outside
		// that range undefined, and the engine does not treat the ring's older
		// contents as dead: SortingRendererClass draws sorted translucent
		// geometry that can still reference vertex ranges written earlier in
		// the frame. Those ranges survive in m_bits but not in the newly
		// orphaned GL storage, so they rasterized as garbage -- confirmed on a
		// real device as stray grids and long diagonal lines across the scene.
		// glBufferData WITH data respecifies the storage too (so it does not
		// wait on the GPU either), it just also refills it. This only happens
		// when the ring wraps, so the ordinary NOOVERWRITE appends below stay
		// cheap.
		const bool fullUpload = !ib->m_gl.allocated || !haveRange || ib->m_gl.pendingDiscard;
		if (fullUpload) {
			glBufferData(GL_COPY_WRITE_BUFFER, ib->m_bits.size(), ib->m_bits.data(), GL_DYNAMIC_DRAW);
			ib->m_gl.allocated = true;
		} else {
			// GeneralsX @perf Android port 09/05/2026 This is the
			// D3DLOCK_NOOVERWRITE case (the engine's ring buffer appending
			// past offset 0). glBufferSubData here makes the driver
			// synchronize against the GPU still reading earlier ranges of the
			// same buffer, and real-device timings showed exactly that: a flat
			// ~1 ms per Render2DClass::Render() call, tracking UI draw count
			// 1:1 (50.6 draws -> 49.3 ms, 65.2 -> 62.5) and independent of how
			// much data was written -- the signature of waiting, not working.
			// GL_MAP_UNSYNCHRONIZED_BIT is the exact translation of what
			// NOOVERWRITE promises: the caller guarantees it is not touching
			// bytes the GPU may still read, so no wait is needed.
			const GLintptr off = (GLintptr)ib->m_gl.dirtyBegin;
			const GLsizeiptr len = (GLsizeiptr)(ib->m_gl.dirtyEnd - ib->m_gl.dirtyBegin);
			void *mapped = glMapBufferRange(GL_COPY_WRITE_BUFFER, off, len,
				GL_MAP_WRITE_BIT | GL_MAP_UNSYNCHRONIZED_BIT);
			if (mapped) {
				memcpy(mapped, ib->m_bits.data() + ib->m_gl.dirtyBegin, (size_t)len);
				glUnmapBuffer(GL_COPY_WRITE_BUFFER);
			} else {
				// Mapping can legitimately fail (driver refusal, lost
				// context); the synchronizing path is slower but correct.
				glBufferSubData(GL_COPY_WRITE_BUFFER, off, len,
					ib->m_bits.data() + ib->m_gl.dirtyBegin);
			}
		}
		ib->m_gl.dirty = false;
		ib->m_gl.pendingDiscard = false;
		ib->m_gl.clearRange();
	}
}

// GeneralsX @build Android port GLES experiment - perf pass. The *UP draw
// paths (drawUP/drawIndexedUP below) reuse one fixed streaming buffer
// (m_upVBO/m_upIBO) across every call, uploading fresh data every time --
// unlike the dirty-gated VB/IB path above, there is no "skip the upload"
// option here, the data really is new every call. A plain glBufferData with
// new contents into the SAME buffer object risks the driver having to stall
// the CPU until the GPU finishes consuming whatever THIS buffer held for the
// previous draw (which may still be in flight) before it can safely
// overwrite it. Explicitly orphaning first -- glBufferData with the same
// target/size and a null data pointer, requesting a fresh anonymous
// allocation with no dependency on the old one -- is the standard,
// driver-portable way to ask for a new backing allocation instead of
// waiting; mobile GL drivers are the ones most likely to need this spelled
// out rather than inferring it from the "same size, new data" pattern alone.
static void orphanAndUpload(GLenum target, GLuint buffer, size_t size, const void *data, GLenum usage)
{
	glBindBuffer(target, buffer);
	glBufferData(target, size, nullptr, usage);
	glBufferData(target, size, data, usage);
}

void WebGLPipeline::drawIndexed(WebGLDevice *dev, unsigned primType, unsigned /*minIndex*/,
                                unsigned numVertices, unsigned startIndex, unsigned primCount)
{
	if (!m_ctxReady) return;
	WebGLVertexBuffer *vb = dev->getStream0();
	WebGLIndexBuffer *ib = dev->getIndices();
	if (!vb || !ib) return;

	ensureVBUploaded(vb);
	ensureIBUploaded(ib);

	const unsigned fvf = dev->getFVF() ? dev->getFVF() : vb->m_fvf;
	const unsigned stride = dev->getStream0Stride();
	const int baseBytes = (int)(dev->getBaseVertexIndex() * stride);
	drawCommon(dev, primType, primCount, vb->m_gl.name, stride, fvf,
	           ib->m_gl.name, ib->m_format, startIndex, baseBytes, numVertices);
}

void WebGLPipeline::draw(WebGLDevice *dev, unsigned primType, unsigned startVertex, unsigned primCount)
{
	if (!m_ctxReady) return;
	WebGLVertexBuffer *vb = dev->getStream0();
	if (!vb) return;

	ensureVBUploaded(vb);

	const unsigned fvf = dev->getFVF() ? dev->getFVF() : vb->m_fvf;
	const unsigned stride = dev->getStream0Stride();
	drawCommon(dev, primType, primCount, vb->m_gl.name, stride, fvf,
	           0, 0, startVertex, 0, 0);
}

void WebGLPipeline::drawUP(WebGLDevice *dev, unsigned primType, unsigned primCount,
                           const void *vertexData, unsigned stride)
{
	if (!m_ctxReady || !vertexData) return;
	const unsigned fvf = dev->getFVF();
	FVFLayout l;
	if (!parseFVF(fvf, &l)) return;
	if (stride == 0) stride = l.stride;

	const unsigned vcount = primVertexCount(primType, primCount);
	orphanAndUpload(GL_COPY_WRITE_BUFFER, m_upVBO, (size_t)vcount * stride, vertexData, GL_STREAM_DRAW);

	drawCommon(dev, primType, primCount, m_upVBO, stride, fvf, 0, 0, 0, 0, vcount);
}

void WebGLPipeline::drawIndexedUP(WebGLDevice *dev, unsigned primType, unsigned minVertexIdx,
                                  unsigned numVertices, unsigned primCount,
                                  const void *indexData, unsigned indexFormat,
                                  const void *vertexData, unsigned stride)
{
	if (!m_ctxReady || !vertexData || !indexData) return;
	const unsigned fvf = dev->getFVF();
	FVFLayout l;
	if (!parseFVF(fvf, &l)) return;
	if (stride == 0) stride = l.stride;

	orphanAndUpload(GL_COPY_WRITE_BUFFER, m_upVBO, (size_t)(minVertexIdx + numVertices) * stride,
	                vertexData, GL_STREAM_DRAW);

	const unsigned isize = (indexFormat == D3DFMT_INDEX32) ? 4 : 2;
	const unsigned icount = primVertexCount(primType, primCount);
	orphanAndUpload(GL_COPY_WRITE_BUFFER, m_upIBO, (size_t)icount * isize, indexData, GL_STREAM_DRAW);

	drawCommon(dev, primType, primCount, m_upVBO, stride, fvf, m_upIBO, indexFormat, 0, 0, numVertices);
}

// ---------------------------------------------------------------------------
// Clear / present
// ---------------------------------------------------------------------------

void WebGLPipeline::clear(WebGLDevice *dev, unsigned flags, uint32_t argb, float z, unsigned stencil)
{
	if (!m_ctxReady) return;

	// GeneralsX @feature XR port Phase 1.0 - frames that never call
	// SetRenderTarget (the menu draws to the implicit default binding) still
	// need the owned FBO bound in XR mode. Only when the engine believes it
	// is on the backbuffer; explicit render targets pass through.
	if (m_xrMode && m_curFBO == 0 && ensureOffscreenTarget()) {
		m_curRTWidth = m_fbWidth;
		m_curRTHeight = m_fbHeight;
	}

	// D3D clears the viewport region only.
	const D3DVIEWPORT8 &vp = dev->getViewport();
	const bool full = (vp.X == 0 && vp.Y == 0 &&
	                   (int)vp.Width == m_curRTWidth && (int)vp.Height == m_curRTHeight);
	if (!full) {
		glEnable(GL_SCISSOR_TEST);
		// Same D3D-top-to-GL-bottom Y conversion as applyFixedState's
		// glViewport call -- glScissor's y is bottom-origin in GL too.
		glScissor((GLint)vp.X, (GLint)(m_curRTHeight - (int)vp.Y - (int)vp.Height),
		          (GLsizei)vp.Width, (GLsizei)vp.Height);
	}

	GLbitfield mask = 0;
	if (flags & D3DCLEAR_TARGET) {
		float c[4];
		argbToFloats(argb, c);
		// GeneralsX @bugfix Android port 09/04/2026 D3D8's Clear() alpha
		// component is meaningless on a real Windows backbuffer (no OS-level
		// compositing depends on it there), so callers have always felt free
		// to pass whatever's convenient -- e.g. W3DDisplay.cpp's main
		// per-frame Begin_Render() clear reuses TheWaterTransparency->
		// m_minWaterOpacity as this value, which has nothing to do with
		// wanting the actual screen transparent. That's harmless as long as
		// the default framebuffer has no alpha channel to write to, but
		// becomes a real hazard on Android if it ever does (SurfaceFlinger
		// can and does composite a GL-backed Surface's alpha against
		// whatever is behind it). Force full opacity on the DEFAULT
		// framebuffer specifically (m_curFBO==0, i.e. the real on-screen
		// backbuffer) regardless of whatever alpha the caller asked for --
		// offscreen render targets (m_curFBO!=0, e.g. water reflections)
		// keep using the caller's real alpha, since those aren't presented
		// directly to the OS compositor.
		if (m_curFBO == 0) c[3] = 1.0f;
		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
		glClearColor(c[0], c[1], c[2], c[3]);
		mask |= GL_COLOR_BUFFER_BIT;
	}
	if (flags & D3DCLEAR_ZBUFFER) {
		glDepthMask(GL_TRUE);
		glClearDepthf(z);
		mask |= GL_DEPTH_BUFFER_BIT;
	}
	if (flags & D3DCLEAR_STENCIL) {
		glStencilMask(0xFFFFFFFF);
		glClearStencil((GLint)stencil);
		mask |= GL_STENCIL_BUFFER_BIT;
	}
	if (full) glDisable(GL_SCISSOR_TEST);
	if (mask) glClear(mask);
	// GeneralsX @performance Codex 14/09/2026 P17 never relabel an old partial
	// image after an empty Present. Native world/movie frames clear before drawing.
	if(m_xrMode && m_curFBO==0 && m_offFBO && full && (flags & D3DCLEAR_TARGET))
		m_xrElision.fullClear();

	if (!full) glDisable(GL_SCISSOR_TEST);

	// GeneralsX @build Android port GLES experiment - clear() just forced
	// glColorMask/glDepthMask/glStencilMask to their clear-time values
	// (all-write) regardless of what D3D render state actually wants (e.g.
	// COLORWRITEENABLE=0 for stencil shadow volumes). applyFixedState()'s
	// redundant-state cache doesn't know that happened, so without this it
	// could see an unchanged D3DRS_* key and skip re-applying those masks,
	// leaving GL state out of sync with what the next draw actually needs.
	m_haveFixedStateKey = false;
}

// GeneralsX @feature XR port Phase 1.0 - see the header comment. The XR
// quad uses texture unit 2 (the game only ever touches 0-1) and its own
// VAO/program/FBO, so no game object is disturbed -- but the *bindings*
// all changed, and every one of these caches would skip re-applying on a
// match. Rebinding the FBO from the m_curFBO fiction is required because
// game draws assume the target is already bound (only setRenderTarget/
// clear ever bind). m_lastProgram=0 resets the legacy global uniform keys
// through the program-change path; the XR epoch lazily invalidates each
// program's resident uniform keys at its next use.
void WebGLPipeline::invalidateCachedGLState()
{
	if (!m_ctxReady) return;
	m_lastProgram = 0;
	if(m_xrMode)++m_xrUniformEpoch; // External GL users may have changed uniforms.
	m_haveLastVAOKey = false;
	m_haveFixedStateKey = false;
	m_lastArrayBuffer = ~0u;
	m_lastBoundTex[0] = m_lastBoundTex[1] = ~0u;
	m_lastShroudBound = ~0u;
	if (m_xrMode && m_curFBO == 0) {
		if (m_offFBO != 0) {
			glBindFramebuffer(GL_FRAMEBUFFER, m_offFBO);
		}
	} else {
		glBindFramebuffer(GL_FRAMEBUFFER, m_curFBO);
	}
}

// GeneralsX @spike XR port Phase 0.2 - see the header comment.
void WebGLPipeline::setOffscreenCapture(bool enabled)
{
	if (!m_ctxReady) return;
	if (enabled == m_offscreenCapture) return;
	m_offscreenCapture = enabled;
	fprintf(stderr, "[d3d8gles] XR spike: offscreen capture %s\n", enabled ? "ON" : "OFF");
}

bool WebGLPipeline::ensureOffscreenTarget()
{
	if (m_offFBO != 0 && m_offW == m_fbWidth && m_offH == m_fbHeight) {
		glBindFramebuffer(GL_FRAMEBUFFER, m_offFBO);
		return true;
	}
	destroyXRLayers();
	if (m_offFBO) { glDeleteFramebuffers(1, &m_offFBO); m_offFBO = 0; }
	if (m_offColorTex) { glDeleteTextures(1, &m_offColorTex); m_offColorTex = 0; }
	if (m_offDepthRB) { glDeleteRenderbuffers(1, &m_offDepthRB); m_offDepthRB = 0; }
	m_offW = m_fbWidth;
	m_offH = m_fbHeight;
	glGenTextures(1, &m_offColorTex);
	glBindTexture(GL_TEXTURE_2D, m_offColorTex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, m_offW, m_offH, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	// Keep the sampler-bind cache honest (same reason as the render-target path).
	m_lastBoundTex[0] = m_lastBoundTex[1] = ~0u;
	glGenFramebuffers(1, &m_offFBO);
	glBindFramebuffer(GL_FRAMEBUFFER, m_offFBO);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_offColorTex, 0);
	glGenRenderbuffers(1, &m_offDepthRB);
	glBindRenderbuffer(GL_RENDERBUFFER, m_offDepthRB);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, m_offW, m_offH);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_offDepthRB);
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
		WARN_ONCE(s_offscreenIncomplete, "offscreen capture FBO incomplete (%dx%d), capture skipped",
			m_offW, m_offH);
		glDeleteFramebuffers(1, &m_offFBO); m_offFBO = 0;
		glDeleteTextures(1, &m_offColorTex); m_offColorTex = 0;
		glDeleteRenderbuffers(1, &m_offDepthRB); m_offDepthRB = 0;
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		return false;
	}
	return true;
}

// GeneralsX @feature Codex 13/09/2026 GPU-only layer capture. The UI is
// submitted ONCE into the ordinary composed image and attachment 1 together.
void WebGLPipeline::destroyXRStereo()
{
	if(m_xrDecorProgram) glDeleteProgram(m_xrDecorProgram);
	if(m_xrDecorVAO) glDeleteVertexArrays(1,&m_xrDecorVAO);
	if(m_xrDecorVBO) glDeleteBuffers(1,&m_xrDecorVBO);
	m_xrDecorProgram=m_xrDecorVAO=m_xrDecorVBO=0;
	if(m_xrMultiviewFBO)glDeleteFramebuffers(1,&m_xrMultiviewFBO);
	if(m_xrMultiviewDepth)glDeleteTextures(1,&m_xrMultiviewDepth);
	m_xrMultiviewFBO=m_xrMultiviewDepth=0;
	m_xrStereoMultiview=m_xrMultiviewRequested=false;
	glDeleteFramebuffers(2,m_xrStereoFBO);glDeleteTextures(2,m_xrStereoTex);glDeleteRenderbuffers(2,m_xrStereoDepth);
	for(int i=0;i<2;++i) m_xrStereoFBO[i]=m_xrStereoTex[i]=m_xrStereoDepth[i]=0;
	m_xrStereoW=m_xrStereoH=0;m_xrStereoActive=m_xrStereoReady=false;
	m_xrStereoCoverage=false;m_xrStereoProbeWait=0;
	m_xrStereoAtlas=m_xrAtlasRequested=false;
}
bool WebGLPipeline::beginXRStereo(int width,int height,const float *left,const float *right,const float *board,float aspect,const float *camera,bool atlas,bool multiview)
{
	m_xrStereoActive=m_xrStereoReady=false;m_xrStereoDraws=0;m_xrTerrainDraws=m_xrModelDraws=0;
	m_xrEffectDraws=m_xrShadowDraws=0;
	if(!m_xrMode || !m_ctxReady || !m_xrSplitRequested || m_curFBO!=0 ||
		width<64 || height<64 || width>2560 || height>2560 || !left || !right || !board || !camera ||
		!gxXrStereoAspectValid(aspect)) return false;
	for(int i=0;i<16;++i) if(!std::isfinite(left[i]) || !std::isfinite(right[i]) || !std::isfinite(board[i]) || !std::isfinite(camera[i])) return false;
	memcpy(m_xrCamera,camera,sizeof(m_xrCamera));
	if(width!=m_xrStereoW || height!=m_xrStereoH || atlas!=m_xrAtlasRequested || multiview!=m_xrMultiviewRequested || (m_xrStereoMultiview && m_xrMultiviewFailed)) {
		auto allocate=[&](bool combined) {
		destroyXRStereo();m_xrStereoW=width;m_xrStereoH=height;m_xrStereoAtlas=combined;
		const int targets=combined ? 1:2,targetWidth=combined ? width*2:width;
		GLint maxTexture=0,maxBuffer=0;glGetIntegerv(GL_MAX_TEXTURE_SIZE,&maxTexture);glGetIntegerv(GL_MAX_RENDERBUFFER_SIZE,&maxBuffer);
		if(targetWidth>maxTexture || targetWidth>maxBuffer || height>maxTexture || height>maxBuffer)return false;
		glGenFramebuffers(targets,m_xrStereoFBO);glGenTextures(targets,m_xrStereoTex);glGenRenderbuffers(targets,m_xrStereoDepth);
		bool ok=true;
		for(int eye=0;eye<targets;++eye) {
			glBindTexture(GL_TEXTURE_2D,m_xrStereoTex[eye]);
			glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,targetWidth,height,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
			glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
			glBindRenderbuffer(GL_RENDERBUFFER,m_xrStereoDepth[eye]);glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH24_STENCIL8,targetWidth,height);
			glBindFramebuffer(GL_FRAMEBUFFER,m_xrStereoFBO[eye]);
			glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,m_xrStereoTex[eye],0);
			glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_STENCIL_ATTACHMENT,GL_RENDERBUFFER,m_xrStereoDepth[eye]);
			ok=ok && glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE;
		}
		return ok;
		};
		bool ok=false;
		if(multiview && !m_xrMultiviewFailed && m_xrMultiview.available()) {
			destroyXRStereo();m_xrStereoW=width;m_xrStereoH=height;
			GLint maxTexture=0;glGetIntegerv(GL_MAX_TEXTURE_SIZE,&maxTexture);
			if(width<=maxTexture && height<=maxTexture) {
				glGenTextures(1,&m_xrStereoTex[0]);glGenTextures(1,&m_xrMultiviewDepth);
				for(int target=0;target<2;++target) {
					glBindTexture(GL_TEXTURE_2D_ARRAY,target ? m_xrMultiviewDepth:m_xrStereoTex[0]);
					m_xrMultiview.storage(GL_TEXTURE_2D_ARRAY,1,target ? GL_DEPTH24_STENCIL8:GL_RGBA8,width,height,2);
					glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
					glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
					glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
					glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
				}
				glGenFramebuffers(1,&m_xrMultiviewFBO);glBindFramebuffer(GL_FRAMEBUFFER,m_xrMultiviewFBO);
				m_xrMultiview.attach(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,m_xrStereoTex[0],0,0,2);
				m_xrMultiview.attach(GL_FRAMEBUFFER,GL_DEPTH_STENCIL_ATTACHMENT,m_xrMultiviewDepth,0,0,2);
				ok=glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE;
				// Single-layer views for coverage probes, decorations and safe per-draw fallback.
				glGenFramebuffers(2,m_xrStereoFBO);
				for(int eye=0;eye<2;++eye) {
					glBindFramebuffer(GL_FRAMEBUFFER,m_xrStereoFBO[eye]);
					m_xrMultiview.layer(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,m_xrStereoTex[0],0,eye);
					m_xrMultiview.layer(GL_FRAMEBUFFER,GL_DEPTH_STENCIL_ATTACHMENT,m_xrMultiviewDepth,0,eye);
					ok=ok && glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE;
				}
				glBindTexture(GL_TEXTURE_2D_ARRAY,0);
				ok=(glGetError()==GL_NO_ERROR) && ok;
				m_xrStereoMultiview=ok;
			}
		}
		if(!ok) {ok=allocate(atlas);if(!ok && atlas)ok=allocate(false);}
		if(!ok) {destroyXRStereo();invalidateCachedGLState();return false;}
		m_xrAtlasRequested=atlas;m_xrMultiviewRequested=multiview;
		fprintf(stderr,"[d3d8gles] P15 geometry targets %dx%d per eye, %s; unchanged total pixels\n",width,height,m_xrStereoMultiview ? "multiview":m_xrStereoAtlas ? "atlas":"separate");
	}
	glDisable(GL_SCISSOR_TEST);glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);glDepthMask(GL_TRUE);glClearDepthf(1);
	glClearColor(0,0,0,0);
	glStencilMask(0xff);glClearStencil(0);
	for(int eye=0;eye<(m_xrStereoAtlas ? 1:2);++eye) {glBindFramebuffer(GL_FRAMEBUFFER,m_xrStereoFBO[eye]);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);}
	memcpy(m_xrEyeClip[0],left,sizeof(m_xrEyeClip[0]));memcpy(m_xrEyeClip[1],right,sizeof(m_xrEyeClip[1]));
	memcpy(m_xrBoard,board,sizeof(m_xrBoard));m_xrStereoAspect=aspect;
	invalidateCachedGLState();m_xrStereoActive=true;m_xrStereoSeen=true;return true;
}
void WebGLPipeline::endXRStereo()
{
	if(!m_xrStereoActive) return;
	m_xrStereoActive=false;
	// GeneralsX @bugfix Codex 13/09/2026 Submitted draws can all be invisible.
	// Validate actual alpha in BOTH eyes once per activation/allocation before
	// hiding the planar board. Failed probes retry every 120 frames; successful
	// probes do not read back during steady rendering. This prototype gate may
	// hitch once and is not a substitute for continuous GPU health checks.
	if(m_xrStereoDraws && !m_xrStereoCoverage && m_xrStereoProbeWait==0) {
		GLint readFBO=0,packBuffer=0;glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,&readFBO);
		glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING,&packBuffer);glBindBuffer(GL_PIXEL_PACK_BUFFER,0);
		const GLenum packNames[]={GL_PACK_ALIGNMENT,GL_PACK_ROW_LENGTH,GL_PACK_SKIP_ROWS,GL_PACK_SKIP_PIXELS};
		GLint packValues[4];for(int i=0;i<4;++i) {glGetIntegerv(packNames[i],&packValues[i]);glPixelStorei(packNames[i],i==0 ? 4:0);}
		std::vector<unsigned char> pixels(size_t(m_xrStereoW)*m_xrStereoH*4);
		bool covered=true;
		for(int eye=0;eye<2;++eye) {
			glBindFramebuffer(GL_READ_FRAMEBUFFER,m_xrStereoFBO[m_xrStereoAtlas ? 0:eye]);glReadBuffer(GL_COLOR_ATTACHMENT0);
			std::fill(pixels.begin(),pixels.end(),0);
			glReadPixels(m_xrStereoAtlas ? eye*m_xrStereoW:0,0,m_xrStereoW,m_xrStereoH,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
			const GLenum error=glGetError();unsigned alpha=0,rgb=0;
			for(size_t i=0;i<pixels.size();i+=4) {alpha+=pixels[i+3]>0;rgb+=(pixels[i]|pixels[i+1]|pixels[i+2])!=0;}
			const bool valid=error==GL_NO_ERROR && alpha>=64;covered=covered && valid;
			fprintf(stderr,"[d3d8gles] P7.1 eye=%d alphaPixels=%u rgbPixels=%u error=0x%x %s\n",
				eye,alpha,rgb,error,valid ? "VISIBLE":"EMPTY: retain planar fallback");
		}
		glBindFramebuffer(GL_READ_FRAMEBUFFER,readFBO);glBindBuffer(GL_PIXEL_PACK_BUFFER,packBuffer);
		for(int i=0;i<4;++i) glPixelStorei(packNames[i],packValues[i]);
		m_xrStereoCoverage=covered;m_xrStereoProbeWait=120;
	} else if(m_xrStereoProbeWait) --m_xrStereoProbeWait;
	m_xrStereoReady=m_xrStereoDraws>0 && m_xrStereoCoverage;
	static unsigned frames=0;
	if((frames++%180)==0) fprintf(stderr,"[d3d8gles] P16 published %u source draws (%s): terrain=%u models=%u shadows=%u effects=%u; one traversal\n",
		m_xrStereoDraws,m_xrStereoMultiview && !m_xrMultiviewFailed ? "multiview":"two eye draws",m_xrTerrainDraws,m_xrModelDraws,m_xrShadowDraws,m_xrEffectDraws);
}
void WebGLPipeline::destroyXRLayers()
{
	destroyXRStereo();
	if(m_xrWorldFBO) glDeleteFramebuffers(1,&m_xrWorldFBO);
	if(m_xrWorldTex) glDeleteTextures(1,&m_xrWorldTex);
	if(m_xrUITex) glDeleteTextures(1,&m_xrUITex);
	m_xrWorldFBO=m_xrWorldTex=m_xrUITex=0; m_xrSplitReady=m_xrUI=false;
	m_xrWorldSnapshotValid=false;
}

// GeneralsX @feature Codex 13/09/2026 Frame/feedback share the real world's
// per-eye depth. Run AFTER the world visibility probe, so decoration cannot
// accidentally certify an otherwise empty capture as a playable world.
void WebGLPipeline::drawXRDecorations(const float *vertices,int count) {
	if(!m_xrStereoReady || !vertices || count<=0 || count>200000 || count%3) return;
	if(!m_xrDecorProgram) {
		GLuint vs=compileShader(GL_VERTEX_SHADER,kXrDecorationVertex);
		GLuint fs=compileShader(GL_FRAGMENT_SHADER,kXrDecorationFragment);
		if(!vs || !fs) {if(vs)glDeleteShader(vs);if(fs)glDeleteShader(fs);return;}
		m_xrDecorProgram=glCreateProgram();glAttachShader(m_xrDecorProgram,vs);glAttachShader(m_xrDecorProgram,fs);glLinkProgram(m_xrDecorProgram);
		glDeleteShader(vs);glDeleteShader(fs);GLint linked=0;glGetProgramiv(m_xrDecorProgram,GL_LINK_STATUS,&linked);
		if(!linked) {fprintf(stderr,"[d3d8gles] P9 decoration program link failed\n");glDeleteProgram(m_xrDecorProgram);m_xrDecorProgram=0;return;}
		glGenVertexArrays(1,&m_xrDecorVAO);glGenBuffers(1,&m_xrDecorVBO);
	}
	glUseProgram(m_xrDecorProgram);glBindVertexArray(m_xrDecorVAO);glBindBuffer(GL_ARRAY_BUFFER,m_xrDecorVBO);
	// Fully refill orphaned storage; both eyes consume it before the next update.
	glBufferData(GL_ARRAY_BUFFER,count*7*sizeof(float),vertices,GL_STREAM_DRAW);
	glEnableVertexAttribArray(0);glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,7*sizeof(float),nullptr);
	glEnableVertexAttribArray(1);glVertexAttribPointer(1,4,GL_FLOAT,GL_FALSE,7*sizeof(float),(void*)(3*sizeof(float)));
	glDisable(GL_BLEND);glDisable(GL_STENCIL_TEST);glDisable(GL_SCISSOR_TEST);glDisable(GL_CULL_FACE);glDisable(GL_POLYGON_OFFSET_FILL);
	glEnable(GL_DEPTH_TEST);glDepthFunc(GL_LEQUAL);glDepthMask(GL_TRUE);glDepthRangef(0,1);glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
	const GLint eyeUniform=glGetUniformLocation(m_xrDecorProgram,"eye");
	glUniformMatrix4fv(glGetUniformLocation(m_xrDecorProgram,"board"),1,GL_FALSE,m_xrBoard);
	glUniform1f(glGetUniformLocation(m_xrDecorProgram,"aspect"),m_xrStereoAspect);
	if(m_xrStereoAtlas) {glBindFramebuffer(GL_FRAMEBUFFER,m_xrStereoFBO[0]);glEnable(GL_SCISSOR_TEST);}
	for(int eye=0;eye<2;++eye) {
		if(!m_xrStereoAtlas)glBindFramebuffer(GL_FRAMEBUFFER,m_xrStereoFBO[eye]);
		const int eyeX=m_xrStereoAtlas ? eye*m_xrStereoW:0;glViewport(eyeX,0,m_xrStereoW,m_xrStereoH);
		if(m_xrStereoAtlas)glScissor(eyeX,0,m_xrStereoW,m_xrStereoH);
		glUniformMatrix4fv(eyeUniform,1,GL_FALSE,m_xrEyeClip[eye]);glDrawArrays(GL_TRIANGLES,0,count);
	}
	if(m_xrStereoAtlas)glDisable(GL_SCISSOR_TEST);
	invalidateCachedGLState();
}
void WebGLPipeline::beginXRFrame(bool split,bool elideOrdinaryWorld)
{
	if(!m_xrMode || !m_ctxReady) return;
	m_xrElision.configure(split && elideOrdinaryWorld);
	m_xrSplitRequested=split; m_xrUI=false;
	if(!m_xrStereoSeen) {m_xrStereoCoverage=false;m_xrStereoProbeWait=0;}
	m_xrStereoSeen=false;
	m_xrStereoActive=m_xrStereoReady=false;
	// Replay fast-forward may skip drawing. Keep the last published pair until
	// another Present replaces it; never alternate layouts on skipped renders.
	if(!split) m_xrSplitReady=false;
	if(m_offFBO) {
		glBindFramebuffer(GL_FRAMEBUFFER,m_offFBO);
		const GLenum one=GL_COLOR_ATTACHMENT0; glDrawBuffers(1,&one);
	}
	invalidateCachedGLState();
}
bool WebGLPipeline::beginXRUI(bool elideWorldCopy)
{
	if(!m_xrMode || !m_ctxReady || !m_xrSplitRequested || m_xrUI || m_curFBO!=0) return false;
	if(!ensureOffscreenTarget()) return false;
	if(!m_xrWorldFBO) {
		auto texture=[&](GLuint &name) {
			glGenTextures(1,&name); glBindTexture(GL_TEXTURE_2D,name);
			glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,m_offW,m_offH,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
			glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
		};
		texture(m_xrWorldTex); texture(m_xrUITex);
		glGenFramebuffers(1,&m_xrWorldFBO); glBindFramebuffer(GL_FRAMEBUFFER,m_xrWorldFBO);
		glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,m_xrWorldTex,0);
		bool ok=glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE;
		glBindFramebuffer(GL_FRAMEBUFFER,m_offFBO);
		glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT1,GL_TEXTURE_2D,m_xrUITex,0);
		ok=ok && glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE;
		if(!ok) {
			glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT1,GL_TEXTURE_2D,0,0);
			destroyXRLayers(); invalidateCachedGLState();
			WARN_ONCE(s_xrLayerFailure,"P5 layer allocation failed; composed fallback retained"); return false;
		}
		fprintf(stderr,"[d3d8gles] P5 MRT layers allocated %dx%d; full composed fallback retained\n",m_offW,m_offH);
	}
	glDisable(GL_SCISSOR_TEST); glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
	// GeneralsX @performance Codex 14/09/2026 Never snapshot an incomplete
	// ordinary world. UI still writes independently through the second MRT output.
	m_xrWorldSnapshotValid=m_xrElision.pendingComplete() && !(elideWorldCopy && m_xrStereoReady);
	if(m_xrWorldSnapshotValid) {
		glBindFramebuffer(GL_READ_FRAMEBUFFER,m_offFBO);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER,m_xrWorldFBO);
		glBlitFramebuffer(0,0,m_offW,m_offH,0,0,m_offW,m_offH,GL_COLOR_BUFFER_BIT,GL_NEAREST);
	}
	static unsigned copied=0,skipped=0;
	if(m_xrWorldSnapshotValid)++copied;else ++skipped;
	if(copied+skipped>=120) {
		fprintf(stderr,"[d3d8gles] P17 planar snapshot copied=%u skipped=%u (%dx%d); incomplete snapshots suppressed\n",copied,skipped,m_offW,m_offH);
		copied=skipped=0;
	}
	glBindFramebuffer(GL_FRAMEBUFFER,m_offFBO);
	const GLenum outputs[]={GL_COLOR_ATTACHMENT0,GL_COLOR_ATTACHMENT1}; glDrawBuffers(2,outputs);
	const GLfloat transparent[]={0,0,0,0}; glClearBufferfv(GL_COLOR,1,transparent);
	m_xrUI=true; invalidateCachedGLState(); return true;
}

void WebGLPipeline::setRenderTarget(WebGLDevice * /*dev*/, WebGLTexture *tex)
{
	if (!m_ctxReady) return;

	if (tex == nullptr) {
		// GeneralsX @feature XR port Phase 1.0 - external-context mode has
		// no screen: default-target rendering redirects into the owned FBO,
		// which the XR quad samples. m_curFBO deliberately stays 0 (the
		// engine's "backbuffer" fiction); writing the offscreen FBO here
		// desyncs the tracker and freezes/stales frames (Phase 0.2 lesson).
		if (m_xrMode && ensureOffscreenTarget()) {
			m_curFBO=0; // returning from reflections must reset the logical target too
			m_curRTWidth = m_fbWidth;
			m_curRTHeight = m_fbHeight;
			m_yFlip = 1.0f;
			return;
		}
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		m_curFBO = 0;
		m_curRTWidth = m_fbWidth;
		m_curRTHeight = m_fbHeight;
		// GeneralsX @build Android port GLES experiment - this was +1.0f as
		// ported from the web build. On a browser canvas, the browser's own
		// canvas compositing absorbs part of the D3D-to-GL vertical
		// convention mismatch, so +1.0f was correct there. Android's raw
		// EGL/ANativeWindow presentation (via SDL_GL_SwapWindow) has no such
		// implicit correction, and +1.0f produced a confirmed whole-frame
		// vertical flip on a real device (menu buttons, logo, and the 3D
		// background all appeared upside down and in reversed top-to-bottom
		// order, matched against a known-correct reference screenshot).
		m_yFlip = 1.0f;
		return;
	}

	// The GL texture must exist before it can be an attachment.
	if (tex->m_gl.name == 0 || tex->m_gl.dirty) {
		uploadTexture(tex);
		// GeneralsX @build Android port GLES experiment - part 2/2 of the
		// texture-bind cache perf fix (see m_lastBoundTex's declaration for
		// part 1/2). uploadTexture() just did its own raw glBindTexture on
		// whatever unit bindTextures()'s last draw left active, behind
		// m_lastBoundTex's back -- GL_TEXTURE_BINDING_2D for that unit is now
		// this render-target texture, not whatever bindTextures() last
		// recorded. Without this, the next draw's bindTextures() could
		// wrongly conclude the intended sampler texture is "already bound"
		// (stale cache hit) and skip the real bind, sampling this RT texture
		// instead. Stomp both slots with an impossible GL name so the next
		// bindTextures() call is always forced to re-bind for real.
		m_lastBoundTex[0] = m_lastBoundTex[1] = ~0u;
	}

	const int w = (int)tex->m_levels[0]->m_width;
	const int h = (int)tex->m_levels[0]->m_height;

	if (tex->m_gl.fbo == 0) {
		glGenFramebuffers(1, &tex->m_gl.fbo);
		glBindFramebuffer(GL_FRAMEBUFFER, tex->m_gl.fbo);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex->m_gl.name, 0);
	} else {
		glBindFramebuffer(GL_FRAMEBUFFER, tex->m_gl.fbo);
	}

	// Shared depth-stencil renderbuffer, recreated on size change.
	if (m_depthRB == 0 || m_depthRBW != w || m_depthRBH != h) {
		if (m_depthRB) glDeleteRenderbuffers(1, &m_depthRB);
		glGenRenderbuffers(1, &m_depthRB);
		glBindRenderbuffer(GL_RENDERBUFFER, m_depthRB);
		glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, w, h);
		m_depthRBW = w;
		m_depthRBH = h;
	}
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_depthRB);

	const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	if (status != GL_FRAMEBUFFER_COMPLETE) {
		WARN_ONCE(s_fboIncomplete, "FBO incomplete: 0x%x (%dx%d fmt=%d)", status, w, h, (int)tex->m_format);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		m_curFBO = 0;
		m_curRTWidth = m_fbWidth;
		m_curRTHeight = m_fbHeight;
		// GeneralsX @build Android port GLES experiment - this was +1.0f as
		// ported from the web build. On a browser canvas, the browser's own
		// canvas compositing absorbs part of the D3D-to-GL vertical
		// convention mismatch, so +1.0f was correct there. Android's raw
		// EGL/ANativeWindow presentation (via SDL_GL_SwapWindow) has no such
		// implicit correction, and +1.0f produced a confirmed whole-frame
		// vertical flip on a real device (menu buttons, logo, and the 3D
		// background all appeared upside down and in reversed top-to-bottom
		// order, matched against a known-correct reference screenshot).
		m_yFlip = 1.0f;
		return;
	}

	m_curFBO = tex->m_gl.fbo;
	m_curRTWidth = w;
	m_curRTHeight = h;
	// Same y-flip as the backbuffer: D3D's top row then lands in texel row 0,
	// which is exactly what engine UVs (v=0 = top) expect when sampling.
	m_yFlip = 1.0f;
	// Rendered content supersedes the CPU shadow from now on.
	tex->m_gl.dirty = false;
}

// GeneralsX @bugfix Android port 09/05/2026 See the declaration in
// gles_pipeline.h. Projected shadows (W3DProjectedShadow::updateTexture) render
// an object's silhouette into a 512x512 render target and then blit it into a
// permanent texture with SurfaceClass::Copy -> _Copy_DX8_Rects -> CopyRects.
// CopyRects is a CPU memcpy between shadow bits, and a render target's shadow
// bits were never written by anything, so the permanent shadow texture came out
// all zeroes. Drawn with _PresetMultiplicativeShader (dst *= src), an all-zero
// texture multiplies the terrain to black across the whole decal quad -- which
// is exactly the "strange" helicopter shadows reported on GLES and never on
// DXVK (DXVK's CopyRects does a real GPU copy). Only SHADOW_PROJECTION shadows
// are generated this way; ground units use artist-supplied decal textures,
// which is why the symptom looked aircraft-specific.
void WebGLPipeline::readbackRenderTarget(WebGLTexture *tex)
{
	if (!m_ctxReady || tex == nullptr || tex->m_gl.fbo == 0 || tex->m_levels.empty()) return;

	WebGLSurface *s = tex->m_levels[0];
	const int w = (int)s->m_width;
	const int h = (int)s->m_height;
	if (w <= 0 || h <= 0) return;

	// glReadPixels' guaranteed-supported combination in GLES3 is RGBA/UBYTE,
	// and the only render-target format this engine creates is A8R8G8B8
	// (DX8Wrapper::Create_Render_Target, with an X8R8G8B8 fallback).
	if (tex->m_format != D3DFMT_A8R8G8B8 && tex->m_format != D3DFMT_X8R8G8B8) {
		WARN_ONCE_ARG((unsigned)tex->m_format,
			"render-target readback for format %u not implemented",
			(unsigned)tex->m_format);
		return;
	}
	if (s->m_bits.size() < (size_t)h * s->m_pitch) return;

	m_rtReadback.resize((size_t)w * h * 4);
	glBindFramebuffer(GL_FRAMEBUFFER, tex->m_gl.fbo);
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, m_rtReadback.data());
	glBindFramebuffer(GL_FRAMEBUFFER, m_curFBO);

	// Row 0 of this FBO's texture attachment is D3D's top row (setRenderTarget
	// keeps m_yFlip at +1 precisely so it lands there), and glReadPixels
	// numbers rows from attachment row 0 upward -- so the rows come back in
	// the same order the CPU shadow stores them. No vertical flip here.
	// Bytes still need RGBA -> BGRA, the inverse of the conversion
	// prepareLevelUpload() does when pushing an A8R8G8B8 surface to GL.
	for (int y = 0; y < h; y++) {
		const uint8_t *src = m_rtReadback.data() + (size_t)y * w * 4;
		uint8_t *dst = s->m_bits.data() + (size_t)y * s->m_pitch;
		for (int x = 0; x < w; x++) {
			dst[x * 4 + 0] = src[x * 4 + 2]; // B
			dst[x * 4 + 1] = src[x * 4 + 1]; // G
			dst[x * 4 + 2] = src[x * 4 + 0]; // R
			dst[x * 4 + 3] = src[x * 4 + 3]; // A
		}
	}
}

void WebGLPipeline::debugSampleRenderTarget(WebGLTexture *tex, const char *tag)
{
	static int s_samplesLeft = 4;
	if (s_samplesLeft <= 0) return;
	if (!m_ctxReady || tex == nullptr || tex->m_gl.fbo == 0 || tex->m_levels.empty()) return;

	const int w = (int)tex->m_levels[0]->m_width;
	const int h = (int)tex->m_levels[0]->m_height;
	// Only the screen filters' target is backbuffer-sized; water reflections and
	// projected shadows are small and would drown this out.
	if (w != m_fbWidth || h != m_fbHeight || w <= 0 || h <= 0) return;
	s_samplesLeft--;

	const int bw = w < 64 ? w : 64;
	const int bh = h < 64 ? h : 64;
	const int bx = (w - bw) / 2;
	const int by = (h - bh) / 2;

	std::vector<uint8_t> px((size_t)bw * bh * 4);
	const GLuint prevFBO = m_curFBO;
	glBindFramebuffer(GL_FRAMEBUFFER, tex->m_gl.fbo);
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glReadPixels(bx, by, bw, bh, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
	const GLenum err = glGetError();
	glBindFramebuffer(GL_FRAMEBUFFER, prevFBO);

	double sum[3] = {0.0, 0.0, 0.0};
	int mx[3] = {0, 0, 0};
	int nonBlack = 0;
	const int n = bw * bh;
	for (int i = 0; i < n; i++) {
		const uint8_t *p = px.data() + (size_t)i * 4;
		for (int c = 0; c < 3; c++) {
			sum[c] += p[c];
			if (p[c] > mx[c]) mx[c] = p[c];
		}
		if (p[0] || p[1] || p[2]) nonBlack++;
	}

	fprintf(stderr, "[GX-FILTER] rt-sample %s %dx%d block=%dx%d@%d,%d "
		"avg=(%.1f,%.1f,%.1f) max=(%d,%d,%d) nonblack=%d/%d glerr=0x%x\n",
		tag, w, h, bw, bh, bx, by,
		sum[0] / n, sum[1] / n, sum[2] / n, mx[0], mx[1], mx[2], nonBlack, n, (unsigned)err);
	fflush(stderr);
}

void WebGLPipeline::captureOffscreenPPM()
{
	glBindFramebuffer(GL_READ_FRAMEBUFFER, m_offFBO);
	// RGBA, not RGB: GL_RGB/UNSIGNED_BYTE from an RGBA8 buffer is a
	// GL_INVALID_OPERATION (0x502) on Adreno -- readback stays zeroed
	// and the PPM comes out black (seen on device, twice). Strip the
	// alpha on the CPU instead.
	std::vector<uint8_t> px((size_t)m_offW * m_offH * 4);
	glReadPixels(0, 0, m_offW, m_offH, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
	const char *path = "/sdcard/Download/GeneralsZH/gx_offscreen.ppm";
	FILE *f = fopen(path, "wb");
	if (f) {
		fprintf(f, "P6\n%d %d\n255\n", m_offW, m_offH);
		std::vector<uint8_t> row((size_t)m_offW * 3);
		for (int y = m_offH - 1; y >= 0; --y) {
			const uint8_t *src = &px[(size_t)y * m_offW * 4];
			for (int x = 0; x < m_offW; ++x) {
				row[(size_t)x * 3 + 0] = src[(size_t)x * 4 + 0];
				row[(size_t)x * 3 + 1] = src[(size_t)x * 4 + 1];
				row[(size_t)x * 3 + 2] = src[(size_t)x * 4 + 2];
			}
			fwrite(row.data(), 1, row.size(), f);
		}
		fclose(f);
	}
	fprintf(stderr, "[d3d8gles] XR spike: offscreen capture %dx%d -> %s (%s, bind-mismatch=%u)\n",
		m_offW, m_offH, path, f ? "ok" : "FAILED", m_offBindMismatch);
}

// GeneralsX @performance Codex 14/09/2026 Publish validity per native Present,
// including nested loading/video Presents, not per outer simulation frame.
void WebGLPipeline::finishXRFrame()
{
	m_xrElision.publish(m_xrStereoReady,m_xrUI);
	static unsigned frames=0,skipped=0;
	skipped+=m_xrElision.publishedSkipped;
	if(++frames>=120) {
		fprintf(stderr,"[d3d8gles] P17 ordinary world saved=%u draws/120 frames; requested=%d recovery=%d\n",
			skipped,int(m_xrElision.requested),int(m_xrElision.blocked));
		frames=skipped=0;
	}
}
void WebGLPipeline::present()
{
	if (!m_ctxReady) return;
	m_frame++;

	// GeneralsX @spike XR port Phase 0.2 - marker-gated so the capture can
	// be toggled mid-session from adb without a rebuild. XR mode (1.0) owns
	// its FBO permanently and never polls the marker.
	if (!m_xrMode && (m_frame % 60) == 0) {
		FILE *mk = fopen("/sdcard/Download/GeneralsZH/gx_xr_offscreen.txt", "rb");
		if (mk) fclose(mk);
		setOffscreenCapture(mk != nullptr);
	}

	GLenum err = glGetError();
	if(m_xrMode && err!=GL_NO_ERROR && m_xrElision.pendingMissing) {
		m_xrStereoReady=false;m_xrElision.requireFullWorld();
	}
	if (err != GL_NO_ERROR && (m_frame % 60) == 1) {
		fprintf(stderr, "[d3d8gles] glGetError at frame %u: 0x%x\n", m_frame, err);
	}

	// GeneralsX @build Android port GLES experiment - perf visibility.
	// Logged once every ~2s (not every frame, to keep this from becoming
	// its own source of overhead/spam) so real numbers -- FPS, draws/frame,
	// how often the applyFixedState redundant-state cache actually hits --
	// are available from a device log instead of judging smoothness by feel.
	m_perfFrameCount++;
	m_perfDrawAccum += m_perfDrawsThisFrame;
	m_perfDrawsThisFrame = 0;
	{
		const unsigned nowMs = SDL_GetTicks();
		if (m_perfLogLastMs == 0) {
			m_perfLogLastMs = nowMs;
		} else if (nowMs - m_perfLogLastMs >= 2000) {
			const float seconds = (nowMs - m_perfLogLastMs) / 1000.0f;
			const float fps = m_perfFrameCount / seconds;
			const float drawsPerFrame = m_perfFrameCount > 0
				? (float)m_perfDrawAccum / m_perfFrameCount : 0.0f;
			const int totalStateChecks = m_perfStateCacheHits + m_perfStateCacheMisses;
			const float cacheHitPct = totalStateChecks > 0
				? 100.0f * m_perfStateCacheHits / totalStateChecks : 0.0f;
			const int totalVAOChecks = m_perfVAOCacheHits + m_perfVAOCacheMisses;
			const float vaoHitPct = totalVAOChecks > 0
				? 100.0f * m_perfVAOCacheHits / totalVAOChecks : 0.0f;
			const int totalUniformChecks = m_perfUniformCacheHits + m_perfUniformCacheMisses;
			const float uniformHitPct = totalUniformChecks > 0
				? 100.0f * m_perfUniformCacheHits / totalUniformChecks : 0.0f;
			// GeneralsX @perf Android port 09/05/2026 Per-subsystem draw split,
			// averaged over the same window as draws/frame above.
			{
				const float f = m_perfFrameCount > 0 ? (float)m_perfFrameCount : 1.0f;
				const float models = s_gxDrawsByCategory[GX_DRAWCAT_MODELS] / f;
				const float sorted = s_gxDrawsByCategory[GX_DRAWCAT_SORTED] / f;
				const float ui = s_gxDrawsByCategory[GX_DRAWCAT_2D] / f;
				const float terrain = s_gxDrawsByCategory[GX_DRAWCAT_TERRAIN] / f;
				const float shadows = s_gxDrawsByCategory[GX_DRAWCAT_SHADOWS] / f;
				const float skin = s_gxDrawsByCategory[GX_DRAWCAT_SKIN] / f;
				const float other = s_gxDrawsByCategory[GX_DRAWCAT_OTHER] / f;
				fprintf(stderr, "[d3d8gles] perf-draws/frame by source: models=%.1f sorted(particles)=%.1f "
					"2d-ui=%.1f terrain=%.1f shadows=%.1f skin=%.1f other=%.1f\n",
					models, sorted, ui, terrain, shadows, skin, other);
#if defined(__ANDROID__)
				// Release APKs do not route native stderr into adb logcat. Mirror
				// the already-computed two-second XR draw split so P26 can choose
				// between model culling and terrain batching from device evidence.
				// This is diagnostic only: counters and rendering remain unchanged.
				if (m_xrMode) {
					__android_log_print(ANDROID_LOG_INFO, "gx-perf-draws",
						"models=%.1f sorted=%.1f ui=%.1f terrain=%.1f shadows=%.1f skin=%.1f other=%.1f total=%.1f",
						models, sorted, ui, terrain, shadows, skin, other, drawsPerFrame);
				}
#endif
				fprintf(stderr, "[d3d8gles] perf-ui ms/frame: text-raster=%.2f text-texture=%.2f 2d-submit=%.2f\n",
					s_gxUiTimeUs[0] / 1000.0 / f,
					s_gxUiTimeUs[1] / 1000.0 / f,
					s_gxUiTimeUs[2] / 1000.0 / f);
				for (int i = 0; i < 3; i++) s_gxUiTimeUs[i] = 0.0;
				for (int i = 0; i < GX_DRAWCAT_COUNT; i++) s_gxDrawsByCategory[i] = 0;
			}
			fprintf(stderr, "[d3d8gles] perf: %.1f fps, %.1f draws/frame, "
				"state-cache %.0f%% hit (%d/%d), vao-cache %.0f%% hit (%d/%d, %zu cached, "
				"%d ptr-refresh), uniform-cache %.0f%% hit (%d/%d), "
				"textures live=%ld (created=%ld deleted=%ld), "
				"programs built=%d (%.1fms total, %.2fms avg, %d cached)\n",
				fps, drawsPerFrame, cacheHitPct, m_perfStateCacheHits, totalStateChecks,
				vaoHitPct, m_perfVAOCacheHits, totalVAOChecks, m_vaoCache.size(), m_perfVAOPointerRefresh,
				uniformHitPct, m_perfUniformCacheHits, totalUniformChecks,
				g_texturesCreated - g_texturesDeleted, g_texturesCreated, g_texturesDeleted,
				m_perfProgramBuilds, m_perfProgramBuildUs / 1000.0,
				m_perfProgramBuilds > 0 ? m_perfProgramBuildUs / 1000.0 / m_perfProgramBuilds : 0.0,
				m_programCount);
			if(m_xrRestores)fprintf(stderr,"[d3d8gles] P12.2 narrow-state restores=%u GLcalls=%u; unchanged draws/eye extents\n",m_xrRestores,m_xrRestoreCalls);
			m_xrRestores=m_xrRestoreCalls=0;
			// GeneralsX @build Android port GLES experiment 08/30/2026 Per-
			// block breakdown of the combined uniform-cache rate above. A
			// real device log with this line's first version (transform =
			// view+proj+texMat0+texMat1 as one combined key) showed
			// transform/misc collapsing to ~35% hit rate mid-battle while
			// material/lighting held 82-88% -- suspicious, since the camera
			// (view/proj) is set once per frame and should hit on nearly
			// every draw. Split transform into separate viewproj/texmat
			// buckets (gles_pipeline.h's ViewProjKey/TexMatKey) to confirm
			// whether per-object texture-stage transforms (UV scroll/glow
			// animations) were invalidating the whole combined key on every
			// such draw and forcing a spurious view/proj re-upload too.
			{
				auto pct = [](int hits, int misses) {
					int total = hits + misses;
					return total > 0 ? 100.0f * hits / total : 0.0f;
				};
				fprintf(stderr, "[d3d8gles] uniform-cache breakdown: viewproj %.0f%% (%d/%d), "
					"texmat %.0f%% (%d/%d), misc %.0f%% (%d/%d), material %.0f%% (%d/%d), lighting %.0f%% (%d/%d)\n",
					pct(m_perfUniformViewProjHits, m_perfUniformViewProjMisses),
					m_perfUniformViewProjHits, m_perfUniformViewProjHits + m_perfUniformViewProjMisses,
					pct(m_perfUniformTexMatHits, m_perfUniformTexMatMisses),
					m_perfUniformTexMatHits, m_perfUniformTexMatHits + m_perfUniformTexMatMisses,
					pct(m_perfUniformMiscHits, m_perfUniformMiscMisses),
					m_perfUniformMiscHits, m_perfUniformMiscHits + m_perfUniformMiscMisses,
					pct(m_perfUniformMaterialHits, m_perfUniformMaterialMisses),
					m_perfUniformMaterialHits, m_perfUniformMaterialHits + m_perfUniformMaterialMisses,
					pct(m_perfUniformLightingHits, m_perfUniformLightingMisses),
					m_perfUniformLightingHits, m_perfUniformLightingHits + m_perfUniformLightingMisses);
			}
			// GeneralsX @bugfix Android port 09/04/2026 Diagnostic for a
			// persistent strip near the screen edge reported on GLES/ANGLE
			// (present at native/100% resolution too, so it's not a
			// pillarbox-blit artifact -- confirmed by real-device testing
			// that ruled out every pillarbox-side theory tried so far).
			// Everything on the C++ bookkeeping side (m_fbWidth/Height,
			// _PresentParameters, Get_Render_Target_Resolution) claims the
			// backbuffer is the full real window size, but that's all
			// self-reported -- this logs the ACTUAL GL state at present()
			// time (real glViewport, not what we think we set) to see
			// whether it agrees. If GL_VIEWPORT doesn't match
			// m_fbWidth/m_fbHeight here, the bug is in how a viewport call
			// got lost/cached wrong somewhere in this file. If it DOES
			// match, the bug is further upstream (EGL surface/window
			// geometry itself), outside this pipeline's control.
			{
				GLint vpDump[4] = {-1, -1, -1, -1};
				glGetIntegerv(GL_VIEWPORT, vpDump);
				fprintf(stderr, "[d3d8gles-diag] present(): GL_VIEWPORT=(%d,%d,%d,%d) m_fbWidth=%d m_fbHeight=%d "
					"m_curRTWidth=%d m_curRTHeight=%d m_curFBO=%u m_yFlip=%.1f\n",
					vpDump[0], vpDump[1], vpDump[2], vpDump[3],
					m_fbWidth, m_fbHeight, m_curRTWidth, m_curRTHeight,
					(unsigned)m_curFBO, m_yFlip);
			}
			DumpLiveTextureShapes();
			m_perfLogLastMs = nowMs;
			m_perfFrameCount = 0;
			m_perfDrawAccum = 0;
			m_perfStateCacheHits = 0;
			m_perfStateCacheMisses = 0;
			m_perfVAOCacheHits = 0;
			m_perfVAOCacheMisses = 0;
			m_perfVAOPointerRefresh = 0;
			m_perfUniformCacheHits = 0;
			m_perfUniformCacheMisses = 0;
			m_perfUniformViewProjHits = 0;
			m_perfUniformViewProjMisses = 0;
			m_perfUniformTexMatHits = 0;
			m_perfUniformTexMatMisses = 0;
			m_perfUniformMiscHits = 0;
			m_perfUniformMiscMisses = 0;
			m_perfUniformMaterialHits = 0;
			m_perfUniformMaterialMisses = 0;
			m_perfUniformLightingHits = 0;
			m_perfUniformLightingMisses = 0;
			m_perfProgramBuilds = 0;
			m_perfProgramBuildUs = 0.0;
		}
	}

	// GeneralsX @build Android port GLES experiment - the browser build never
	// needed an explicit swap (the canvas presents implicitly when the game
	// pthread yields back to its rAF loop tick). Android/EGL has no such
	// implicit hook, so this call is new, not adapted from upstream.
	if (m_xrMode) {
		finishXRFrame();
		m_xrSplitReady=m_xrUI;
		m_xrUI=false;
		if(m_offFBO) {
			glBindFramebuffer(GL_FRAMEBUFFER,m_offFBO);
			const GLenum one=GL_COLOR_ATTACHMENT0; glDrawBuffers(1,&one);
		}
		m_haveFixedStateKey=false;
		// GeneralsX @feature XR port Phase 1.0 - nothing to composite to:
		// the frame already sits in m_offFBO (redirect hooks above) and the
		// XR quad samples it from there. No blit, no swap. File readback is
		// explicitly enabled with GX_XR_CAPTURE_PPM; it is intentionally off
		// by default because glReadPixels stalls the shared render/XR thread.
		if (m_xrCaptureEnabled && (m_frame % 600) == 0 && m_offFBO != 0) {
			captureOffscreenPPM();
		}
	} else if (m_offscreenCapture) {
	// GeneralsX @spike XR port Phase 0.2 - snoop the finished frame: copy
	// backbuffer 0 (the true final image) into the owned capture FBO. The
	// screen path below is untouched. The live GL bindings are saved and
	// restored around the blit -- restoring to the m_curFBO fiction instead
	// produced black artifacts on device, so the fiction is not always the
	// truth (m_offBindMismatch counts how often).
		GLint prevRead = 0, prevDraw = 0;
		glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &prevRead);
		glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &prevDraw);
		if (prevDraw != (GLint)m_curFBO) m_offBindMismatch++;
		if (ensureOffscreenTarget() && m_offW == m_fbWidth && m_offH == m_fbHeight) {
			glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
			glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_offFBO);
			glBlitFramebuffer(0, 0, m_offW, m_offH, 0, 0, m_offW, m_offH, GL_COLOR_BUFFER_BIT, GL_NEAREST);
			// Periodic (every ~20s at 30fps), overwriting the same file: an early
			// capture is a dark video frame, so a one-shot proves nothing about
			// the menu / 3D scene. The newest file wins.
			if ((m_frame % 600) == 0) {
			captureOffscreenPPM();
			}
		}
		// Restore the live bindings saved above: the blit must not leak
		// READ/DRAW state into the next frame.
		glBindFramebuffer(GL_READ_FRAMEBUFFER, (GLuint)prevRead);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, (GLuint)prevDraw);
	}

	if (m_window) {
		SDL_GL_SwapWindow(m_window);
	}
}
