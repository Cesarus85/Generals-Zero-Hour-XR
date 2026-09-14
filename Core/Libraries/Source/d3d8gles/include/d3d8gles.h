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
** d3d8gles.h - public entry points for the native GLES3 D3D8 backend.
**
** GeneralsX @build Android port GLES experiment - ported from
** Lolendor/Generals-WebAssembly's d3d8webgl (D3D8 -> WebGL2), adapted to
** native GLES3 via SDL3 instead of Emscripten/WebGL2. See
** Core/Libraries/Source/d3d8gles/src/d3d8gles.cpp for the implementation.
*/

#pragma once

#include <d3d8.h>

// Statically linked, unlike DXVK's Direct3DCreate8 which is dlopen'd from
// libdxvk_d3d8.so -- named distinctly so both can coexist in libmain.so.
extern "C" IDirect3D8 *WINAPI Direct3DCreate8_GLES(UINT sdkVersion);

// Called from the SDL3 window-resize path so the GLES pipeline's cached
// framebuffer size stays in sync without waiting for the next Reset().
extern "C" void d3d8gles_resize(int w, int h);

// GeneralsX @build Android port render-backend picker 07/09/2026 - single
// source of truth for the Vulkan/GLES/GLES+ANGLE choice, called from every
// place that needs to agree on it: SDL3Main.cpp (decides which kind of SDL
// window/EGL surface to create) and dx8wrapper.cpp (decides whether to load
// libdxvk_d3d8.so or use Direct3DCreate8_GLES). These USED to be two
// separate copies of the same getenv() check; they drifted out of sync the
// moment the Setup app's render_backend.cfg picker was added to only one of
// them (SDL3Main.cpp), so a phone with "Vulkan" selected got a Vulkan SDL
// window from SDL3Main.cpp but dx8wrapper.cpp still silently loaded the GLES
// backend underneath it -- SDL_GL_CreateContext then failed ("the specified
// window isn't an OpenGL window") and nothing ever rendered, while the rest
// of the engine (audio, game logic) ran fine. Implemented in d3d8gles.cpp
// (which already links sdl3lib) so both callers -- one of which, WW3D2, does
// NOT itself link sdl3lib -- can share one implementation instead of each
// re-reading render_backend.cfg/the env vars themselves.
extern "C" bool d3d8gles_ShouldUseVulkanBackend();

// GeneralsX @perf Android port 09/05/2026 Draw-call breakdown by subsystem.
// Engine code tags the passes it can identify cheaply so the per-frame perf log
// can report where the ~1200-2500 draws/frame actually come from; anything
// untagged counts as OTHER. Sets the current category and returns the previous
// one, so callers restore it and nesting stays correct.
enum {
	D3D8GLES_DRAWCAT_OTHER  = 0,
	D3D8GLES_DRAWCAT_MODELS = 1,  // DX8TextureCategoryClass::Render -- rigid HLod meshes
	D3D8GLES_DRAWCAT_SORTED = 2,  // SortingRendererClass::Flush -- particles, decals
	D3D8GLES_DRAWCAT_2D     = 3   // Render2DClass::Render -- all UI and video
};
extern "C" int d3d8gles_SetDrawCategory(int category);

// GeneralsX @perf Android port 09/05/2026 Extra draw categories. Real-device
// timings showed the frame is CPU-bound with present() at only 0.5-2.5ms, and
// "other" was the single biggest draw bucket at ~560/frame -- split it so the
// next optimization has a target instead of a guess.
enum {
	D3D8GLES_DRAWCAT_TERRAIN = 4,  // HeightMapRenderObjClass::Render
	D3D8GLES_DRAWCAT_SHADOWS = 5,  // W3DProjectedShadowManager::renderShadows
	D3D8GLES_DRAWCAT_SKIN    = 6   // DX8SkinFVFCategoryContainer::Render
};

