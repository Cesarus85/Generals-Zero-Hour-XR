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
** gles_pipeline.h - the D3D8 fixed-function -> WebGL2 translation core.
**
** GeneralsX @build web-port 05/07/2026 - Web port Phase 2
**
** The device/resource classes in d3d8webgl.cpp keep full CPU-side shadow
** state (render states, stage states, transforms, lights, buffer and texture
** bytes). This pipeline consumes that state at draw time: it generates and
** caches GLSL ES 3.00 programs keyed on the fixed-function state that affects
** shading, uploads dirty resources, and issues the GL draws.
*/

#pragma once
#include "XRMultiview.h"
#include "XRWorldElision.h"

#include <GLES3/gl3.h>
#include <cstdint>
#include <cstring>
#include <unordered_map>
#include <vector>

typedef struct SDL_Window SDL_Window;
// Not "typedef void *SDL_GLContext" here: SDL3's real header (SDL_video.h)
// declares SDL_GLContext as "struct SDL_GLContextState *", and a forward
// declaration must match exactly or every TU that includes both this header
// and <SDL3/SDL.h> gets a typedef-redefinition error. m_glContext below is
// plain void* instead; gles_pipeline.cpp (which does include <SDL3/SDL.h>)
// casts to/from the real SDL_GLContext where needed.

class WebGLDevice;
class WebGLTexture;
class WebGLVertexBuffer;
class WebGLIndexBuffer;
struct FVFLayout; // defined in gles_pipeline.cpp, only used by-reference here

// GL side of a texture: one GL object, recreated when the shadow bits change.
struct GLTextureState {
	GLuint name = 0;
	bool dirty = true;          // shadow bits changed since last upload
	uint32_t samplerKey = ~0u;  // last-applied filter/wrap state
	GLuint fbo = 0;             // lazily created when used as a render target
};

// GL side of a VB/IB.
struct GLBufferState {
	GLuint name = 0;
	bool dirty = true;
	// GeneralsX @perf Android port 09/05/2026 Byte range actually written since
	// the last upload, accumulated across Lock/Unlock pairs (see
	// WebGLVertexBuffer::Lock/Unlock in d3d8gles.cpp). ensureVBUploaded/
	// ensureIBUploaded push only this range instead of respecifying the whole
	// buffer. begin >= end means "nothing recorded" -> fall back to a full
	// upload. `allocated` tracks whether GL storage exists yet, since
	// glBufferSubData needs a sized buffer to write into.
	size_t dirtyBegin = (size_t)-1;
	size_t dirtyEnd = 0;
	bool allocated = false;
	// GeneralsX @perf Android port 09/05/2026 Set when the engine locked with
	// D3DLOCK_DISCARD (dx8vertexbuffer.cpp uses it at ring offset 0, and
	// NOOVERWRITE for the appends after it). Translated as buffer orphaning:
	// glBufferData(..., nullptr) hands back a fresh block, so neither that
	// upload nor the NOOVERWRITE appends that follow in the same ring cycle
	// have to wait on the GPU still reading the old contents.
	bool pendingDiscard = false;

	void markRange(size_t begin, size_t end)
	{
		dirty = true;
		if (end <= begin) return;
		if (begin < dirtyBegin) dirtyBegin = begin;
		if (end > dirtyEnd) dirtyEnd = end;
	}
	void clearRange()
	{
		dirtyBegin = (size_t)-1;
		dirtyEnd = 0;
	}
};

class WebGLPipeline {
public:
	static WebGLPipeline *get(); // created on first use (game pthread)

	// Context management. Returns false if GLES3 is unavailable.
	bool initContext(int backbufferWidth, int backbufferHeight, SDL_Window *window);
	// GeneralsX @feature XR port Phase 1.0 - init on the caller's already-
	// current EGL context (no SDL window). Entered automatically from
	// initContext() when d3d8gles_GetXRConfig() is set: default-target
	// rendering redirects into the owned FBO and present() never swaps.
	bool initContextExternal(int backbufferWidth, int backbufferHeight);
	void resize(int backbufferWidth, int backbufferHeight);
	bool ready() const { return m_ctxReady; }

	// D3D entry points (called from WebGLDevice with `this` device state).
	void clear(WebGLDevice *dev, unsigned flags, uint32_t argbColor, float z, unsigned stencil);
	void drawIndexed(WebGLDevice *dev, unsigned primType, unsigned minIndex,
	                 unsigned numVertices, unsigned startIndex, unsigned primCount);
	void draw(WebGLDevice *dev, unsigned primType, unsigned startVertex, unsigned primCount);
	void drawUP(WebGLDevice *dev, unsigned primType, unsigned primCount,
	            const void *vertexData, unsigned stride);
	void drawIndexedUP(WebGLDevice *dev, unsigned primType, unsigned minVertexIdx,
	                   unsigned numVertices, unsigned primCount,
	                   const void *indexData, unsigned indexFormat,
	                   const void *vertexData, unsigned stride);
	void present();

	// Render-target switch: tex==nullptr selects the canvas backbuffer.
	void setRenderTarget(WebGLDevice *dev, WebGLTexture *tex);

	// GeneralsX @spike XR port Phase 0.2 - offscreen capture: when enabled,
	// present() copies the finished backbuffer (framebuffer 0, after ALL
	// rendering including any path that bypasses setRenderTarget/clear) into
	// an owned FBO. The owned color texture (offscreenTexture()) is what the
	// XR tabletop quad will sample in Phase 1. The on-screen image is never
	// modified -- capture is a snoop, not a redirect. Toggled via the
	// gx_xr_offscreen.txt marker file.
	void setOffscreenCapture(bool enabled);
	GLuint offscreenTexture() const { return !m_xrMode || m_xrElision.composedComplete ? m_offColorTex:0; }
	// GeneralsX @feature Codex 13/09/2026 One UI submission, full fallback plus isolated MRT output.
	void beginXRFrame(bool split,bool elideOrdinaryWorld=false);
	void finishXRFrame();
	void requireXRFullWorld() {m_xrElision.requireFullWorld();}
	// GeneralsX @performance Claude 26/09/2026 Single-pass object shroud (XR).
	// The engine publishes the shroud texture and its world->UV transform once
	// per frame; per-mesh draws then request folding instead of re-drawing the
	// mesh in a multiplicative second pass. See W3DScene.cpp / mesh.cpp.
	void setShroudTexture(WebGLTexture *tex,const float *worldToUV);
	void setShroudFold(bool active) {m_shroudFoldActive=active;}
	bool shroudFoldAvailable();
	unsigned xrOrdinarySkipped() const {return m_xrElision.publishedSkipped;}
	bool beginXRUI(bool elideWorldCopy=false);
	bool xrSplitReady() const { return m_xrSplitReady; }
	GLuint xrWorldTexture() const { return m_xrWorldSnapshotValid && !m_xrElision.pendingMissing && m_xrElision.composedComplete ? m_xrWorldTex:0; }
	GLuint xrUITexture() const { return m_xrUITex; }
	bool beginXRStereo(int width,int height,const float *left,const float *right,const float *board,float aspect,const float *camera,bool atlas,bool multiview=false);
	void endXRStereo();
	void drawXRDecorations(const float *vertices,int count);
	GLuint xrStereoTexture(int eye) const { return eye>=0 && eye<2 && m_xrStereoReady ? m_xrStereoTex[(m_xrStereoAtlas || m_xrStereoMultiview) ? 0:eye]:0; }
	bool xrStereoMultiview() const {return m_xrStereoReady && m_xrStereoMultiview;}
	void configureXRMultiview(GXMultiview::Resolver resolver) {m_xrMultiview={};m_xrMultiview.resolver=resolver;}
	bool xrStereoAtlas() const { return m_xrStereoReady && m_xrStereoAtlas; }

	// GeneralsX @feature XR port Phase 1.0 - the XR quad renderer shares
	// this context and binds its own program/VAO/FBO/textures behind the
	// pipeline's back. Call once after external rendering: drops every
	// CPU-side GL cache (program, VAO, fixed state, buffer/texture binds)
	// so the next game draw re-applies everything, and rebinds the render
	// target the engine believes is current. Uniform sub-keys self-heal
	// through the legacy program-change path or XR's lazy invalidation
	// epoch; the view/proj UBO is untouched by the XR side and stays valid.
	void invalidateCachedGLState();