// GeneralsX @perf Android port 09/05/2026 UI cost breakdown. [GX-PERF-DISPLAY]
// puts uiWidgets (TheInGameUI->DRAW()) at 40-55ms/frame, spiking to 170-190ms,
// against mainScene at 30-42ms -- i.e. the UI costs as much as the entire 3D
// scene while issuing a fifth of the draws. These buckets say which part of it:
// glyph rasterization, glyph-atlas texture building, or 2D draw submission.
enum {
	D3D8GLES_UITIME_TEXT_RASTER  = 0,  // Render2DSentenceClass::Build_Sentence
	D3D8GLES_UITIME_TEXT_TEXTURE = 1,  // Render2DSentenceClass::Build_Textures
	D3D8GLES_UITIME_2D_SUBMIT    = 2,  // Render2DClass::Render
	D3D8GLES_UITIME_COUNT        = 3
};
extern "C" void d3d8gles_AddUiTiming(int bucket, double microseconds);
extern "C" bool d3d8gles_ShouldUseANGLE();

// GeneralsX @feature XR port Phase 1.0 - external-context ("XR") mode. Set
// BEFORE the engine creates its D3D device: the pipeline then skips all SDL
// window handling (no SDL_GL_CreateContext/MakeCurrent/SwapWindow -- there
// is no window) and renders into an owned FBO on the caller's already-
// current GLES context instead. The finished frame is readable as a plain
// GL texture via d3d8gles_GetGameTexture() for the XR tabletop quad.
// The EGL context MUST come from the system EGL implementation: the GL
// dispatch is forced to system libGLESv3.so in this mode (ANGLE pointers
// against a system context would be garbage). Pass nullptr to disable
// (default); the config pointer must stay valid until device creation.
struct D3D8GLES_XRConfig {
	void *eglDisplay; // EGLDisplay, informational (context must be current already)
	void *eglContext; // EGLContext, informational
};
extern "C" void d3d8gles_SetXRConfig(const struct D3D8GLES_XRConfig *cfg);
extern "C" const struct D3D8GLES_XRConfig *d3d8gles_GetXRConfig();
// Game frame texture (owned FBO color attachment), valid once a frame has
// rendered in XR mode, 0 otherwise. Sampled by the XR quad; contents are
// bottom-up GL-native (XR UVs must flip V, like the PPM writer does).
extern "C" unsigned int d3d8gles_GetGameTexture();
// GeneralsX @feature Codex 13/09/2026 XR layer lifecycle, all on the render thread.
extern "C" void d3d8gles_BeginXRFrame(bool split,bool elideOrdinaryWorld=false);
// P17: incomplete ordinary frames must never be presented or used for input.
extern "C" void d3d8gles_RequireXRFullWorld();
extern "C" unsigned d3d8gles_XROrdinarySkipped();
extern "C" bool d3d8gles_BeginXRUI(bool elideWorldCopy=false);
extern "C" bool d3d8gles_XRSplitReady();
extern "C" unsigned int d3d8gles_GetXRWorldTexture();
extern "C" unsigned int d3d8gles_GetXRUITexture();
// GeneralsX @feature Codex 13/09/2026 Immediate geometry replay; no engine callback replay.
extern "C" bool d3d8gles_BeginXRStereo(int width,int height,const float *leftClip,const float *rightClip,const float *board,float aspect,const float *camera,bool atlas,bool multiview=false);
extern "C" void d3d8gles_ConfigureXRMultiview(void *(*resolver)(const char *));
extern "C" bool d3d8gles_XRStereoMultiview();
extern "C" bool d3d8gles_XRStereoAtlas();
extern "C" void d3d8gles_EndXRStereo();
// P9 vertices: XYZ + RGBA float, world coordinates; depth-tested in both eyes.
extern "C" void d3d8gles_DrawXRDecorations(const float *vertices,int count);
extern "C" unsigned int d3d8gles_XRStereoTexture(int eye);
// Drop all CPU-side GL caches after external (XR quad) rendering on the
// shared context -- see WebGLPipeline::invalidateCachedGLState().
extern "C" void d3d8gles_InvalidateCachedState();