	// GeneralsX @bugfix Android port 09/05/2026 Pull a render-target texture's
	// GPU contents back into its CPU shadow bits. Everything else in this
	// backend treats the CPU bits as the source of truth and pushes them to
	// GL; a render target is the one case where GL holds content the CPU side
	// has never seen. CopyRects (the D3D8 surface->surface blit) is a plain
	// memcpy between shadow bits, so a blit whose SOURCE is a render target
	// copied zeroes -- see the call site in d3d8gles.cpp's CopyRects for the
	// visual bug that caused.
	void readbackRenderTarget(WebGLTexture *tex);

	// GeneralsX @feature Android port 09/09/2026 One-shot diagnostic: read a small
	// block out of a full-screen render target and print what is actually in it.
	// The scripted screen filters (W3DShaderManager) render the scene into a
	// full-screen render target and then sample it back as a texture; when that
	// came out black on a device there was no way to tell an empty render target
	// from a bad combiner without a number. Prints at most a few times per launch
	// and ignores every render target that is not backbuffer-sized (water
	// reflections, projected shadows).
	void debugSampleRenderTarget(WebGLTexture *tex, const char *tag);

	bool hasS3TC() const { return m_hasS3TC; }

	// GeneralsX @build Android port GLES experiment - GL deletes a texture's
	// binding on every unit as a side effect of glDeleteTextures (the GL spec
	// guarantees this), but m_lastBoundTex doesn't see that -- and GL names
	// are commonly recycled by the driver, so a later, unrelated texture can
	// receive the exact name this cache still has recorded as "already bound"
	// for a stage. ~WebGLTexture() (d3d8gles.cpp) must call this right after
	// glDeleteTextures so a stale hit can never skip a real bind and leave
	// the wrong texture sampled.
	void invalidateTextureBinding(GLuint name);

	// Same rationale, covering both the GL_ARRAY_BUFFER skip cache
	// (m_lastArrayBuffer below) and the VAO cache (a cached VAO can name a
	// deleted VBO/IBO in its key, with the same reused-GL-name hazard):
	// ~WebGLVertexBuffer()/~WebGLIndexBuffer() (d3d8gles.cpp) must call this
	// right after glDeleteBuffers.
	void invalidateBufferBinding(GLuint name);

private:
	WebGLPipeline() = default;

	struct ProgramInfo;

	// Draw guts shared by the buffer and UP paths. vbo/ibo are explicit GL
	// object names (not read off some "currently bound" global) precisely
	// because with the VAO cache below, no per-draw call is guaranteed to
	// leave GL_ARRAY_BUFFER/GL_ELEMENT_ARRAY_BUFFER pointed at them -- a
	// cache hit skips touching those bindings entirely, and content uploads
	// (ensureVBUploaded/ensureIBUploaded) go through GL_COPY_WRITE_BUFFER,
	// never GL_ARRAY_BUFFER/GL_ELEMENT_ARRAY_BUFFER, for the same reason.
	void drawCommon(WebGLDevice *dev, unsigned primType, unsigned primCount,
	                GLuint vbo, unsigned stride, unsigned fvf,
	                GLuint ibo, unsigned indexFormat,
	                unsigned startIndex, int baseVertexBytes, unsigned vertexCount);

	ProgramInfo *getProgram(WebGLDevice *dev, unsigned fvf,bool multiview=false);
	void applyFixedState(WebGLDevice *dev);
	void applyUniforms(WebGLDevice *dev, ProgramInfo *prog, unsigned fvf);
	void ensureVBUploaded(WebGLVertexBuffer *vb);
	void ensureIBUploaded(WebGLIndexBuffer *ib);

	// GeneralsX @build Android port GLES experiment - perf pass. Only
	// GL_ARRAY_BUFFER gets a redundant-bind-skip cache: it is not part of
	// any VAO's state (a plain, VAO-independent global target, only ever
	// consulted transiently by glVertexAttribPointer/glBufferData), so
	// "was this name last bound here" is always a safe question to ask.
	// GL_ELEMENT_ARRAY_BUFFER is the opposite -- the GL/GLES spec makes it
	// part of *each* VAO's own state, so a single global "last bound"
	// answer can't say whether the *currently bound* VAO already has a
	// given index buffer captured; it is always bound unconditionally,
	// only from bindVertexLayout()'s VAO-creation path below. ~0u is an
	// impossible GL name, used as "unknown/force a real bind" the same way
	// m_lastBoundTex uses it for the RT-invalidation case; 0 is a real,
	// valid "unbound" state so it must round-trip too.
	GLuint m_lastArrayBuffer = ~0u;
	void bindArrayBuffer(GLuint name);

	// GeneralsX @build Android port GLES experiment - perf pass. No VAOs
	// existed anywhere in this backend: setupAttribs() ran in full (8x
	// glDisableVertexAttribArray, then glEnableVertexAttribArray +
	// glVertexAttribPointer for each active attribute) on every single draw,
	// unconditionally -- the one hot-path function bb4d069/this pass's other
	// caches never touched. A VAO captures that state (plus, per the
	// GL/GLES spec, the current GL_ELEMENT_ARRAY_BUFFER binding) once, keyed
	// on the combination that determines the *enabled-attribute set*: the
	// VBO/IBO GL object names (passed in explicitly by drawCommon's caller)
	// and the FVF (fully determines the parsed layout and its stride).
	//
	// Deliberately NOT keyed on the base-vertex byte offset, unlike this
	// cache's first version: a real device log (a battle scene, not even a
	// real skirmish) showed draws/frame up to ~1250 and this cache growing
	// by thousands of entries within seconds, one new VAO -- and GL object
	// -- for nearly every draw. Root cause: a lot of this engine's content
	// draws through one shared/dynamic vertex-buffer pool (DX8Wrapper's
	// BUFFER_TYPE_DYNAMIC_DX8) whose base-vertex offset advances on
	// practically every call, so folding it into the key meant that class
	// of content got treated as brand-new every single time, defeating the
	// cache and leaking VAOs for the session's lifetime. `base` is still
	// tracked (VAOCacheEntry::lastBase, m_lastVAOBase below) since a VAO's
	// *pointers* do encode it and must stay current -- see
	// enableAttribs()/setAttribPointers() in the .cpp for the split this
	// enables: attribute enable/disable state only needs setting once per
	// VAO (a fresh one starts fully disabled), pointers get reissued
	// whenever `base` changes for an otherwise-identical, already-cached
	// VAO -- a handful of glVertexAttribPointer calls, not a new GL object
	// plus the full disable/enable/pointer dance every time.
	struct VAOKey {
		GLuint vbo = 0;
		GLuint ibo = 0; // 0 for the non-indexed draw() path
		unsigned fvf = 0;
		unsigned stride = 0;

		bool operator==(const VAOKey &o) const {
			return memcmp(this, &o, sizeof(VAOKey)) == 0;
		}
	};
	// Keyed by a byte-wise FNV-1a hash of VAOKey (hashVAOKey() below), same
	// collision-tolerant style as computeProgramKey()'s FNV-1a program key
	// -- no verification against a stored raw key on lookup, matching that
	// existing precedent in this file. Lookup cost doesn't grow with how
	// much is cached, so kMaxVAOs below is a sanity backstop against
	// pathological growth (now bounded by unique (vbo,ibo,fvf,stride)
	// combos rather than every base-vertex value ever seen -- expected to
	// be a much smaller, stable number once a level's content has been
	// drawn once), not a real expected ceiling.
	static const size_t kMaxVAOs = 16384;
	struct VAOCacheEntry {
		VAOKey key; // kept only so evictVAOsForBuffer() can scan for vbo/ibo matches
		GLuint vao;
		int lastBase; // base-vertex offset currently baked into this VAO's attribute pointers
	};
	std::unordered_map<uint64_t, VAOCacheEntry> m_vaoCache;
	bool m_haveLastVAOKey = false;
	VAOKey m_lastVAOKey{};
	int m_lastVAOBase = 0;
	int m_perfVAOCacheHits = 0;
	int m_perfVAOCacheMisses = 0;
	// Of the hits above, how many still needed a base-vertex pointer
	// refresh (see bindVertexLayout()'s comment) -- the residual per-draw
	// cost a dynamic/shared vertex-buffer pool leaves behind even once its
	// VAO object itself is being fully reused.
	int m_perfVAOPointerRefresh = 0;
	// A free function couldn't name VAOKey (private nested type); a static
	// member can, same as any other member.
	static uint64_t hashVAOKey(const VAOKey &k);
	void bindVertexLayout(const FVFLayout &l, GLuint vbo, GLuint ibo, unsigned fvf, unsigned stride, int base);
	// A VBO/IBO's GL name can be recycled by the driver after deletion (same
	// hazard as invalidateTextureBinding/invalidateBufferBinding above); a
	// cached VAO keyed on that name would otherwise wrongly match whatever
	// unrelated buffer gets the reused name next. Called from
	// invalidateBufferBinding(), not directly -- deleting a VB/IB always
	// means "forget every GL-side cache entry that named this buffer."
	void evictVAOsForBuffer(GLuint name);

	// GeneralsX @build Android port GLES experiment - perf pass. The ported
	// pipeline was correctness-first: every draw re-applied all fixed GL
	// state and re-uploaded every uniform unconditionally (see the original
	// header comment above), which is fine for a browser tech demo but is
	// real, measurable per-draw driver overhead for an RTS scene with many
	// draws per frame. FixedStateKey mirrors every D3D render-state value
	// applyFixedState() reads (plus the viewport); when consecutive draws
	// share the same key, the whole function body -- a dozen-plus
	// glEnable/glDisable/glBlendFunc/... calls -- is skipped entirely.
	struct FixedStateKey {
		DWORD zEnable, zWrite, zFunc, zBias;
		DWORD alphaBlend, srcBlend, destBlend;
		DWORD cullMode, colorWrite;
		DWORD stencilEnable, stencilFunc, stencilRef, stencilMask;
		DWORD stencilFail, stencilZFail, stencilPass, stencilWriteMask;
		int vpX, vpY, vpW, vpH;
		float vpMinZ, vpMaxZ;

		bool operator==(const FixedStateKey &o) const {
			return memcmp(this, &o, sizeof(FixedStateKey)) == 0;
		}
	};
	bool m_haveFixedStateKey = false;
	FixedStateKey m_lastFixedStateKey{};
	GLuint m_lastProgram = 0;
	int m_perfStateCacheHits = 0;
	int m_perfStateCacheMisses = 0;

	// GeneralsX @build Android port ANGLE experiment - a program-cache miss in
	// getProgram() means an actual glCreateProgram/glLinkProgram (this module
	// caches by full render-state key, so a new key = a genuinely new shader
	// variant). Under ANGLE's Vulkan backend this can mean synchronous
	// VkPipeline creation, which is far pricier than the equivalent state
	// change on a native GLES driver -- suspected cause of the multi-second
	// freeze reported when opening menus with many distinct widget/state
	// combinations (e.g. SkirmishGameOptionsMenu) for the first time. Counts
	// and total time are folded into the existing "[d3d8gles] perf: ..." line
	// in present() to confirm/refute this without guessing further.
	int m_perfProgramBuilds = 0;
	double m_perfProgramBuildUs = 0.0;

	// GeneralsX @build Android port GLES experiment - perf pass. applyUniforms()
	// used to re-upload every uniform on every single draw, even on an
	// m_lastProgram cache hit -- ~15-25 glUniform* calls per draw that are
	// pure waste whenever the values feeding them didn't change since the
	// last draw. Unlike FixedStateKey above, this can't be one monolithic
	// key: uWorld changes on nearly every draw in real battlefield rendering
	// (each object has its own transform), so a single all-or-nothing key
	// would rarely hit. Split into independently-cached sub-blocks instead,
	// matching applyUniforms()'s own existing comment that each uniform can
	// be optimized out independently. uWorld itself is deliberately NOT
	// cached (see above) and is always uploaded directly.
	//
	// Every legacy global sub-key is invalidated when the program changes --
	// uniform locations are per-program, so a hit against a key computed for
	// a DIFFERENT program would wrongly skip uploading to this one. That
	// invalidation happens in the one place program switches are already
	// detected, at the top of applyUniforms() (see m_lastProgram above).
	// XR instead retains these keys per ProgramInfo (see XRUniformCache),
	// so switching programs alone does not discard their resident values.
	// GeneralsX @build Android port GLES experiment 08/30/2026 Split from one
	// combined "TransformKey" (view+proj+texMat0+texMat1 as a single cache
	// slot) after a real device log's new per-block uniform-cache breakdown
	// showed the transform bucket collapsing to ~35% hit rate mid-battle,
	// far below vao-cache's 99.8%+ -- suspicious, since the camera
	// (view/proj) is set once per frame and every draw that frame should
	// share it. The likely culprit: per-object texture-stage transforms
	// (UV scroll/glow animations on individual units) invalidating the
	// WHOLE combined key on every draw that uses them, forcing a spurious
	// view/proj re-upload too even though the camera hadn't changed at all.
	// Splitting into two independent caches lets view/proj stay cached
	// across a whole frame regardless of what any one object's texMat is
	// doing -- can only raise the hit rate, never lower it, since it's the
	// same comparisons just no longer coupled together.
	struct ViewProjKey {
		float view[16], proj[16];
		bool operator==(const ViewProjKey &o) const {
			return memcmp(this, &o, sizeof(ViewProjKey)) == 0;
		}
	};
	bool m_haveViewProjKey = false;
	ViewProjKey m_lastViewProjKey{};

	struct TexMatKey {
		float texMat0[16], texMat1[16];
		bool operator==(const TexMatKey &o) const {
			return memcmp(this, &o, sizeof(TexMatKey)) == 0;
		}
	};
	bool m_haveTexMatKey = false;
	TexMatKey m_lastTexMatKey{};

	struct MiscUniformKey { // viewport, yFlip, texture-factor, alpha ref, fog
		float vpX, vpY, vpW, vpH;
		float yFlip;
		float tFactor[4];
		float alphaRef;
		float fogColor[4];
		float fogStart, fogEnd;
		bool operator==(const MiscUniformKey &o) const {
			return memcmp(this, &o, sizeof(MiscUniformKey)) == 0;
		}
	};
	bool m_haveMiscKey = false;
	MiscUniformKey m_lastMiscKey{};

	struct MaterialKey { // VertexMaterialClass diffuse/ambient/emissive
		float diffuse[4], ambient[4], emissive[4];
		bool operator==(const MaterialKey &o) const {
			return memcmp(this, &o, sizeof(MaterialKey)) == 0;
		}
	};
	bool m_haveMaterialKey = false;
	MaterialKey m_lastMaterialKey{};

	struct LightingKey { // global ambient + up to 4 active lights
		float globalAmbient[4];
		int numLights;
		int types[4];
		float dirs[12], poss[12], diff[16], amb[16], att[16];
		bool operator==(const LightingKey &o) const {
			return memcmp(this, &o, sizeof(LightingKey)) == 0;
		}
	};
	bool m_haveLightingKey = false;
	LightingKey m_lastLightingKey{};
	// GeneralsX @performance Codex 14/09/2026 XR ordinary/multiview programs
	// retain their own uniform values across switches. Explicit GL ownership
	// handoff invalidates them lazily; the shared view/projection UBO is separate.
	struct XRUniformCache {
		uint64_t epoch=0;
		bool haveTexMatKey=false,haveMiscKey=false,haveMaterialKey=false,haveLightingKey=false;
		TexMatKey lastTexMatKey{};
		MiscUniformKey lastMiscKey{};
		MaterialKey lastMaterialKey{};
		LightingKey lastLightingKey{};
	};
	uint64_t m_xrUniformEpoch=1;
	// One combined hit/miss counter across all four sub-blocks above, logged
	// by present() the same way m_perfStateCacheHits/Misses is -- a per-block
	// breakdown would be more precise but four more numbers in an
	// already-dense perf line is not worth it for what is fundamentally one
	// question: "is the uniform cache doing anything."
	int m_perfUniformCacheHits = 0;
	int m_perfUniformCacheMisses = 0;
	// GeneralsX @build Android port GLES experiment 08/30/2026 The combined
	// counters above answer "is the cache doing anything" but not "which of
	// the four blocks is dragging the average down" -- real device logs
	// (Redmi Note 8 Pro) showed the combined rate stuck around 74% while
	// vao-cache sits at 99.8%+, and there's no way to tell from that one
	// number whether the miss-heavy block is one that's genuinely
	// per-object-variant (material/lighting -- expected to miss a lot in a
	// battle with many differently-colored/lit units) or one that SHOULD be
	// near-constant within a frame (transform/misc -- a real inefficiency if
	// it's missing a lot). Split out so the next log settles which.
	int m_perfUniformViewProjHits = 0, m_perfUniformViewProjMisses = 0;
	int m_perfUniformTexMatHits = 0, m_perfUniformTexMatMisses = 0;
	int m_perfUniformMiscHits = 0, m_perfUniformMiscMisses = 0;
	int m_perfUniformMaterialHits = 0, m_perfUniformMaterialMisses = 0;
	int m_perfUniformLightingHits = 0, m_perfUniformLightingMisses = 0;

	// GeneralsX @build Android port GLES experiment - same redundant-state
	// rationale as m_lastProgram above, applied to bindTextures()'s two
	// texture stages: the D3D-era engine calls SetTexture() before every
	// draw regardless of whether the stage's texture actually changed (the
	// same "reapply unconditionally" style that motivated FixedStateKey and
	// m_lastProgram), so consecutive draws sharing a material/texture -- a
	// terrain tile batch, a run of UI glyphs off the same font sheet, a
	// string of units using the same skin -- previously re-issued
	// glBindTexture per stage per draw for no reason. 0 doubles as "no GL
	// texture bound" for both an unset stage and an explicitly-unbound one,
	// which is fine since both cases want the same skip-if-unchanged
	// behavior. Only the bind itself is skipped; glActiveTexture still runs
	// unconditionally so applySamplerState (called right after) always has
	// the correct unit current if it needs to touch sampler parameters.
	GLuint m_lastBoundTex[2] = {0, 0};
	WebGLTexture *m_shroudTex=nullptr; // AddRef'd while published
	float m_shroudST[4]={0,0,0,0};      // scale xy, offset xy
	bool m_shroudFoldActive=false,m_shroudFoldDisabled=false;
	unsigned m_shroudMarkerPoll=0;
	GLuint m_shroudSampler=0,m_lastShroudBound=~0u;
	bool shroudFoldFor(bool xyzrhw) const {return m_xrMode && m_shroudFoldActive && m_shroudTex && !xyzrhw;}
	void bindShroud(ProgramInfo *prog);

	// Perf counters logged once every couple of seconds by present(), not
	// per frame -- draws/frame and cache hit rate are the numbers that
	// actually say whether the state-cache above is doing anything, instead
	// of guessing from feel alone.
	int m_perfDrawsThisFrame = 0;
	int m_perfFrameCount = 0;
	int m_perfDrawAccum = 0;
	unsigned m_perfLogLastMs = 0;
	void bindTextures(WebGLDevice *dev, ProgramInfo *prog);
	void uploadTexture(WebGLTexture *tex);
	void applySamplerState(WebGLDevice *dev, unsigned stage, WebGLTexture *tex);

	uint64_t computeProgramKey(WebGLDevice *dev, unsigned fvf) const;

	bool m_ctxReady = false;
	bool m_hasS3TC = false;
	int m_fbWidth = 0;
	int m_fbHeight = 0;
	SDL_Window *m_window = nullptr;
	void *m_glContext = nullptr; // really an SDL_GLContext, see the comment above

	// Current render target (FBO rendering for SetRenderTarget).
	GLuint m_curFBO = 0;
	int m_curRTWidth = 0;
	int m_curRTHeight = 0;
	float m_yFlip = 1.0f; // +1 backbuffer (flip), -1 FBO (no flip)
	GLuint m_depthRB = 0; // shared depth-stencil renderbuffer for FBOs
	int m_depthRBW = 0, m_depthRBH = 0;

	// XR spike (Phase 0.2) offscreen capture state. Rendering is untouched;
	// present() just copies the finished frame into m_offFBO (snoop).
	bool m_offscreenCapture = false;
	// XR port (Phase 1.0) external-context mode: no window, default-target
	// rendering redirects into m_offFBO, present() never swaps or snoops.
	bool m_xrMode = false;
	bool m_xrSplitRequested=false, m_xrUI=false, m_xrSplitReady=false;
	bool m_xrWorldSnapshotValid=false;
	GXWorldElision m_xrElision;
	GLuint m_xrWorldFBO=0, m_xrWorldTex=0, m_xrUITex=0;
	bool m_xrStereoActive=false,m_xrStereoReady=false;
	bool m_xrStereoSeen=false,m_xrStereoCoverage=false;
	bool m_xrStereoAtlas=false,m_xrAtlasRequested=false;
	bool m_xrStereoMultiview=false,m_xrMultiviewRequested=false,m_xrMultiviewFailed=false;
	GXMultiview m_xrMultiview;
	GLuint m_xrMultiviewFBO=0,m_xrMultiviewDepth=0;
	unsigned m_xrStereoProbeWait=0;
	GLuint m_xrStereoFBO[2]={},m_xrStereoTex[2]={},m_xrStereoDepth[2]={};
	GLuint m_xrDecorProgram=0,m_xrDecorVAO=0,m_xrDecorVBO=0;
	int m_xrStereoW=0,m_xrStereoH=0;
	unsigned m_xrStereoDraws=0;
	// GeneralsX @performance Codex 14/09/2026 Bounded restore-cost counters.
	unsigned m_xrRestores=0,m_xrRestoreCalls=0;
	unsigned m_xrTerrainDraws=0,m_xrModelDraws=0;
	float m_xrEyeClip[2][16]={},m_xrBoard[16]={},m_xrStereoAspect=.45f;
	float m_xrCamera[16]={};
	unsigned m_xrEffectDraws=0,m_xrShadowDraws=0;
	void destroyXRStereo();
	void destroyXRLayers();
	// GeneralsX @feature Quest tabletop recovery 13/09/2026 Diagnostic frame
	// readback is opt-in. glReadPixels stalls the shared render/XR thread, so
	// normal headset sessions must never capture periodically by accident.
	bool m_xrCaptureEnabled = false;
	void captureOffscreenPPM(); // opt-in diagnostic readback of m_offFBO
	GLuint m_offFBO = 0;
	GLuint m_offColorTex = 0;
	GLuint m_offDepthRB = 0;
	int m_offW = 0, m_offH = 0;
	unsigned m_offBindMismatch = 0; // present-time GL binding != m_curFBO fiction (diagnostic)
	bool ensureOffscreenTarget(); // (re)creates at m_fbWidth/Height, binds, false on failure

	// GeneralsX @build Android port GLES experiment 08/30/2026 Camera
	// (view+proj) uniform buffer -- see kViewProjUBOBinding's comment in
	// gles_pipeline.cpp for why this exists instead of plain glUniform*
	// calls. Created once in initContext(), bound to kViewProjUBOBinding
	// for the whole session; only its DATA changes, via applyUniforms()'s
	// glBufferSubData when the CPU-side ViewProjKey actually differs from
	// what's currently uploaded.
	GLuint m_viewProjUBO = 0;

	// Streaming buffers for the UP draw paths.
	// Scratch for readbackRenderTarget(); a member so the 1 MB staging buffer
	// is allocated once instead of per shadow update.
	std::vector<uint8_t> m_rtReadback;
	GLuint m_upVBO = 0;
	GLuint m_upIBO = 0;

	// Program cache: key -> program.
	static const int kMaxPrograms = 512;
	struct CacheEntry {
		uint64_t key;
		bool multiview=false;
		ProgramInfo *prog;
	};
	CacheEntry m_programs[kMaxPrograms];
	int m_programCount = 0;

	unsigned m_frame = 0;
};
