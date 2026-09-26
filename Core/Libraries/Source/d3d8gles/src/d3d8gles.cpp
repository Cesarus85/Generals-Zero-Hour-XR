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
** d3d8gles.cpp - native D3D8 -> GLES3 implementation for Android.
**
** GeneralsX @build Android port GLES experiment - ported from
** Lolendor/Generals-WebAssembly's d3d8webgl.cpp (D3D8 -> WebGL2 for
** Emscripten). The D3D8 interface emulation below is unchanged from that
** port; only gles_pipeline.cpp's context/present calls were adapted from
** Emscripten's WebGL2 canvas API to SDL3's native GLES3 context API (see
** WebGLPipeline::initContext/present there for the actual platform diff).
**
** Reported caps are a fixed-function DX8 part (VertexShaderVersion = 0,
** PixelShaderVersion = 0, 2 texture stages) so the engine selects the shipped
** fixed-function fallback paths - the ones gles_pipeline.cpp implements.
**
** Set the D3D8GLES_TRACE env var to log unimplemented/interesting calls.
*/

#include <d3d8.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <map>
#include <vector>

#if defined(__ANDROID__)
#include <SDL3/SDL.h>
#endif

#include "gles_pipeline.h"

// GeneralsX @feature XR port Phase 1.0 - this TU never included its own
// public header (definitions don't need declarations); the XR backend
// forcing below *calls* d3d8gles_GetXRConfig(), so it does now.
#include "d3d8gles.h"

#if defined(__ANDROID__)
// GeneralsX @build Android port render-backend picker 07/09/2026 - see
// d3d8gles.h's comment on d3d8gles_ShouldUseVulkanBackend()/
// d3d8gles_ShouldUseANGLE() for why this lives here instead of being
// duplicated per-caller. Reads <internalPath>/render_backend.cfg (written by
// SetupActivity.java's "Render Backend" picker; one line: "vulkan", "gles",
// or "gles_angle"), falling back to the GENERALSX_RENDER_BACKEND/
// GENERALSX_GLES_ANGLE env vars this experiment used before that picker
// existed (kept working for manual/dev testing without a rebuild). Each
// value is read once and cached -- Setup only ever writes this file before
// the game process starts, never while it's running.
static const char *ReadRenderBackendConfigFile()
{
	static bool s_read = false;
	static char s_value[32] = {0};
	if (!s_read) {
		s_read = true;
		const char *internalPath = SDL_GetAndroidInternalStoragePath();
		if (internalPath != nullptr) {
			char cfgPath[1024];
			snprintf(cfgPath, sizeof(cfgPath), "%s/render_backend.cfg", internalPath);
			FILE *cfg = fopen(cfgPath, "r");
			if (cfg != nullptr) {
				if (fgets(s_value, sizeof(s_value), cfg) != nullptr) {
					size_t len = strlen(s_value);
					while (len > 0 && (s_value[len - 1] == '\n' || s_value[len - 1] == '\r')) {
						s_value[--len] = '\0';
					}
				}
				fclose(cfg);
			}
		}
		// GeneralsX @bugfix Android port 08/30/2026 Cheap, permanent
		// diagnostic: which render backend actually got resolved, and from
		// where. Added while chasing a vertical-flip report where the
		// higher-level "Vulkan vs GLES" log line (SDL3Main.cpp) didn't
		// distinguish ANGLE from the system driver, leaving no way to tell
		// from a device log whether ANGLE came from an explicit choice or
		// this file being missing/unreadable.
		fprintf(stderr, "[d3d8gles] render_backend.cfg resolved to: %s\n",
			s_value[0] != '\0' ? s_value : "(unset -- falling back to env vars/defaults)");
	}
	return s_value[0] != '\0' ? s_value : nullptr;
}

extern "C" bool d3d8gles_ShouldUseVulkanBackend()
{
	// GeneralsX @feature XR port Phase 1.0 - the XR boot has no window
	// surface (DXVK could present nothing into it) and the game frame must
	// come out of the d3d8gles owned FBO, so the DXVK path is never taken
	// in external-context mode regardless of render_backend.cfg.
	if (d3d8gles_GetXRConfig() != nullptr) {
		return false;
	}
	const char *configured = ReadRenderBackendConfigFile();
	if (configured != nullptr) {
		return strcmp(configured, "vulkan") == 0;
	}
	const char *backend = getenv("GENERALSX_RENDER_BACKEND");
	return backend != nullptr && strcmp(backend, "vulkan") == 0;
}

extern "C" bool d3d8gles_ShouldUseANGLE()
{
	// GeneralsX @feature XR port Phase 1.0 - same mode, same reason as
	// above: ANGLE entry points against the XR loop's system-EGL context
	// would be garbage (initContextExternal() forces system dispatch).
	if (d3d8gles_GetXRConfig() != nullptr) {
		return false;
	}
	const char *configured = ReadRenderBackendConfigFile();
	if (configured != nullptr) {
		return strcmp(configured, "gles_angle") == 0;
	}
	// GeneralsX @bugfix Android port 08/30/2026 This used to default to
	// TRUE (ANGLE on) whenever render_backend.cfg was missing/unreadable --
	// inconsistent with d3d8gles_ShouldUseVulkanBackend() just above, which
	// conservatively defaults to FALSE in the same situation. A real device
	// report (Redmi Note 8 Pro) showed the whole-frame vertical-flip bug
	// bd2a2379 fixed and verified back on 08/07 -- against the system GLES
	// driver, before ANGLE existed in this codebase at all -- reappearing;
	// the device's log confirmed ANGLE ("[d3d8gles] GLES backend:
	// libGLESv2_angle.so") was active. Whether that came from an explicit
	// "gles_angle" choice or this exact silent fallback, defaulting an
	// unconfigured install into an experimental Vulkan-translation layer
	// nobody opted into defeats the whole point of the picker being
	// explicit. Default OFF like the Vulkan check now.
	const char *v = getenv("GENERALSX_GLES_ANGLE");
	return v != nullptr && strcmp(v, "0") != 0;
}
#endif // __ANDROID__

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static bool g_trace = false;

// GeneralsX @build Android port GLES experiment - direct, unambiguous
// evidence for the texture-leak fix instead of inferring it indirectly from
// whether icons still flicker. A bare live-count number only says *whether*
// something leaks, not *what* -- there is exactly one C++ call site that
// constructs a WebGLTexture (WebGLDevice::CreateTexture below) and exactly
// one that destroys one (~WebGLTexture), so "where in the code" is not
// ambiguous; what's actually unknown is *which* textures -- by size/format,
// i.e. which category of asset -- are the ones failing to reach zero
// refcount. g_liveTexturesByShape buckets every currently-live WebGLTexture
// by (width, height, format) and is logged periodically (present()'s
// existing perf line, via DumpLiveTextureShapes) as a sorted top-N. If the
// texture-leak fix (glDeleteTextures now called in ~WebGLTexture) actually
// closed the icon flicker, small icon-shaped buckets (64x64/128x128,
// fmt=26=D3DFMT_A4R4G4B4) should plateau instead of climbing across
// successive log lines; if some other bucket climbs unbounded instead, that
// shape identifies exactly which asset class still leaks.
struct LiveTextureShapeKey {
	UINT w = 0, h = 0;
	D3DFORMAT fmt = D3DFMT_UNKNOWN;
	bool operator<(const LiveTextureShapeKey &o) const
	{
		if (w != o.w) return w < o.w;
		if (h != o.h) return h < o.h;
		return fmt < o.fmt;
	}
};
static std::map<LiveTextureShapeKey, int> g_liveTexturesByShape;
static long g_texturesCreated = 0;
static long g_texturesDeleted = 0;

static void DumpLiveTextureShapes()
{
	std::vector<std::pair<LiveTextureShapeKey, int>> shapes(
		g_liveTexturesByShape.begin(), g_liveTexturesByShape.end());
	std::sort(shapes.begin(), shapes.end(),
		[](const auto &a, const auto &b) { return a.second > b.second; });
	fprintf(stderr, "[d3d8gles] live texture shapes (top %zu of %zu distinct):",
		std::min<size_t>(shapes.size(), 8), shapes.size());
	for (size_t i = 0; i < shapes.size() && i < 8; i++) {
		fprintf(stderr, " %ux%u,fmt%d=%d", shapes[i].first.w, shapes[i].first.h,
			(int)shapes[i].first.fmt, shapes[i].second);
	}
	fprintf(stderr, "\n");
}

#define D3D8GLES_TRACE_CALL(...)                 \
	do {                                          \
		if (g_trace) {                            \
			fprintf(stderr, "[d3d8gles] ");      \
			fprintf(stderr, __VA_ARGS__);         \
			fprintf(stderr, "\n");                \
		}                                         \
	} while (0)

// Bytes per pixel (or per 4x4 block for DXT) for the formats the engine uses.
static UINT FormatBytesPerPixel(D3DFORMAT fmt)
{
	switch (fmt) {
	case D3DFMT_A8R8G8B8:
	case D3DFMT_X8R8G8B8:
	case D3DFMT_D24S8:
	case D3DFMT_D24X8:
	case D3DFMT_D32:
	case D3DFMT_X8L8V8U8:
	case D3DFMT_Q8W8V8U8:
	case D3DFMT_V16U16:
	case D3DFMT_A2B10G10R10:
	case D3DFMT_G16R16:
		return 4;
	case D3DFMT_R8G8B8:
		return 3;
	case D3DFMT_R5G6B5:
	case D3DFMT_X1R5G5B5:
	case D3DFMT_A1R5G5B5:
	case D3DFMT_A4R4G4B4:
	case D3DFMT_X4R4G4B4:
	case D3DFMT_A8L8:
	case D3DFMT_V8U8:
	case D3DFMT_L6V5U5:
	case D3DFMT_D16:
	case D3DFMT_D16_LOCKABLE:
	case D3DFMT_D15S1:
	case D3DFMT_A8P8:
		return 2;
	case D3DFMT_A8:
	case D3DFMT_L8:
	case D3DFMT_P8:
	case D3DFMT_A4L4:
		return 1;
	default:
		return 4;
	}
}

static bool FormatIsDXT(D3DFORMAT fmt)
{
	return fmt == D3DFMT_DXT1 || fmt == D3DFMT_DXT2 || fmt == D3DFMT_DXT3 ||
	       fmt == D3DFMT_DXT4 || fmt == D3DFMT_DXT5;
}

static UINT FormatDXTBlockBytes(D3DFORMAT fmt)
{
	return (fmt == D3DFMT_DXT1) ? 8 : 16;
}

// Row pitch + total byte size of one surface level in the given format.
static void FormatLevelLayout(D3DFORMAT fmt, UINT width, UINT height, INT *pitch, UINT *size)
{
	if (FormatIsDXT(fmt)) {
		const UINT blocksWide = (width + 3) / 4;
		const UINT blocksHigh = (height + 3) / 4;
		*pitch = (INT)(blocksWide * FormatDXTBlockBytes(fmt));
		*size = blocksWide * blocksHigh * FormatDXTBlockBytes(fmt);
	} else {
		*pitch = (INT)(width * FormatBytesPerPixel(fmt));
		*size = (UINT)(*pitch) * height;
	}
}

// Common IUnknown boilerplate: identity QueryInterface, plain ref counting,
// delete on last Release. The engine is single-threaded through this API.
#define D3D8GLES_IUNKNOWN_IMPL(klass)                                          \
	HRESULT QueryInterface(REFIID /*riid*/, void **ppvObject) override          \
	{                                                                           \
		if (ppvObject == nullptr) return E_POINTER;                             \
		*ppvObject = this;                                                      \
		AddRef();                                                               \
		return S_OK;                                                            \
	}                                                                           \
	ULONG AddRef() override { return ++m_ref; }                                 \
	ULONG Release() override                                                    \
	{                                                                           \
		const ULONG r = --m_ref;                                                \
		if (r == 0) delete this;                                                \
		return r;                                                               \
	}                                                                           \
	ULONG m_ref = 1;

// Resource-private-data stubs shared by all resource types.
#define D3D8GLES_RESOURCE_STUBS(devfield)                                            \
	HRESULT GetDevice(struct IDirect3DDevice8 **ppDevice) override;                   \
	HRESULT SetPrivateData(REFGUID, const void *, DWORD, DWORD) override { return D3D_OK; } \
	HRESULT GetPrivateData(REFGUID, void *, DWORD *) override { return D3DERR_NOTFOUND; }   \
	HRESULT FreePrivateData(REFGUID) override { return D3D_OK; }

class WebGLDevice; // fwd
class WebGLTexture; // fwd

// ---------------------------------------------------------------------------
// Surface: system-memory image, texture level view, or render target.
// Owns its bits unless it aliases a texture level (then 'owner' holds them).
// ---------------------------------------------------------------------------

class WebGLSurface final : public IDirect3DSurface8
{
public:
	WebGLSurface(WebGLDevice *dev, UINT w, UINT h, D3DFORMAT fmt, D3DRESOURCETYPE containerType)
		: m_device(dev), m_width(w), m_height(h), m_format(fmt), m_containerType(containerType)
	{
		FormatLevelLayout(fmt, w, h, &m_pitch, &m_size);
		m_bits.resize(m_size);
	}

	// Manual IUnknown: texture-level views are privately owned by their
	// texture (deleted in its dtor), NOT by the public refcount. Game-era
	// D3DX code over-releases level surfaces (see D3DXFilterTexture); real
	// runtimes and DXVK tolerate that, so this layer must too.
	HRESULT QueryInterface(REFIID /*riid*/, void **ppvObject) override
	{
		if (ppvObject == nullptr) return E_POINTER;
		*ppvObject = this;
		AddRef();
		return S_OK;
	}
	ULONG AddRef() override { return ++m_ref; }
	ULONG Release() override
	{
		const ULONG r = --m_ref;
		if (r == 0 && !m_textureOwned) delete this;
		return r;
	}
	ULONG m_ref = 1;
	bool m_textureOwned = false; // lifetime bound to the owning texture

	HRESULT GetDevice(struct IDirect3DDevice8 **ppDevice) override;
	HRESULT SetPrivateData(REFGUID, const void *, DWORD, DWORD) override { return D3D_OK; }
	HRESULT GetPrivateData(REFGUID, void *, DWORD *) override { return D3DERR_NOTFOUND; }
	HRESULT FreePrivateData(REFGUID) override { return D3D_OK; }

	HRESULT GetContainer(REFIID, void **ppContainer) override
	{
		if (ppContainer) *ppContainer = nullptr;
		return E_NOINTERFACE;
	}

	HRESULT GetDesc(D3DSURFACE_DESC *pDesc) override
	{
		if (!pDesc) return D3DERR_INVALIDCALL;
		memset(pDesc, 0, sizeof(*pDesc));
		pDesc->Format = m_format;
		pDesc->Type = D3DRTYPE_SURFACE;
		pDesc->Usage = 0;
		pDesc->Pool = D3DPOOL_SYSTEMMEM;
		pDesc->Size = m_size;
		pDesc->MultiSampleType = D3DMULTISAMPLE_NONE;
		pDesc->Width = m_width;
		pDesc->Height = m_height;
		return D3D_OK;
	}

	HRESULT LockRect(D3DLOCKED_RECT *locked_rect, const RECT *rect, DWORD /*flags*/) override
	{
		if (!locked_rect) return D3DERR_INVALIDCALL;
		if (!m_alive) {
			fprintf(stderr, "[d3d8gles] WARNING: LockRect on retired surface %p "
				"(%ux%u, fmt%d) -- owning texture already destroyed, use-after-free avoided\n",
				(void *)this, m_width, m_height, (int)m_format);
			return D3DERR_INVALIDCALL;
		}
		locked_rect->Pitch = m_pitch;
		BYTE *base = m_bits.data();
		if (rect) {
			const UINT bpp = FormatIsDXT(m_format) ? 0 : FormatBytesPerPixel(m_format);
			if (FormatIsDXT(m_format)) {
				base += (rect->top / 4) * m_pitch + (rect->left / 4) * FormatDXTBlockBytes(m_format);
			} else {
				base += rect->top * m_pitch + rect->left * bpp;
			}
		}
		locked_rect->pBits = base;
		return D3D_OK;
	}

	HRESULT UnlockRect() override
	{
		if (!m_alive) {
			fprintf(stderr, "[d3d8gles] WARNING: UnlockRect on retired surface %p "
				"(%ux%u, fmt%d) -- owning texture already destroyed, use-after-free avoided\n",
				(void *)this, m_width, m_height, (int)m_format);
			return D3DERR_INVALIDCALL;
		}
		if (m_ownerGL) m_ownerGL->dirty = true;
		return D3D_OK;
	}

	GLTextureState *m_ownerGL = nullptr; // owning texture's GL state (level surfaces)
	WebGLTexture *m_ownerTex = nullptr;  // owning texture (for render-target FBOs)
	WebGLDevice *m_device;
	UINT m_width;
	UINT m_height;
	D3DFORMAT m_format;
	D3DRESOURCETYPE m_containerType;
	INT m_pitch = 0;
	UINT m_size = 0;
	std::vector<BYTE> m_bits;

	// GeneralsX @build Android port GLES experiment - use-after-free
	// detection for the residual icon-flicker investigation (see
	// RetireSurface below). True for the surface's whole life until its
	// owning texture is destroyed; texture-owned levels are then quarantined
	// instead of deleted immediately so a stale IDirect3DSurface8* the
	// engine kept around (e.g. from an earlier GetSurfaceLevel) can be
	// caught here instead of silently reading/writing freed memory.
	bool m_alive = true;
};

// GeneralsX @build Android port GLES experiment - see WebGLSurface::m_alive
// above. Texture-owned level surfaces are documented (see the comment on
// WebGLSurface's manual IUnknown) as tolerating over-Release() from D3DX8-era
// engine code because their lifetime is bound to the owning WebGLTexture, not
// the public refcount. That means ~WebGLTexture() always deletes them
// outright regardless of whether some other code still holds a raw pointer
// to one -- which is fine *if* nothing does, but if something does (a cached
// "current cameo write target" surface pointer that outlives a texture
// recreation, say), that pointer instantly dangles. Deferring the real
// delete through a small quarantine keeps the memory (and therefore m_alive)
// valid for a while after "logical" destruction, so LockRect/UnlockRect/
// CopyRects can detect and refuse the stale access instead of corrupting the
// heap or reading garbage -- which is a very plausible match for icons that
// flicker rather than stay reliably broken or reliably fine.
static std::vector<WebGLSurface *> g_surfaceQuarantine;
static const size_t kSurfaceQuarantineMax = 64;

static void RetireSurface(WebGLSurface *s)
{
	s->m_alive = false;
	g_surfaceQuarantine.push_back(s);
	if (g_surfaceQuarantine.size() > kSurfaceQuarantineMax) {
		delete g_surfaceQuarantine.front();
		g_surfaceQuarantine.erase(g_surfaceQuarantine.begin());
	}
}

// ---------------------------------------------------------------------------
// Texture (2D). Levels are WebGLSurface objects.
// ---------------------------------------------------------------------------

class WebGLTexture final : public IDirect3DTexture8
{
public:
	WebGLTexture(WebGLDevice *dev, UINT w, UINT h, UINT levels, D3DFORMAT fmt, D3DPOOL pool)
		: m_device(dev), m_format(fmt), m_pool(pool)
	{
		if (levels == 0) {
			levels = 1;
			UINT tw = w, th = h;
			while (tw > 1 || th > 1) {
				tw = tw > 1 ? tw / 2 : 1;
				th = th > 1 ? th / 2 : 1;
				levels++;
			}
		}
		UINT tw = w, th = h;
		for (UINT i = 0; i < levels; i++) {
			WebGLSurface *lvl = new WebGLSurface(dev, tw, th, fmt, D3DRTYPE_TEXTURE);
			lvl->m_ownerGL = &m_gl;
			lvl->m_ownerTex = this;
			lvl->m_textureOwned = true;
			m_levels.push_back(lvl);
			tw = tw > 1 ? tw / 2 : 1;
			th = th > 1 ? th / 2 : 1;
		}
		m_shapeKey = LiveTextureShapeKey{w, h, fmt};
		g_liveTexturesByShape[m_shapeKey]++;
	}

	~WebGLTexture()
	{
		{
			auto it = g_liveTexturesByShape.find(m_shapeKey);
			if (it != g_liveTexturesByShape.end() && --it->second <= 0) {
				g_liveTexturesByShape.erase(it);
			}
		}
		// GeneralsX @build Android port GLES experiment - this destructor
		// used to free only the CPU-side shadow surfaces, never the actual
		// GL texture object (m_gl.name) or its FBO (m_gl.fbo, if this
		// texture was ever used as a render target). Every D3D texture the
		// engine creates and releases -- confirmed via device logs to
		// include very frequently recreated small icon/cameo textures --
		// leaked its GL-side memory forever. Over a play session this grows
		// unbounded and is a strong suspect for icons flickering in and out
		// over time (confirmed via device screenshots a few seconds apart
		// showing the same icon slots alternate between correct and black)
		// as the driver runs low on texture memory. GL object deletion must
		// happen on the GL thread with a context current, which holds here
		// since resource release always happens from the game/render
		// thread in this port.
		if (m_gl.fbo) {
			glDeleteFramebuffers(1, &m_gl.fbo);
			m_gl.fbo = 0;
		}
		if (m_gl.name) {
			// GeneralsX @build Android port GLES experiment - part 2/2 of the
			// glBindTexture-skip cache correctness fix (see
			// WebGLPipeline::invalidateTextureBinding's declaration): GL
			// itself clears this name's binding on every texture unit as a
			// side effect of glDeleteTextures, but the cache doesn't observe
			// that, and the driver commonly recycles freed names -- without
			// this, a later unrelated texture reusing this exact name could
			// be wrongly treated as "already bound" by a stale cache entry.
			glDeleteTextures(1, &m_gl.name);
			WebGLPipeline::get()->invalidateTextureBinding(m_gl.name);
			m_gl.name = 0;
			g_texturesDeleted++;
		}
		for (WebGLSurface *s : m_levels) {
			RetireSurface(s); // quarantined, not deleted outright -- see RetireSurface's comment
		}
	}

	// GeneralsX @build Android port GLES experiment - manual IUnknown
	// instead of the shared D3D8GLES_IUNKNOWN_IMPL macro. The macro's
	// Release() trusts refcounting blindly (decrement, delete at 0); the
	// class comment on WebGLSurface above already documents that D3DX8-era
	// engine code over-releases surface *levels* (tolerated there by never
	// deleting texture-owned levels via the public refcount at all). If the
	// same class of over-release happens on the *texture* object itself --
	// plausible for cameo/icon textures given how often they're recreated,
	// per the live-shape perf log -- the generic macro would silently
	// double-delete/use-after-free instead of surfacing it, which fits
	// "icons flicker depending on screen position" better than a real
	// GL-side leak (already fixed and confirmed via the shape histogram to
	// plateau, not climb). This guards against ref hitting 0 twice and logs
	// it loudly instead of guessing further.
	HRESULT QueryInterface(REFIID /*riid*/, void **ppvObject) override
	{
		if (ppvObject == nullptr) return E_POINTER;
		*ppvObject = this;
		AddRef();
		return S_OK;
	}
	ULONG AddRef() override { return ++m_ref; }
	ULONG Release() override
	{
		if (m_ref == 0) {
			fprintf(stderr, "[d3d8gles] WARNING: WebGLTexture %p over-released "
				"(Release() called with ref already 0) shape=%ux%u fmt=%d -- "
				"ignoring to avoid a double-delete/use-after-free\n",
				(void *)this, m_shapeKey.w, m_shapeKey.h, (int)m_shapeKey.fmt);
			return 0;
		}
		const ULONG r = --m_ref;
		if (r == 0) delete this;
		return r;
	}
	ULONG m_ref = 1;

	HRESULT GetDevice(struct IDirect3DDevice8 **ppDevice) override;
	HRESULT SetPrivateData(REFGUID, const void *, DWORD, DWORD) override { return D3D_OK; }
	HRESULT GetPrivateData(REFGUID, void *, DWORD *) override { return D3DERR_NOTFOUND; }
	HRESULT FreePrivateData(REFGUID) override { return D3D_OK; }

	DWORD SetPriority(DWORD p) override { const DWORD old = m_priority; m_priority = p; return old; }
	DWORD GetPriority() override { return m_priority; }
	void PreLoad() override {}
	D3DRESOURCETYPE GetType() override { return D3DRTYPE_TEXTURE; }
	DWORD SetLOD(DWORD lod) override { const DWORD old = m_lod; m_lod = lod; return old; }
	DWORD GetLOD() override { return m_lod; }
	DWORD GetLevelCount() override { return (DWORD)m_levels.size(); }

	HRESULT GetLevelDesc(UINT level, D3DSURFACE_DESC *pDesc) override
	{
		if (level >= m_levels.size()) return D3DERR_INVALIDCALL;
		HRESULT hr = m_levels[level]->GetDesc(pDesc);
		if (hr == D3D_OK) {
			pDesc->Type = D3DRTYPE_TEXTURE;
			pDesc->Pool = m_pool;
		}
		return hr;
	}

	HRESULT GetSurfaceLevel(UINT level, IDirect3DSurface8 **ppSurfaceLevel) override
	{
		if (!ppSurfaceLevel || level >= m_levels.size()) return D3DERR_INVALIDCALL;
		m_levels[level]->AddRef();
		*ppSurfaceLevel = m_levels[level];
		return D3D_OK;
	}

	HRESULT LockRect(UINT level, D3DLOCKED_RECT *locked_rect, const RECT *rect, DWORD flags) override
	{
		if (level >= m_levels.size()) return D3DERR_INVALIDCALL;
		return m_levels[level]->LockRect(locked_rect, rect, flags);
	}

	HRESULT UnlockRect(UINT level) override
	{
		if (level >= m_levels.size()) return D3DERR_INVALIDCALL;
		m_gl.dirty = true;
		return m_levels[level]->UnlockRect();
	}

	HRESULT AddDirtyRect(const RECT *) override
	{
		m_gl.dirty = true;
		return D3D_OK;
	}

	WebGLDevice *m_device;
	D3DFORMAT m_format;
	D3DPOOL m_pool;
	DWORD m_priority = 0;
	DWORD m_lod = 0;
	GLTextureState m_gl;
	std::vector<WebGLSurface *> m_levels;
	LiveTextureShapeKey m_shapeKey;
};

// ---------------------------------------------------------------------------
// Cube texture. 6 faces x N levels of WebGLSurface.
// ---------------------------------------------------------------------------

class WebGLCubeTexture final : public IDirect3DCubeTexture8
{
public:
	WebGLCubeTexture(WebGLDevice *dev, UINT edge, UINT levels, D3DFORMAT fmt, D3DPOOL pool)
		: m_device(dev), m_format(fmt), m_pool(pool)
	{
		if (levels == 0) {
			levels = 1;
			UINT e = edge;
			while (e > 1) { e /= 2; levels++; }
		}
		m_levelCount = levels;
		for (UINT f = 0; f < 6; f++) {
			UINT e = edge;
			for (UINT i = 0; i < levels; i++) {
				WebGLSurface *s = new WebGLSurface(dev, e, e, fmt, D3DRTYPE_CUBETEXTURE);
				s->m_textureOwned = true;
				m_faces[f].push_back(s);
				e = e > 1 ? e / 2 : 1;
			}
		}
	}

	~WebGLCubeTexture()
	{
		for (auto &face : m_faces) {
			for (WebGLSurface *s : face) {
				delete s; // privately owned; public refcount does not delete these
			}
		}
	}

	D3D8GLES_IUNKNOWN_IMPL(WebGLCubeTexture)

	HRESULT GetDevice(struct IDirect3DDevice8 **ppDevice) override;
	HRESULT SetPrivateData(REFGUID, const void *, DWORD, DWORD) override { return D3D_OK; }
	HRESULT GetPrivateData(REFGUID, void *, DWORD *) override { return D3DERR_NOTFOUND; }
	HRESULT FreePrivateData(REFGUID) override { return D3D_OK; }

	DWORD SetPriority(DWORD p) override { const DWORD old = m_priority; m_priority = p; return old; }
	DWORD GetPriority() override { return m_priority; }
	void PreLoad() override {}
	D3DRESOURCETYPE GetType() override { return D3DRTYPE_CUBETEXTURE; }
	DWORD SetLOD(DWORD lod) override { const DWORD old = m_lod; m_lod = lod; return old; }
	DWORD GetLOD() override { return m_lod; }
	DWORD GetLevelCount() override { return m_levelCount; }

	HRESULT GetLevelDesc(UINT level, D3DSURFACE_DESC *pDesc) override
	{
		if (level >= m_levelCount) return D3DERR_INVALIDCALL;
		HRESULT hr = m_faces[0][level]->GetDesc(pDesc);
		if (hr == D3D_OK) {
			pDesc->Type = D3DRTYPE_CUBETEXTURE;
			pDesc->Pool = m_pool;
		}
		return hr;
	}

	HRESULT GetCubeMapSurface(D3DCUBEMAP_FACES face, UINT level, IDirect3DSurface8 **ppCubeMapSurface) override
	{
		if (!ppCubeMapSurface || face > D3DCUBEMAP_FACE_NEGATIVE_Z || level >= m_levelCount) {
			return D3DERR_INVALIDCALL;
		}
		m_faces[face][level]->AddRef();
		*ppCubeMapSurface = m_faces[face][level];
		return D3D_OK;
	}

	HRESULT LockRect(D3DCUBEMAP_FACES face, UINT level, D3DLOCKED_RECT *locked_rect,
	                 const RECT *rect, DWORD flags) override
	{
		if (face > D3DCUBEMAP_FACE_NEGATIVE_Z || level >= m_levelCount) return D3DERR_INVALIDCALL;
		return m_faces[face][level]->LockRect(locked_rect, rect, flags);
	}

	HRESULT UnlockRect(D3DCUBEMAP_FACES face, UINT level) override
	{
		if (face > D3DCUBEMAP_FACE_NEGATIVE_Z || level >= m_levelCount) return D3DERR_INVALIDCALL;
		return m_faces[face][level]->UnlockRect();
	}

	HRESULT AddDirtyRect(D3DCUBEMAP_FACES, const RECT *) override { return D3D_OK; }

	WebGLDevice *m_device;
	D3DFORMAT m_format;
	D3DPOOL m_pool;
	DWORD m_priority = 0;
	DWORD m_lod = 0;
	UINT m_levelCount = 0;
	std::vector<WebGLSurface *> m_faces[6];
};

// ---------------------------------------------------------------------------
// Volume texture. Minimal: one memory block per level.
// ---------------------------------------------------------------------------

class WebGLVolumeTexture final : public IDirect3DVolumeTexture8
{
public:
	struct Level {
		UINT w, h, d;
		INT rowPitch;
		INT slicePitch;
		std::vector<BYTE> bits;
	};

	WebGLVolumeTexture(WebGLDevice *dev, UINT w, UINT h, UINT d, UINT levels, D3DFORMAT fmt, D3DPOOL pool)
		: m_device(dev), m_format(fmt), m_pool(pool)
	{
		if (levels == 0) {
			levels = 1;
			UINT tw = w, th = h, td = d;
			while (tw > 1 || th > 1 || td > 1) {
				tw = tw > 1 ? tw / 2 : 1;
				th = th > 1 ? th / 2 : 1;
				td = td > 1 ? td / 2 : 1;
				levels++;
			}
		}
		UINT tw = w, th = h, td = d;
		for (UINT i = 0; i < levels; i++) {
			Level lvl;
			lvl.w = tw; lvl.h = th; lvl.d = td;
			UINT size2d;
			FormatLevelLayout(fmt, tw, th, &lvl.rowPitch, &size2d);
			lvl.slicePitch = (INT)size2d;
			lvl.bits.resize((size_t)size2d * td);
			m_levels.push_back(std::move(lvl));
			tw = tw > 1 ? tw / 2 : 1;
			th = th > 1 ? th / 2 : 1;
			td = td > 1 ? td / 2 : 1;
		}
	}

	D3D8GLES_IUNKNOWN_IMPL(WebGLVolumeTexture)

	HRESULT GetDevice(struct IDirect3DDevice8 **ppDevice) override;
	HRESULT SetPrivateData(REFGUID, const void *, DWORD, DWORD) override { return D3D_OK; }
	HRESULT GetPrivateData(REFGUID, void *, DWORD *) override { return D3DERR_NOTFOUND; }
	HRESULT FreePrivateData(REFGUID) override { return D3D_OK; }

	DWORD SetPriority(DWORD p) override { const DWORD old = m_priority; m_priority = p; return old; }
	DWORD GetPriority() override { return m_priority; }
	void PreLoad() override {}
	D3DRESOURCETYPE GetType() override { return D3DRTYPE_VOLUMETEXTURE; }
	DWORD SetLOD(DWORD lod) override { const DWORD old = m_lod; m_lod = lod; return old; }
	DWORD GetLOD() override { return m_lod; }
	DWORD GetLevelCount() override { return (DWORD)m_levels.size(); }

	HRESULT GetLevelDesc(UINT level, D3DVOLUME_DESC *pDesc) override
	{
		if (!pDesc || level >= m_levels.size()) return D3DERR_INVALIDCALL;
		const Level &lvl = m_levels[level];
		memset(pDesc, 0, sizeof(*pDesc));
		pDesc->Format = m_format;
		pDesc->Type = D3DRTYPE_VOLUMETEXTURE;
		pDesc->Usage = 0;
		pDesc->Pool = m_pool;
		pDesc->Size = (UINT)lvl.bits.size();
		pDesc->Width = lvl.w;
		pDesc->Height = lvl.h;
		pDesc->Depth = lvl.d;
		return D3D_OK;
	}

	HRESULT GetVolumeLevel(UINT, IDirect3DVolume8 **ppVolumeLevel) override
	{
		if (ppVolumeLevel) *ppVolumeLevel = nullptr;
		D3D8GLES_TRACE_CALL("VolumeTexture::GetVolumeLevel unimplemented");
		return D3DERR_INVALIDCALL;
	}

	HRESULT LockBox(UINT level, D3DLOCKED_BOX *locked_box, const D3DBOX *, DWORD) override
	{
		if (!locked_box || level >= m_levels.size()) return D3DERR_INVALIDCALL;
		Level &lvl = m_levels[level];
		locked_box->RowPitch = lvl.rowPitch;
		locked_box->SlicePitch = lvl.slicePitch;
		locked_box->pBits = lvl.bits.data();
		return D3D_OK;
	}

	HRESULT UnlockBox(UINT level) override
	{
		return level < m_levels.size() ? D3D_OK : D3DERR_INVALIDCALL;
	}

	HRESULT AddDirtyBox(const D3DBOX *) override { return D3D_OK; }

	WebGLDevice *m_device;
	D3DFORMAT m_format;
	D3DPOOL m_pool;
	DWORD m_priority = 0;
	DWORD m_lod = 0;
	std::vector<Level> m_levels;
};

// ---------------------------------------------------------------------------
// Vertex / index buffers: plain CPU memory in Phase 0.
// ---------------------------------------------------------------------------

class WebGLVertexBuffer final : public IDirect3DVertexBuffer8
{
public:
	WebGLVertexBuffer(WebGLDevice *dev, UINT length, DWORD usage, DWORD fvf, D3DPOOL pool)
		: m_device(dev), m_usage(usage), m_fvf(fvf), m_pool(pool)
	{
		m_bits.resize(length);
	}

	// GeneralsX @build Android port GLES experiment - same leak class as the
	// one fixed in ~WebGLTexture() (glDeleteTextures never called before
	// that fix): this class had no destructor at all, so glDeleteBuffers was
	// never called on release and every VB the engine creates and frees
	// leaks its GL buffer object forever. invalidateBufferBinding() closes
	// the same GL-name-reuse hazard documented on WebGLPipeline's
	// invalidateTextureBinding().
	~WebGLVertexBuffer()
	{
		if (m_gl.name) {
			glDeleteBuffers(1, &m_gl.name);
			WebGLPipeline::get()->invalidateBufferBinding(m_gl.name);
			m_gl.name = 0;
		}
	}

	D3D8GLES_IUNKNOWN_IMPL(WebGLVertexBuffer)

	HRESULT GetDevice(struct IDirect3DDevice8 **ppDevice) override;
	HRESULT SetPrivateData(REFGUID, const void *, DWORD, DWORD) override { return D3D_OK; }
	HRESULT GetPrivateData(REFGUID, void *, DWORD *) override { return D3DERR_NOTFOUND; }
	HRESULT FreePrivateData(REFGUID) override { return D3D_OK; }

	DWORD SetPriority(DWORD p) override { const DWORD old = m_priority; m_priority = p; return old; }
	DWORD GetPriority() override { return m_priority; }
	void PreLoad() override {}
	D3DRESOURCETYPE GetType() override { return D3DRTYPE_VERTEXBUFFER; }

	// GeneralsX @perf Android port 09/05/2026 The `size` argument used to be
	// ignored and Unlock() just flagged the whole buffer dirty, so
	// ensureVBUploaded respecified ALL of it on every update. The engine's
	// shared dynamic VB is DEFAULT_VB_SIZE (5000) vertices -- ~220 KB at
	// dynamic_fvf_type's 44 bytes/vertex -- and Render2DClass::Render() locks
	// just a few quads out of it per call. Real-device timings put that at
	// 16-59 ms/frame for only 24-60 UI draws (~0.5-1 ms per draw), roughly
	// half the frame; SortingRendererClass::Flush (particles, 200+ draws)
	// streams through the same buffer. Record what was actually written so
	// only that range is uploaded.
	HRESULT Lock(UINT offset, UINT size, BYTE **ppbData, DWORD flags) override
	{
		if (!ppbData || offset > m_bits.size()) return D3DERR_INVALIDCALL;
		// D3D8: size 0 means "from offset to the end of the buffer".
		size_t end = (size == 0) ? m_bits.size() : (size_t)offset + size;
		if (end > m_bits.size()) end = m_bits.size();
		m_lockBegin = offset;
		m_lockEnd = end;
		m_lockDiscard = (flags & D3DLOCK_DISCARD) != 0;
		*ppbData = m_bits.data() + offset;
		return D3D_OK;
	}

	HRESULT Unlock() override
	{
		m_gl.markRange(m_lockBegin, m_lockEnd);
		if (m_lockDiscard) m_gl.pendingDiscard = true;
		m_lockBegin = 0;
		m_lockEnd = 0;
		m_lockDiscard = false;
		return D3D_OK;
	}

	HRESULT GetDesc(D3DVERTEXBUFFER_DESC *pDesc) override
	{
		if (!pDesc) return D3DERR_INVALIDCALL;
		pDesc->Format = D3DFMT_VERTEXDATA;
		pDesc->Type = D3DRTYPE_VERTEXBUFFER;
		pDesc->Usage = m_usage;
		pDesc->Pool = m_pool;
		pDesc->Size = (UINT)m_bits.size();
		pDesc->FVF = m_fvf;
		return D3D_OK;
	}

	WebGLDevice *m_device;
	DWORD m_usage;
	DWORD m_fvf;
	D3DPOOL m_pool;
	DWORD m_priority = 0;
	size_t m_lockBegin = 0;
	size_t m_lockEnd = 0;
	bool m_lockDiscard = false;
	GLBufferState m_gl;
	std::vector<BYTE> m_bits;
};

class WebGLIndexBuffer final : public IDirect3DIndexBuffer8
{
public:
	WebGLIndexBuffer(WebGLDevice *dev, UINT length, DWORD usage, D3DFORMAT fmt, D3DPOOL pool)
		: m_device(dev), m_usage(usage), m_format(fmt), m_pool(pool)
	{
		m_bits.resize(length);
	}

	// GeneralsX @build Android port GLES experiment - see
	// ~WebGLVertexBuffer()'s comment; same missing-destructor leak, same fix.
	~WebGLIndexBuffer()
	{
		if (m_gl.name) {
			glDeleteBuffers(1, &m_gl.name);
			WebGLPipeline::get()->invalidateBufferBinding(m_gl.name);
			m_gl.name = 0;
		}
	}

	D3D8GLES_IUNKNOWN_IMPL(WebGLIndexBuffer)

	HRESULT GetDevice(struct IDirect3DDevice8 **ppDevice) override;
	HRESULT SetPrivateData(REFGUID, const void *, DWORD, DWORD) override { return D3D_OK; }
	HRESULT GetPrivateData(REFGUID, void *, DWORD *) override { return D3DERR_NOTFOUND; }
	HRESULT FreePrivateData(REFGUID) override { return D3D_OK; }

	DWORD SetPriority(DWORD p) override { const DWORD old = m_priority; m_priority = p; return old; }
	DWORD GetPriority() override { return m_priority; }
	void PreLoad() override {}
	D3DRESOURCETYPE GetType() override { return D3DRTYPE_INDEXBUFFER; }

	// GeneralsX @perf Android port 09/05/2026 Same dirty-range tracking as
	// WebGLVertexBuffer::Lock -- see that comment for the measurements.
	HRESULT Lock(UINT offset, UINT size, BYTE **ppbData, DWORD flags) override
	{
		if (!ppbData || offset > m_bits.size()) return D3DERR_INVALIDCALL;
		size_t end = (size == 0) ? m_bits.size() : (size_t)offset + size;
		if (end > m_bits.size()) end = m_bits.size();
		m_lockBegin = offset;
		m_lockEnd = end;
		m_lockDiscard = (flags & D3DLOCK_DISCARD) != 0;
		*ppbData = m_bits.data() + offset;
		return D3D_OK;
	}

	HRESULT Unlock() override
	{
		m_gl.markRange(m_lockBegin, m_lockEnd);
		if (m_lockDiscard) m_gl.pendingDiscard = true;
		m_lockBegin = 0;
		m_lockEnd = 0;
		m_lockDiscard = false;
		return D3D_OK;
	}

	HRESULT GetDesc(D3DINDEXBUFFER_DESC *pDesc) override
	{
		if (!pDesc) return D3DERR_INVALIDCALL;
		pDesc->Format = m_format;
		pDesc->Type = D3DRTYPE_INDEXBUFFER;
		pDesc->Usage = m_usage;
		pDesc->Pool = m_pool;
		pDesc->Size = (UINT)m_bits.size();
		return D3D_OK;
	}

	WebGLDevice *m_device;
	DWORD m_usage;
	D3DFORMAT m_format;
	D3DPOOL m_pool;
	DWORD m_priority = 0;
	size_t m_lockBegin = 0;
	size_t m_lockEnd = 0;
	bool m_lockDiscard = false;
	GLBufferState m_gl;
	std::vector<BYTE> m_bits;
};

// ---------------------------------------------------------------------------
// Swap chain stub (W3D creates one additional swap chain in some paths).
// ---------------------------------------------------------------------------

class WebGLSwapChain final : public IDirect3DSwapChain8
{
public:
	WebGLSwapChain(WebGLDevice *dev, UINT w, UINT h, D3DFORMAT fmt)
		: m_device(dev)
	{
		m_backBuffer = new WebGLSurface(dev, w, h, fmt, D3DRTYPE_SURFACE);
	}

	~WebGLSwapChain()
	{
		if (m_backBuffer) m_backBuffer->Release();
	}

	D3D8GLES_IUNKNOWN_IMPL(WebGLSwapChain)

	HRESULT Present(const RECT *, const RECT *, HWND, const RGNDATA *) override { return D3D_OK; }

	HRESULT GetBackBuffer(UINT, D3DBACKBUFFER_TYPE, struct IDirect3DSurface8 **ppBackBuffer) override
	{
		if (!ppBackBuffer) return D3DERR_INVALIDCALL;
		m_backBuffer->AddRef();
		*ppBackBuffer = m_backBuffer;
		return D3D_OK;
	}

	WebGLDevice *m_device;
	WebGLSurface *m_backBuffer;
};

// ---------------------------------------------------------------------------
// The device.
// ---------------------------------------------------------------------------

static void FillCaps(D3DCAPS8 *caps, UINT adapter);

class WebGLDevice final : public IDirect3DDevice8
{
public:
	WebGLDevice(IDirect3D8 *parent, const D3DPRESENT_PARAMETERS *pp, HWND focusWindow, DWORD behaviorFlags)
		: m_parent(parent), m_focusWindow(focusWindow), m_behaviorFlags(behaviorFlags)
	{
		m_pp = *pp;
		if (m_pp.BackBufferWidth == 0) m_pp.BackBufferWidth = 800;
		if (m_pp.BackBufferHeight == 0) m_pp.BackBufferHeight = 600;
		if (m_pp.BackBufferFormat == D3DFMT_UNKNOWN) m_pp.BackBufferFormat = D3DFMT_X8R8G8B8;
		createBackBuffers();
		memset(m_textures, 0, sizeof(m_textures));
		memset(m_streamStride, 0, sizeof(m_streamStride));
		memset(m_streams, 0, sizeof(m_streams));
		// D3D default transforms are identity, not zero.
		for (size_t i = 0; i < kMaxTransforms; i++) {
			memset(&m_transforms[i], 0, sizeof(D3DMATRIX));
			m_transforms[i]._11 = m_transforms[i]._22 = 1.0f;
			m_transforms[i]._33 = m_transforms[i]._44 = 1.0f;
		}
		// Non-zero D3D render-state defaults the engine relies on. In
		// particular COLORWRITEENABLE defaults to all channels; a zero here
		// must mean "the game explicitly disabled color writes" (stencil
		// shadow volume passes), which the pipeline honors as an all-off mask.
		m_renderStates[D3DRS_COLORWRITEENABLE] = 0xF;
		m_renderStates[D3DRS_COLORVERTEX] = TRUE;
		m_renderStates[D3DRS_DIFFUSEMATERIALSOURCE] = D3DMCS_COLOR1;
		m_renderStates[D3DRS_SPECULARMATERIALSOURCE] = D3DMCS_COLOR2;
		// AMBIENT/EMISSIVEMATERIALSOURCE default to D3DMCS_MATERIAL (0).
		// GeneralsX @bugfix Android port 09/05/2026 Stencil defaults. These
		// matter for the same reason COLORWRITEENABLE above does: zero is a
		// MEANINGFUL value for the mask states (D3D compares/writes no bits),
		// so the pipeline must be able to tell "the game asked for 0" from
		// "nobody ever set this". Seeding D3D8's documented defaults here is
		// what lets applyFixedState pass these through verbatim instead of
		// second-guessing a zero -- see the stencil block there for the bug
		// that guessing caused.
		m_renderStates[D3DRS_STENCILFUNC] = D3DCMP_ALWAYS;
		m_renderStates[D3DRS_STENCILMASK] = 0xFFFFFFFF;
		m_renderStates[D3DRS_STENCILWRITEMASK] = 0xFFFFFFFF;
		m_renderStates[D3DRS_STENCILFAIL] = D3DSTENCILOP_KEEP;
		m_renderStates[D3DRS_STENCILZFAIL] = D3DSTENCILOP_KEEP;
		m_renderStates[D3DRS_STENCILPASS] = D3DSTENCILOP_KEEP;
		// GeneralsX @bugfix Android port 09/05/2026 THE DEPTH DEFAULTS, and the
		// reason this whole block exists. D3DRS_ZENABLE defaults to D3DZB_TRUE
		// in D3D8, and the engine relies on that -- ShaderClass drives ZFUNC and
		// ZWRITEENABLE but never turns the depth TEST on, because under a real
		// runtime it already is. With the array starting zeroed, ZENABLE read
		// back as 0 = D3DZB_FALSE and the pipeline disabled GL_DEPTH_TEST for
		// the entire scene: everything drew in submission order, so the water,
		// which is drawn last, painted over the terrain in front of it.
		//
		// It only showed below High detail because W3DVolumetricShadow's fill
		// passes set D3DRS_ZENABLE = TRUE explicitly, and shadow volumes are
		// on only at High and above. That single write repaired the state for
		// the rest of the session, which is exactly why a fresh start on Low
		// was broken, a live switch DOWN to Low was fine, and switching up to
		// High "fixed" it permanently. Measured directly:
		//   start Low      -> [gxstate] TERRAIN zEnable=0 ... (all categories)
		//   switch to High -> [gxstate] TERRAIN zEnable=1 ...
		//   back to Low    -> [gxstate] TERRAIN zEnable=1 ... (stays repaired)
		m_renderStates[D3DRS_ZENABLE] = D3DZB_TRUE;
		m_renderStates[D3DRS_ZWRITEENABLE] = TRUE;
		m_renderStates[D3DRS_ZFUNC] = D3DCMP_LESSEQUAL;
		// The rest of the D3D8 defaults the pipeline actually reads. Same rule
		// as above: a zero here must mean "the game asked for zero", so every
		// state whose documented default is non-zero has to be seeded rather
		// than pattern-matched at the point of use.
		m_renderStates[D3DRS_CULLMODE] = D3DCULL_CCW;
		m_renderStates[D3DRS_SRCBLEND] = D3DBLEND_ONE;
		m_renderStates[D3DRS_DESTBLEND] = D3DBLEND_ZERO;
		m_renderStates[D3DRS_ALPHAFUNC] = D3DCMP_ALWAYS;
		m_renderStates[D3DRS_TEXTUREFACTOR] = 0xFFFFFFFF;
		// GeneralsX @build Android port 09/05/2026 Prints the seeded values back
		// out. Two consecutive builds differed ONLY by this seeding and emitted
		// no distinguishing log line, so when a device log came back still
		// showing zEnable=0 there was no way to tell "the seeding is not in the
		// build you tested" from "the seeding is there and something overwrites
		// it". That ambiguity cost a full test round. Any change whose whole
		// effect is a value must print that value.
		fprintf(stderr, "[gxdefaults] seeded render states: zEnable=%u zWrite=%u "
			"zFunc=%u cull=%u src=%u dst=%u alphaFunc=%u tFactor=0x%x\n",
			(unsigned)m_renderStates[D3DRS_ZENABLE],
			(unsigned)m_renderStates[D3DRS_ZWRITEENABLE],
			(unsigned)m_renderStates[D3DRS_ZFUNC],
			(unsigned)m_renderStates[D3DRS_CULLMODE],
			(unsigned)m_renderStates[D3DRS_SRCBLEND],
			(unsigned)m_renderStates[D3DRS_DESTBLEND],
			(unsigned)m_renderStates[D3DRS_ALPHAFUNC],
			(unsigned)m_renderStates[D3DRS_TEXTUREFACTOR]);
		// D3DRS_LIGHTING's documented default is TRUE, and it is deliberately
		// NOT seeded: every engine path that wants lighting sets it explicitly
		// (VertexMaterialClass::Apply, ShaderClass), so seeding it would light
		// draws that are currently unlit and change rendering with nothing to
		// show for it. Revisit only with a measurement that says a path relies
		// on the default.
		// Phase 2: bring up the WebGL2 pipeline on the canvas.
		WebGLPipeline::get()->initContext((int)m_pp.BackBufferWidth, (int)m_pp.BackBufferHeight,
			(SDL_Window *)focusWindow);
	}

	D3D8GLES_IUNKNOWN_IMPL(WebGLDevice)

	// --- Cooperative level / misc -----------------------------------------
	HRESULT TestCooperativeLevel() override { return D3D_OK; }
	UINT GetAvailableTextureMem() override { return 256u * 1024u * 1024u; }
	HRESULT ResourceManagerDiscardBytes(DWORD) override { return D3D_OK; }

	HRESULT GetDirect3D(IDirect3D8 **ppD3D8) override
	{
		if (!ppD3D8) return D3DERR_INVALIDCALL;
		m_parent->AddRef();
		*ppD3D8 = m_parent;
		return D3D_OK;
	}

	HRESULT GetDeviceCaps(D3DCAPS8 *pCaps) override
	{
		if (!pCaps) return D3DERR_INVALIDCALL;
		FillCaps(pCaps, 0);
		return D3D_OK;
	}

	HRESULT GetDisplayMode(D3DDISPLAYMODE *pMode) override
	{
		if (!pMode) return D3DERR_INVALIDCALL;
		pMode->Width = m_pp.BackBufferWidth;
		pMode->Height = m_pp.BackBufferHeight;
		pMode->RefreshRate = 60;
		pMode->Format = m_pp.BackBufferFormat;
		return D3D_OK;
	}

	HRESULT GetCreationParameters(D3DDEVICE_CREATION_PARAMETERS *pParameters) override
	{
		if (!pParameters) return D3DERR_INVALIDCALL;
		pParameters->AdapterOrdinal = 0;
		pParameters->DeviceType = D3DDEVTYPE_HAL;
		pParameters->hFocusWindow = m_focusWindow;
		pParameters->BehaviorFlags = m_behaviorFlags;
		return D3D_OK;
	}

	HRESULT SetCursorProperties(UINT, UINT, IDirect3DSurface8 *) override { return D3D_OK; }
	void SetCursorPosition(UINT, UINT, DWORD) override {}
	WINBOOL ShowCursor(WINBOOL bShow) override { const WINBOOL old = m_cursorShown; m_cursorShown = bShow; return old; }

	HRESULT CreateAdditionalSwapChain(D3DPRESENT_PARAMETERS *pp, IDirect3DSwapChain8 **ppSwapChain) override
	{
		if (!pp || !ppSwapChain) return D3DERR_INVALIDCALL;
		*ppSwapChain = new WebGLSwapChain(this,
			pp->BackBufferWidth ? pp->BackBufferWidth : m_pp.BackBufferWidth,
			pp->BackBufferHeight ? pp->BackBufferHeight : m_pp.BackBufferHeight,
			pp->BackBufferFormat != D3DFMT_UNKNOWN ? pp->BackBufferFormat : m_pp.BackBufferFormat);
		return D3D_OK;
	}

	HRESULT Reset(D3DPRESENT_PARAMETERS *pp) override
	{
		if (!pp) return D3DERR_INVALIDCALL;
		m_pp = *pp;
		if (m_pp.BackBufferWidth == 0) m_pp.BackBufferWidth = 800;
		if (m_pp.BackBufferHeight == 0) m_pp.BackBufferHeight = 600;
		if (m_pp.BackBufferFormat == D3DFMT_UNKNOWN) m_pp.BackBufferFormat = D3DFMT_X8R8G8B8;
		releaseBackBuffers();
		createBackBuffers();
		WebGLPipeline::get()->resize((int)m_pp.BackBufferWidth, (int)m_pp.BackBufferHeight);
		D3D8GLES_TRACE_CALL("Device::Reset %ux%u", m_pp.BackBufferWidth, m_pp.BackBufferHeight);
		return D3D_OK;
	}

	HRESULT Present(const RECT *, const RECT *, HWND, const RGNDATA *) override
	{
		WebGLPipeline::get()->present();
		return D3D_OK;
	}

	HRESULT GetBackBuffer(UINT, D3DBACKBUFFER_TYPE, IDirect3DSurface8 **ppBackBuffer) override
	{
		if (!ppBackBuffer) return D3DERR_INVALIDCALL;
		m_backBuffer->AddRef();
		*ppBackBuffer = m_backBuffer;
		return D3D_OK;
	}

	HRESULT GetRasterStatus(D3DRASTER_STATUS *pRasterStatus) override
	{
		if (!pRasterStatus) return D3DERR_INVALIDCALL;
		pRasterStatus->InVBlank = FALSE;
		pRasterStatus->ScanLine = 0;
		return D3D_OK;
	}

	void SetGammaRamp(DWORD, const D3DGAMMARAMP *) override {}
	void GetGammaRamp(D3DGAMMARAMP *pRamp) override
	{
		if (!pRamp) return;
		for (int i = 0; i < 256; i++) {
			pRamp->red[i] = pRamp->green[i] = pRamp->blue[i] = (WORD)(i << 8);
		}
	}

	// --- Resource creation --------------------------------------------------
	HRESULT CreateTexture(UINT w, UINT h, UINT levels, DWORD /*usage*/, D3DFORMAT fmt, D3DPOOL pool,
	                      IDirect3DTexture8 **ppTexture) override
	{
		if (!ppTexture) return D3DERR_INVALIDCALL;
		*ppTexture = new WebGLTexture(this, w, h, levels, fmt, pool);
		return D3D_OK;
	}

	HRESULT CreateVolumeTexture(UINT w, UINT h, UINT d, UINT levels, DWORD /*usage*/, D3DFORMAT fmt,
	                            D3DPOOL pool, IDirect3DVolumeTexture8 **ppVolumeTexture) override
	{
		if (!ppVolumeTexture) return D3DERR_INVALIDCALL;
		*ppVolumeTexture = new WebGLVolumeTexture(this, w, h, d, levels, fmt, pool);
		return D3D_OK;
	}

	HRESULT CreateCubeTexture(UINT edge, UINT levels, DWORD /*usage*/, D3DFORMAT fmt, D3DPOOL pool,
	                          IDirect3DCubeTexture8 **ppCubeTexture) override
	{
		if (!ppCubeTexture) return D3DERR_INVALIDCALL;
		*ppCubeTexture = new WebGLCubeTexture(this, edge, levels, fmt, pool);
		return D3D_OK;
	}

	HRESULT CreateVertexBuffer(UINT length, DWORD usage, DWORD fvf, D3DPOOL pool,
	                           IDirect3DVertexBuffer8 **ppVertexBuffer) override
	{
		if (!ppVertexBuffer) return D3DERR_INVALIDCALL;
		*ppVertexBuffer = new WebGLVertexBuffer(this, length, usage, fvf, pool);
		return D3D_OK;
	}

	HRESULT CreateIndexBuffer(UINT length, DWORD usage, D3DFORMAT fmt, D3DPOOL pool,
	                          IDirect3DIndexBuffer8 **ppIndexBuffer) override
	{
		if (!ppIndexBuffer) return D3DERR_INVALIDCALL;
		*ppIndexBuffer = new WebGLIndexBuffer(this, length, usage, fmt, pool);
		return D3D_OK;
	}

	HRESULT CreateRenderTarget(UINT w, UINT h, D3DFORMAT fmt, D3DMULTISAMPLE_TYPE, WINBOOL,
	                           IDirect3DSurface8 **ppSurface) override
	{
		if (!ppSurface) return D3DERR_INVALIDCALL;
		*ppSurface = new WebGLSurface(this, w, h, fmt, D3DRTYPE_SURFACE);
		return D3D_OK;
	}

	HRESULT CreateDepthStencilSurface(UINT w, UINT h, D3DFORMAT fmt, D3DMULTISAMPLE_TYPE,
	                                  IDirect3DSurface8 **ppSurface) override
	{
		if (!ppSurface) return D3DERR_INVALIDCALL;
		*ppSurface = new WebGLSurface(this, w, h, fmt, D3DRTYPE_SURFACE);
		return D3D_OK;
	}

	HRESULT CreateImageSurface(UINT w, UINT h, D3DFORMAT fmt, IDirect3DSurface8 **ppSurface) override
	{
		if (!ppSurface) return D3DERR_INVALIDCALL;
		*ppSurface = new WebGLSurface(this, w, h, fmt, D3DRTYPE_SURFACE);
		return D3D_OK;
	}

	HRESULT CopyRects(IDirect3DSurface8 *src_surface, const RECT *src_rects, UINT rect_count,
	                  IDirect3DSurface8 *dst_surface, const POINT *dst_points) override
	{
		// CPU copy between the shadow bits; enough for the engine's
		// texture-upload path (surface -> texture level).
		if (!src_surface || !dst_surface) return D3DERR_INVALIDCALL;
		WebGLSurface *src = static_cast<WebGLSurface *>(src_surface);
		WebGLSurface *dst = static_cast<WebGLSurface *>(dst_surface);
		// GeneralsX @build Android port GLES experiment - see
		// WebGLSurface::m_alive / RetireSurface. CopyRects is the actual
		// icon-update path (cameo blits go through here, confirmed by the
		// CopyRects diagnostic log below) and touches src/dst directly
		// without going through LockRect, so it needs its own guard.
		if (!src->m_alive || !dst->m_alive) {
			fprintf(stderr, "[d3d8gles] WARNING: CopyRects on a retired surface "
				"(src=%p alive=%d, dst=%p alive=%d) -- owning texture already "
				"destroyed, use-after-free avoided; this is very likely the "
				"remaining icon-flicker cause\n",
				(void *)src, src->m_alive, (void *)dst, dst->m_alive);
			return D3DERR_INVALIDCALL;
		}
		// GeneralsX @perf Android port 09/05/2026 Removed the per-call
		// srcNonZeroBytes diagnostic that used to live here. It scanned EVERY
		// byte of the source surface for the first 80 calls -- ~11.6M
		// iterations per call on a full-screen surface -- as a one-off probe
		// for the (since resolved) missing-cameo-icon investigation. The
		// m_alive guard above is a real safety check and stays.
		if (src->m_format != dst->m_format) return D3DERR_INVALIDCALL;

		// GeneralsX @bugfix Android port 09/05/2026 If the SOURCE is a render
		// target, its CPU shadow bits are stale by construction -- the content
		// only exists in GL. Pull it back first, or the memcpy below faithfully
		// copies zeroes. This is how projected (runtime-generated) shadows are
		// moved into their permanent texture; see
		// WebGLPipeline::readbackRenderTarget for the full story. The fbo
		// handle is the discriminator: it is only ever created by
		// setRenderTarget, so an ordinary texture never takes this path.
		if (src->m_ownerTex != nullptr && src->m_ownerTex->m_gl.fbo != 0 &&
		    !src->m_ownerTex->m_levels.empty() && src->m_ownerTex->m_levels[0] == src) {
			WebGLPipeline::get()->readbackRenderTarget(src->m_ownerTex);
		}

		if (rect_count == 0 || src_rects == nullptr) {
			const size_t n = src->m_bits.size() < dst->m_bits.size() ? src->m_bits.size() : dst->m_bits.size();
			memcpy(dst->m_bits.data(), src->m_bits.data(), n);
			if (dst->m_ownerGL) dst->m_ownerGL->dirty = true;
			return D3D_OK;
		}

		if (FormatIsDXT(src->m_format)) {
			// Block copies with arbitrary rects are not needed in Phase 0.
			const size_t n = src->m_bits.size() < dst->m_bits.size() ? src->m_bits.size() : dst->m_bits.size();
			memcpy(dst->m_bits.data(), src->m_bits.data(), n);
			if (dst->m_ownerGL) dst->m_ownerGL->dirty = true;
			return D3D_OK;
		}

		const UINT bpp = FormatBytesPerPixel(src->m_format);
		for (UINT i = 0; i < rect_count; i++) {
			const RECT &r = src_rects[i];
			LONG dx = dst_points ? dst_points[i].x : r.left;
			LONG dy = dst_points ? dst_points[i].y : r.top;
			const LONG rows = r.bottom - r.top;
			const LONG cols = r.right - r.left;
			for (LONG row = 0; row < rows; row++) {
				const BYTE *s = src->m_bits.data() + (r.top + row) * src->m_pitch + r.left * bpp;
				BYTE *d = dst->m_bits.data() + (dy + row) * dst->m_pitch + dx * bpp;
				memcpy(d, s, (size_t)cols * bpp);
			}
		}
		if (dst->m_ownerGL) dst->m_ownerGL->dirty = true;
		return D3D_OK;
	}

	HRESULT UpdateTexture(IDirect3DBaseTexture8 *src, IDirect3DBaseTexture8 *dst) override
	{
		if (!src || !dst) return D3DERR_INVALIDCALL;
		if (src->GetType() != D3DRTYPE_TEXTURE || dst->GetType() != D3DRTYPE_TEXTURE) return D3D_OK;
		WebGLTexture *s = static_cast<WebGLTexture *>(src);
		WebGLTexture *d = static_cast<WebGLTexture *>(dst);
		const size_t levels = s->m_levels.size() < d->m_levels.size() ? s->m_levels.size() : d->m_levels.size();
		for (size_t i = 0; i < levels; i++) {
			const size_t n = s->m_levels[i]->m_bits.size() < d->m_levels[i]->m_bits.size()
				? s->m_levels[i]->m_bits.size() : d->m_levels[i]->m_bits.size();
			memcpy(d->m_levels[i]->m_bits.data(), s->m_levels[i]->m_bits.data(), n);
		}
		d->m_gl.dirty = true;
		return D3D_OK;
	}

	HRESULT GetFrontBuffer(IDirect3DSurface8 *pDestSurface) override
	{
		return pDestSurface ? D3D_OK : D3DERR_INVALIDCALL;
	}

	HRESULT SetRenderTarget(IDirect3DSurface8 *pRenderTarget, IDirect3DSurface8 *pNewZStencil) override
	{
		if (pRenderTarget) {
			// GeneralsX @feature Android port 09/09/2026 One-shot diagnostic. This is the
			// exact moment W3DShaderManager::endRenderToTexture() hands the scene it just
			// rendered offscreen to a screen filter, which samples it as a texture. When
			// the black-and-white cinematic came out solid black there was no way to tell
			// "the render target is empty" from "the combiner maths is wrong" -- this
			// prints what is in the render target so the two can never be confused again.
			// Prints at most 4 times per launch, only for backbuffer-sized targets.
			if (m_currentRT && m_currentRT != pRenderTarget &&
			    static_cast<WebGLSurface *>(pRenderTarget) == m_backBuffer &&
			    m_currentRT->m_ownerTex != nullptr && m_currentRT->m_ownerTex->m_gl.fbo != 0) {
				WebGLPipeline::get()->debugSampleRenderTarget(m_currentRT->m_ownerTex,
					"endRenderToTexture");
			}

			pRenderTarget->AddRef();
			if (m_currentRT) m_currentRT->Release();
			m_currentRT = static_cast<WebGLSurface *>(pRenderTarget);

			if (m_currentRT == m_backBuffer) {
				WebGLPipeline::get()->setRenderTarget(this, nullptr);
			} else if (m_currentRT->m_ownerTex) {
				WebGLPipeline::get()->setRenderTarget(this, m_currentRT->m_ownerTex);
			} else {
				static bool s_rtWarn = false;
				if (!s_rtWarn) {
					s_rtWarn = true;
					fprintf(stderr, "[d3d8gles] WARN: SetRenderTarget to standalone surface (%ux%u) unsupported\n",
						m_currentRT->m_width, m_currentRT->m_height);
				}
				WebGLPipeline::get()->setRenderTarget(this, nullptr);
			}
		}
		if (pNewZStencil) {
			pNewZStencil->AddRef();
			if (m_currentDS) m_currentDS->Release();
			m_currentDS = static_cast<WebGLSurface *>(pNewZStencil);
		}
		return D3D_OK;
	}

	HRESULT GetRenderTarget(IDirect3DSurface8 **ppRenderTarget) override
	{
		if (!ppRenderTarget) return D3DERR_INVALIDCALL;
		WebGLSurface *rt = m_currentRT ? m_currentRT : m_backBuffer;
		rt->AddRef();
		*ppRenderTarget = rt;
		return D3D_OK;
	}

	HRESULT GetDepthStencilSurface(IDirect3DSurface8 **ppZStencilSurface) override
	{
		if (!ppZStencilSurface) return D3DERR_INVALIDCALL;
		WebGLSurface *ds = m_currentDS ? m_currentDS : m_depthStencil;
		if (!ds) {
			*ppZStencilSurface = nullptr;
			return D3DERR_NOTFOUND;
		}
		ds->AddRef();
		*ppZStencilSurface = ds;
		return D3D_OK;
	}

	// --- Scene / draw --------------------------------------------------------
	HRESULT BeginScene() override { return D3D_OK; }
	HRESULT EndScene() override { return D3D_OK; }
	HRESULT Clear(DWORD, const D3DRECT *, DWORD flags, D3DCOLOR color, float z, DWORD stencil) override
	{
		WebGLPipeline::get()->clear(this, flags, color, z, stencil);
		return D3D_OK;
	}

	HRESULT SetTransform(D3DTRANSFORMSTATETYPE state, const D3DMATRIX *matrix) override
	{
		if (!matrix) return D3DERR_INVALIDCALL;
		if ((size_t)state < kMaxTransforms) m_transforms[state] = *matrix;
		return D3D_OK;
	}

	HRESULT GetTransform(D3DTRANSFORMSTATETYPE state, D3DMATRIX *pMatrix) override
	{
		if (!pMatrix) return D3DERR_INVALIDCALL;
		if ((size_t)state < kMaxTransforms) {
			*pMatrix = m_transforms[state];
			return D3D_OK;
		}
		return D3DERR_INVALIDCALL;
	}

	HRESULT MultiplyTransform(D3DTRANSFORMSTATETYPE, const D3DMATRIX *) override { return D3D_OK; }

	HRESULT SetViewport(const D3DVIEWPORT8 *viewport) override
	{
		if (!viewport) return D3DERR_INVALIDCALL;
		m_viewport = *viewport;
		return D3D_OK;
	}

	HRESULT GetViewport(D3DVIEWPORT8 *pViewport) override
	{
		if (!pViewport) return D3DERR_INVALIDCALL;
		*pViewport = m_viewport;
		return D3D_OK;
	}

	HRESULT SetMaterial(const D3DMATERIAL8 *material) override
	{
		if (!material) return D3DERR_INVALIDCALL;
		m_material = *material;
		return D3D_OK;
	}

	HRESULT GetMaterial(D3DMATERIAL8 *pMaterial) override
	{
		if (!pMaterial) return D3DERR_INVALIDCALL;
		*pMaterial = m_material;
		return D3D_OK;
	}

	HRESULT SetLight(DWORD index, const D3DLIGHT8 *light) override
	{
		if (!light || index >= kMaxLights) return D3D_OK;
		m_lights[index] = *light;
		return D3D_OK;
	}

	HRESULT GetLight(DWORD index, D3DLIGHT8 *pLight) override
	{
		if (!pLight || index >= kMaxLights) return D3DERR_INVALIDCALL;
		*pLight = m_lights[index];
		return D3D_OK;
	}

	HRESULT LightEnable(DWORD index, WINBOOL enable) override
	{
		if (index < kMaxLights) m_lightEnabled[index] = enable;
		return D3D_OK;
	}

	HRESULT GetLightEnable(DWORD index, WINBOOL *pEnable) override
	{
		if (!pEnable || index >= kMaxLights) return D3DERR_INVALIDCALL;
		*pEnable = m_lightEnabled[index];
		return D3D_OK;
	}

	HRESULT SetClipPlane(DWORD, const float *) override { return D3D_OK; }
	HRESULT GetClipPlane(DWORD, float *) override { return D3D_OK; }

	HRESULT SetRenderState(D3DRENDERSTATETYPE state, DWORD value) override
	{
		if ((size_t)state < kMaxRenderStates) m_renderStates[state] = value;
		return D3D_OK;
	}

	HRESULT GetRenderState(D3DRENDERSTATETYPE state, DWORD *pValue) override
	{
		if (!pValue) return D3DERR_INVALIDCALL;
		*pValue = ((size_t)state < kMaxRenderStates) ? m_renderStates[state] : 0;
		return D3D_OK;
	}

	HRESULT BeginStateBlock() override { return D3D_OK; }
	HRESULT EndStateBlock(DWORD *pToken) override
	{
		if (pToken) *pToken = 1;
		return D3D_OK;
	}
	HRESULT ApplyStateBlock(DWORD) override { return D3D_OK; }
	HRESULT CaptureStateBlock(DWORD) override { return D3D_OK; }
	HRESULT DeleteStateBlock(DWORD) override { return D3D_OK; }
	HRESULT CreateStateBlock(D3DSTATEBLOCKTYPE, DWORD *pToken) override
	{
		if (pToken) *pToken = 1;
		return D3D_OK;
	}

	HRESULT SetClipStatus(const D3DCLIPSTATUS8 *) override { return D3D_OK; }
	HRESULT GetClipStatus(D3DCLIPSTATUS8 *pClipStatus) override
	{
		if (pClipStatus) memset(pClipStatus, 0, sizeof(*pClipStatus));
		return D3D_OK;
	}

	HRESULT GetTexture(DWORD stage, IDirect3DBaseTexture8 **ppTexture) override
	{
		if (!ppTexture || stage >= kMaxTextureStages) return D3DERR_INVALIDCALL;
		if (m_textures[stage]) m_textures[stage]->AddRef();
		*ppTexture = m_textures[stage];
		return D3D_OK;
	}

	HRESULT SetTexture(DWORD stage, IDirect3DBaseTexture8 *pTexture) override
	{
		if (stage >= kMaxTextureStages) return D3D_OK;
		if (pTexture) pTexture->AddRef();
		if (m_textures[stage]) m_textures[stage]->Release();
		m_textures[stage] = pTexture;
		return D3D_OK;
	}

	HRESULT GetTextureStageState(DWORD stage, D3DTEXTURESTAGESTATETYPE type, DWORD *pValue) override
	{
		if (!pValue) return D3DERR_INVALIDCALL;
		*pValue = (stage < kMaxTextureStages && (size_t)type < kMaxStageStates)
			? m_stageStates[stage][type] : 0;
		return D3D_OK;
	}

	HRESULT SetTextureStageState(DWORD stage, D3DTEXTURESTAGESTATETYPE type, DWORD value) override
	{
		if (stage < kMaxTextureStages && (size_t)type < kMaxStageStates) {
			m_stageStates[stage][type] = value;
		}
		return D3D_OK;
	}

	HRESULT ValidateDevice(DWORD *pNumPasses) override
	{
		if (pNumPasses) *pNumPasses = 1;
		return D3D_OK;
	}

	HRESULT GetInfo(DWORD, void *, DWORD) override { return E_FAIL; }
	HRESULT SetPaletteEntries(UINT, const PALETTEENTRY *) override { return D3D_OK; }
	HRESULT GetPaletteEntries(UINT, PALETTEENTRY *) override { return D3D_OK; }
	HRESULT SetCurrentTexturePalette(UINT) override { return D3D_OK; }
	HRESULT GetCurrentTexturePalette(UINT *p) override
	{
		if (p) *p = 0;
		return D3D_OK;
	}

	HRESULT DrawPrimitive(D3DPRIMITIVETYPE pt, UINT startVertex, UINT primCount) override
	{
		WebGLPipeline::get()->draw(this, pt, startVertex, primCount);
		return D3D_OK;
	}
	HRESULT DrawIndexedPrimitive(D3DPRIMITIVETYPE pt, UINT minIndex, UINT numVertices,
	                             UINT startIndex, UINT primCount) override
	{
		WebGLPipeline::get()->drawIndexed(this, pt, minIndex, numVertices, startIndex, primCount);
		return D3D_OK;
	}
	HRESULT DrawPrimitiveUP(D3DPRIMITIVETYPE pt, UINT primCount, const void *data, UINT stride) override
	{
		WebGLPipeline::get()->drawUP(this, pt, primCount, data, stride);
		return D3D_OK;
	}
	HRESULT DrawIndexedPrimitiveUP(D3DPRIMITIVETYPE pt, UINT minVertexIdx, UINT numVertices,
	                               UINT primCount, const void *indexData, D3DFORMAT indexFormat,
	                               const void *data, UINT stride) override
	{
		WebGLPipeline::get()->drawIndexedUP(this, pt, minVertexIdx, numVertices, primCount,
		                                    indexData, indexFormat, data, stride);
		return D3D_OK;
	}
	HRESULT ProcessVertices(UINT, UINT, UINT, IDirect3DVertexBuffer8 *, DWORD) override { return D3D_OK; }

	// --- Shaders (fixed-function only: FVF codes pass through) ---------------
	HRESULT CreateVertexShader(const DWORD *, const DWORD *, DWORD *shader, DWORD) override
	{
		// Caps report no programmable shaders; the engine should never call
		// this. If it does, fail loudly so the caller takes the FF path.
		D3D8GLES_TRACE_CALL("CreateVertexShader called despite vs=0 caps");
		if (shader) *shader = 0;
		return D3DERR_NOTAVAILABLE;
	}

	HRESULT SetVertexShader(DWORD handle) override
	{
		// FVF codes are valid handles in D3D8; programmable handles are not
		// created (see CreateVertexShader).
		m_fvf = handle;
		return D3D_OK;
	}

	HRESULT GetVertexShader(DWORD *pHandle) override
	{
		if (!pHandle) return D3DERR_INVALIDCALL;
		*pHandle = m_fvf;
		return D3D_OK;
	}

	HRESULT DeleteVertexShader(DWORD) override { return D3D_OK; }
	HRESULT SetVertexShaderConstant(DWORD, const void *, DWORD) override { return D3D_OK; }
	HRESULT GetVertexShaderConstant(DWORD, void *, DWORD) override { return D3D_OK; }
	HRESULT GetVertexShaderDeclaration(DWORD, void *, DWORD *) override { return E_FAIL; }
	HRESULT GetVertexShaderFunction(DWORD, void *, DWORD *) override { return E_FAIL; }

	HRESULT SetStreamSource(UINT stream, IDirect3DVertexBuffer8 *pStreamData, UINT stride) override
	{
		if (stream >= kMaxStreams) return D3DERR_INVALIDCALL;
		if (pStreamData) pStreamData->AddRef();
		if (m_streams[stream]) m_streams[stream]->Release();
		m_streams[stream] = pStreamData;
		m_streamStride[stream] = stride;
		return D3D_OK;
	}

	HRESULT GetStreamSource(UINT stream, IDirect3DVertexBuffer8 **ppStreamData, UINT *pStride) override
	{
		if (!ppStreamData || stream >= kMaxStreams) return D3DERR_INVALIDCALL;
		if (m_streams[stream]) m_streams[stream]->AddRef();
		*ppStreamData = m_streams[stream];
		if (pStride) *pStride = m_streamStride[stream];
		return D3D_OK;
	}

	HRESULT SetIndices(IDirect3DIndexBuffer8 *pIndexData, UINT baseVertexIndex) override
	{
		if (pIndexData) pIndexData->AddRef();
		if (m_indices) m_indices->Release();
		m_indices = pIndexData;
		m_baseVertexIndex = baseVertexIndex;
		return D3D_OK;
	}

	HRESULT GetIndices(IDirect3DIndexBuffer8 **ppIndexData, UINT *pBaseVertexIndex) override
	{
		if (!ppIndexData) return D3DERR_INVALIDCALL;
		if (m_indices) m_indices->AddRef();
		*ppIndexData = m_indices;
		if (pBaseVertexIndex) *pBaseVertexIndex = m_baseVertexIndex;
		return D3D_OK;
	}

	HRESULT CreatePixelShader(const DWORD *, DWORD *shader) override
	{
		D3D8GLES_TRACE_CALL("CreatePixelShader called despite ps=0 caps");
		if (shader) *shader = 0;
		return D3DERR_NOTAVAILABLE;
	}

	HRESULT SetPixelShader(DWORD) override { return D3D_OK; }
	HRESULT GetPixelShader(DWORD *pHandle) override
	{
		if (pHandle) *pHandle = 0;
		return D3D_OK;
	}
	HRESULT DeletePixelShader(DWORD) override { return D3D_OK; }
	HRESULT SetPixelShaderConstant(DWORD, const void *, DWORD) override { return D3D_OK; }
	HRESULT GetPixelShaderConstant(DWORD, void *, DWORD) override { return D3D_OK; }
	HRESULT GetPixelShaderFunction(DWORD, void *, DWORD *) override { return E_FAIL; }

	HRESULT DrawRectPatch(UINT, const float *, const D3DRECTPATCH_INFO *) override { return D3D_OK; }
	HRESULT DrawTriPatch(UINT, const float *, const D3DTRIPATCH_INFO *) override { return D3D_OK; }
	HRESULT DeletePatch(UINT) override { return D3D_OK; }

	// ------- pipeline accessors (WebGLPipeline reads device state) -------
	DWORD getRenderState(DWORD st) const
	{
		return ((size_t)st < kMaxRenderStates) ? m_renderStates[st] : 0;
	}
	DWORD getStageState(DWORD stage, DWORD t) const
	{
		return (stage < kMaxTextureStages && (size_t)t < kMaxStageStates) ? m_stageStates[stage][t] : 0;
	}
	const D3DMATRIX &getTransform(DWORD t) const
	{
		return m_transforms[(size_t)t < kMaxTransforms ? t : 0];
	}
	const D3DVIEWPORT8 &getViewport() const { return m_viewport; }
	const D3DMATERIAL8 &getMaterial() const { return m_material; }
	bool isLightEnabled(unsigned i) const { return i < kMaxLights && m_lightEnabled[i]; }
	const D3DLIGHT8 &getLight(unsigned i) const { return m_lights[i < kMaxLights ? i : 0]; }
	unsigned getFVF() const { return m_fvf; }
	WebGLVertexBuffer *getStream0() const { return static_cast<WebGLVertexBuffer *>(m_streams[0]); }
	unsigned getStream0Stride() const { return m_streamStride[0]; }
	WebGLIndexBuffer *getIndices() const { return static_cast<WebGLIndexBuffer *>(m_indices); }
	unsigned getBaseVertexIndex() const { return m_baseVertexIndex; }
	WebGLTexture *getTexture2D(unsigned stage) const
	{
		if (stage >= kMaxTextureStages || !m_textures[stage]) return nullptr;
		if (m_textures[stage]->GetType() != D3DRTYPE_TEXTURE) return nullptr;
		return static_cast<WebGLTexture *>(m_textures[stage]);
	}

	static constexpr UINT kMaxLights = 8;

private:
	~WebGLDevice()
	{
		releaseBackBuffers();
		for (UINT i = 0; i < kMaxTextureStages; i++) {
			if (m_textures[i]) m_textures[i]->Release();
		}
		for (UINT i = 0; i < kMaxStreams; i++) {
			if (m_streams[i]) m_streams[i]->Release();
		}
		if (m_indices) m_indices->Release();
	}

	void createBackBuffers()
	{
		m_backBuffer = new WebGLSurface(this, m_pp.BackBufferWidth, m_pp.BackBufferHeight,
		                                m_pp.BackBufferFormat, D3DRTYPE_SURFACE);
		if (m_pp.EnableAutoDepthStencil) {
			m_depthStencil = new WebGLSurface(this, m_pp.BackBufferWidth, m_pp.BackBufferHeight,
			                                  m_pp.AutoDepthStencilFormat, D3DRTYPE_SURFACE);
		}
	}

	void releaseBackBuffers()
	{
		if (m_currentRT) { m_currentRT->Release(); m_currentRT = nullptr; }
		if (m_currentDS) { m_currentDS->Release(); m_currentDS = nullptr; }
		if (m_backBuffer) { m_backBuffer->Release(); m_backBuffer = nullptr; }
		if (m_depthStencil) { m_depthStencil->Release(); m_depthStencil = nullptr; }
	}

public:
	static constexpr size_t kMaxRenderStates = 256;
	static constexpr size_t kMaxTransforms = 300; // world matrices live at 256+
	static constexpr size_t kMaxStageStates = 33;
	static constexpr UINT kMaxTextureStages = 8;
	static constexpr UINT kMaxStreams = 16;
private:

	IDirect3D8 *m_parent;
	HWND m_focusWindow;
	DWORD m_behaviorFlags;
	D3DPRESENT_PARAMETERS m_pp = {};
	WebGLSurface *m_backBuffer = nullptr;
	WebGLSurface *m_depthStencil = nullptr;
	WebGLSurface *m_currentRT = nullptr;
	WebGLSurface *m_currentDS = nullptr;
	WINBOOL m_cursorShown = FALSE;

	D3DMATRIX m_transforms[kMaxTransforms] = {};
	D3DVIEWPORT8 m_viewport = {};
	D3DMATERIAL8 m_material = {};
	D3DLIGHT8 m_lights[kMaxLights] = {};
	WINBOOL m_lightEnabled[kMaxLights] = {};
	DWORD m_renderStates[kMaxRenderStates] = {};
	DWORD m_stageStates[kMaxTextureStages][kMaxStageStates] = {};
	IDirect3DBaseTexture8 *m_textures[kMaxTextureStages];
	IDirect3DVertexBuffer8 *m_streams[kMaxStreams];
	UINT m_streamStride[kMaxStreams];
	IDirect3DIndexBuffer8 *m_indices = nullptr;
	UINT m_baseVertexIndex = 0;
	DWORD m_fvf = 0;
};

// GetDevice implementations (need the complete WebGLDevice type).
HRESULT WebGLSurface::GetDevice(struct IDirect3DDevice8 **ppDevice)
{
	if (!ppDevice) return D3DERR_INVALIDCALL;
	m_device->AddRef();
	*ppDevice = m_device;
	return D3D_OK;
}
HRESULT WebGLTexture::GetDevice(struct IDirect3DDevice8 **ppDevice)
{
	if (!ppDevice) return D3DERR_INVALIDCALL;
	m_device->AddRef();
	*ppDevice = m_device;
	return D3D_OK;
}
HRESULT WebGLCubeTexture::GetDevice(struct IDirect3DDevice8 **ppDevice)
{
	if (!ppDevice) return D3DERR_INVALIDCALL;
	m_device->AddRef();
	*ppDevice = m_device;
	return D3D_OK;
}
HRESULT WebGLVolumeTexture::GetDevice(struct IDirect3DDevice8 **ppDevice)
{
	if (!ppDevice) return D3DERR_INVALIDCALL;
	m_device->AddRef();
	*ppDevice = m_device;
	return D3D_OK;
}
HRESULT WebGLVertexBuffer::GetDevice(struct IDirect3DDevice8 **ppDevice)
{
	if (!ppDevice) return D3DERR_INVALIDCALL;
	m_device->AddRef();
	*ppDevice = m_device;
	return D3D_OK;
}
HRESULT WebGLIndexBuffer::GetDevice(struct IDirect3DDevice8 **ppDevice)
{
	if (!ppDevice) return D3DERR_INVALIDCALL;
	m_device->AddRef();
	*ppDevice = m_device;
	return D3D_OK;
}

// ---------------------------------------------------------------------------
// Caps: a solid fixed-function DX8 part. No programmable shaders -> the engine
// takes its shipped 2-stage-combiner fallback paths (terrainShader2Stage etc.)
// which are exactly what the Phase 2 WebGL2 pipeline implements.
// ---------------------------------------------------------------------------

static void FillCaps(D3DCAPS8 *caps, UINT adapter)
{
	memset(caps, 0, sizeof(*caps));
	caps->DeviceType = D3DDEVTYPE_HAL;
	caps->AdapterOrdinal = adapter;

	caps->Caps2 = D3DCAPS2_DYNAMICTEXTURES | D3DCAPS2_FULLSCREENGAMMA;
	caps->PresentationIntervals = D3DPRESENT_INTERVAL_ONE | D3DPRESENT_INTERVAL_IMMEDIATE;
	caps->CursorCaps = D3DCURSORCAPS_COLOR;

	caps->DevCaps = D3DDEVCAPS_EXECUTESYSTEMMEMORY | D3DDEVCAPS_EXECUTEVIDEOMEMORY |
	                D3DDEVCAPS_TLVERTEXSYSTEMMEMORY | D3DDEVCAPS_TLVERTEXVIDEOMEMORY |
	                D3DDEVCAPS_TEXTURESYSTEMMEMORY | D3DDEVCAPS_TEXTUREVIDEOMEMORY |
	                D3DDEVCAPS_DRAWPRIMTLVERTEX | D3DDEVCAPS_CANRENDERAFTERFLIP |
	                D3DDEVCAPS_TEXTURENONLOCALVIDMEM | D3DDEVCAPS_DRAWPRIMITIVES2 |
	                D3DDEVCAPS_DRAWPRIMITIVES2EX | D3DDEVCAPS_HWTRANSFORMANDLIGHT |
	                D3DDEVCAPS_HWRASTERIZATION;

	caps->PrimitiveMiscCaps = D3DPMISCCAPS_MASKZ | D3DPMISCCAPS_CULLNONE |
	                          D3DPMISCCAPS_CULLCW | D3DPMISCCAPS_CULLCCW |
	                          D3DPMISCCAPS_COLORWRITEENABLE | D3DPMISCCAPS_BLENDOP;

	caps->RasterCaps = D3DPRASTERCAPS_DITHER | D3DPRASTERCAPS_ZTEST |
	                   D3DPRASTERCAPS_FOGVERTEX | D3DPRASTERCAPS_FOGTABLE |
	                   D3DPRASTERCAPS_MIPMAPLODBIAS | D3DPRASTERCAPS_ZBIAS |
	                   D3DPRASTERCAPS_FOGRANGE | D3DPRASTERCAPS_ANISOTROPY |
	                   D3DPRASTERCAPS_WFOG | D3DPRASTERCAPS_ZFOG;

	caps->ZCmpCaps = D3DPCMPCAPS_NEVER | D3DPCMPCAPS_LESS | D3DPCMPCAPS_EQUAL |
	                 D3DPCMPCAPS_LESSEQUAL | D3DPCMPCAPS_GREATER | D3DPCMPCAPS_NOTEQUAL |
	                 D3DPCMPCAPS_GREATEREQUAL | D3DPCMPCAPS_ALWAYS;
	caps->AlphaCmpCaps = caps->ZCmpCaps;

	caps->SrcBlendCaps = D3DPBLENDCAPS_ZERO | D3DPBLENDCAPS_ONE | D3DPBLENDCAPS_SRCCOLOR |
	                     D3DPBLENDCAPS_INVSRCCOLOR | D3DPBLENDCAPS_SRCALPHA |
	                     D3DPBLENDCAPS_INVSRCALPHA | D3DPBLENDCAPS_DESTALPHA |
	                     D3DPBLENDCAPS_INVDESTALPHA | D3DPBLENDCAPS_DESTCOLOR |
	                     D3DPBLENDCAPS_INVDESTCOLOR | D3DPBLENDCAPS_SRCALPHASAT;
	caps->DestBlendCaps = caps->SrcBlendCaps;

	caps->ShadeCaps = D3DPSHADECAPS_COLORGOURAUDRGB | D3DPSHADECAPS_SPECULARGOURAUDRGB |
	                  D3DPSHADECAPS_ALPHAGOURAUDBLEND | D3DPSHADECAPS_FOGGOURAUD;

	caps->TextureCaps = D3DPTEXTURECAPS_PERSPECTIVE | D3DPTEXTURECAPS_ALPHA |
	                    D3DPTEXTURECAPS_MIPMAP | D3DPTEXTURECAPS_PROJECTED |
	                    D3DPTEXTURECAPS_CUBEMAP | D3DPTEXTURECAPS_MIPCUBEMAP;

	caps->TextureFilterCaps = D3DPTFILTERCAPS_MINFPOINT | D3DPTFILTERCAPS_MINFLINEAR |
	                          D3DPTFILTERCAPS_MINFANISOTROPIC |
	                          D3DPTFILTERCAPS_MIPFPOINT | D3DPTFILTERCAPS_MIPFLINEAR |
	                          D3DPTFILTERCAPS_MAGFPOINT | D3DPTFILTERCAPS_MAGFLINEAR;
	caps->CubeTextureFilterCaps = caps->TextureFilterCaps;
	caps->VolumeTextureFilterCaps = caps->TextureFilterCaps;

	caps->TextureAddressCaps = D3DPTADDRESSCAPS_WRAP | D3DPTADDRESSCAPS_MIRROR |
	                           D3DPTADDRESSCAPS_CLAMP | D3DPTADDRESSCAPS_BORDER |
	                           D3DPTADDRESSCAPS_INDEPENDENTUV;
	caps->VolumeTextureAddressCaps = caps->TextureAddressCaps;

	caps->LineCaps = D3DLINECAPS_TEXTURE | D3DLINECAPS_ZTEST | D3DLINECAPS_BLEND |
	                 D3DLINECAPS_ALPHACMP | D3DLINECAPS_FOG;

	caps->MaxTextureWidth = 4096;
	caps->MaxTextureHeight = 4096;
	caps->MaxVolumeExtent = 256;
	caps->MaxTextureRepeat = 8192;
	caps->MaxTextureAspectRatio = 4096;
	caps->MaxAnisotropy = 16;
	caps->MaxVertexW = 1e10f;

	caps->GuardBandLeft = -32768.0f;
	caps->GuardBandTop = -32768.0f;
	caps->GuardBandRight = 32768.0f;
	caps->GuardBandBottom = 32768.0f;

	caps->StencilCaps = D3DSTENCILCAPS_KEEP | D3DSTENCILCAPS_ZERO | D3DSTENCILCAPS_REPLACE |
	                    D3DSTENCILCAPS_INCRSAT | D3DSTENCILCAPS_DECRSAT | D3DSTENCILCAPS_INVERT |
	                    D3DSTENCILCAPS_INCR | D3DSTENCILCAPS_DECR;

	caps->FVFCaps = 8; // 8 texture coordinate sets
	caps->TextureOpCaps = D3DTEXOPCAPS_DISABLE | D3DTEXOPCAPS_SELECTARG1 | D3DTEXOPCAPS_SELECTARG2 |
	                      D3DTEXOPCAPS_MODULATE | D3DTEXOPCAPS_MODULATE2X | D3DTEXOPCAPS_MODULATE4X |
	                      D3DTEXOPCAPS_ADD | D3DTEXOPCAPS_ADDSIGNED | D3DTEXOPCAPS_ADDSIGNED2X |
	                      D3DTEXOPCAPS_SUBTRACT | D3DTEXOPCAPS_ADDSMOOTH |
	                      D3DTEXOPCAPS_BLENDDIFFUSEALPHA | D3DTEXOPCAPS_BLENDTEXTUREALPHA |
	                      D3DTEXOPCAPS_BLENDFACTORALPHA | D3DTEXOPCAPS_BLENDCURRENTALPHA |
	                      D3DTEXOPCAPS_DOTPRODUCT3;

	// Two stages: steers W3DShaderManager::getChipset() to the shipped
	// 2-stage fixed-function paths.
	caps->MaxTextureBlendStages = 2;
	caps->MaxSimultaneousTextures = 2;

	caps->VertexProcessingCaps = D3DVTXPCAPS_TEXGEN | D3DVTXPCAPS_MATERIALSOURCE7 |
	                             D3DVTXPCAPS_DIRECTIONALLIGHTS | D3DVTXPCAPS_POSITIONALLIGHTS |
	                             D3DVTXPCAPS_LOCALVIEWER;
	caps->MaxActiveLights = 8;
	caps->MaxUserClipPlanes = 1;
	caps->MaxVertexBlendMatrices = 0;
	caps->MaxVertexBlendMatrixIndex = 0;

	caps->MaxPointSize = 64.0f;
	caps->MaxPrimitiveCount = 0x000FFFFF;
	caps->MaxVertexIndex = 0x000FFFFF;
	caps->MaxStreams = 16;
	caps->MaxStreamStride = 256;

	// No programmable shaders: the engine's ps.1.1 water/terrain paths are
	// gated on these and fall back to fixed function.
	caps->VertexShaderVersion = 0;
	caps->MaxVertexShaderConst = 0;
	caps->PixelShaderVersion = 0;
	caps->MaxPixelShaderValue = 0.0f;
}

// ---------------------------------------------------------------------------
// IDirect3D8
// ---------------------------------------------------------------------------

class WebGLDirect3D8 final : public IDirect3D8
{
public:
	WebGLDirect3D8() = default;

	D3D8GLES_IUNKNOWN_IMPL(WebGLDirect3D8)

	HRESULT RegisterSoftwareDevice(void *) override { return D3D_OK; }
	UINT GetAdapterCount() override { return 1; }

	HRESULT GetAdapterIdentifier(UINT adapter, DWORD, D3DADAPTER_IDENTIFIER8 *pIdentifier) override
	{
		if (!pIdentifier || adapter != 0) return D3DERR_INVALIDCALL;
		memset(pIdentifier, 0, sizeof(*pIdentifier));
		strncpy(pIdentifier->Driver, "d3d8gles", MAX_DEVICE_IDENTIFIER_STRING - 1);
		strncpy(pIdentifier->Description, "WebGL2 (d3d8gles)", MAX_DEVICE_IDENTIFIER_STRING - 1);
		// Generic vendor/device: W3DShaderManager::getChipset() must not match
		// any vendor-specific path.
		pIdentifier->VendorId = 0;
		pIdentifier->DeviceId = 0;
		return D3D_OK;
	}

	UINT GetAdapterModeCount(UINT adapter) override
	{
		return adapter == 0 ? kModeCount + 1 : 0;
	}

	HRESULT EnumAdapterModes(UINT adapter, UINT mode, D3DDISPLAYMODE *pMode) override
	{
		if (!pMode || adapter != 0 || mode >= kModeCount + 1) return D3DERR_INVALIDCALL;
		if (mode == 0) {
			// The native (canvas) mode MUST be enumerable: DX8Wrapper's
			// Find_Color_Mode requires an EXACT width/height match to accept a
			// 32-bit backbuffer format - without this entry the engine fell
			// back to BitDepth=16 and every texture in the game degraded to
			// R5G6B5/A4R4G4B4/A1R5G5B5 (killing most alpha channels).
			pMode->Width = (UINT)s_nativeModeW;
			pMode->Height = (UINT)s_nativeModeH;
		} else {
			pMode->Width = kModes[mode - 1][0];
			pMode->Height = kModes[mode - 1][1];
		}
		pMode->RefreshRate = 60;
		pMode->Format = D3DFMT_X8R8G8B8;
		return D3D_OK;
	}

	HRESULT GetAdapterDisplayMode(UINT adapter, D3DDISPLAYMODE *pMode) override
	{
		if (!pMode || adapter != 0) return D3DERR_INVALIDCALL;
		pMode->Width = (UINT)s_nativeModeW;
		pMode->Height = (UINT)s_nativeModeH;
		pMode->RefreshRate = 60;
		pMode->Format = D3DFMT_X8R8G8B8;
		return D3D_OK;
	}

	// Set from WebMain before the device is created (see d3d8gles_set_native_mode).
	static int s_nativeModeW;
	static int s_nativeModeH;

	HRESULT CheckDeviceType(UINT adapter, D3DDEVTYPE, D3DFORMAT, D3DFORMAT, WINBOOL) override
	{
		return adapter == 0 ? D3D_OK : D3DERR_INVALIDCALL;
	}

	HRESULT CheckDeviceFormat(UINT adapter, D3DDEVTYPE, D3DFORMAT, DWORD, D3DRESOURCETYPE,
	                          D3DFORMAT checkFormat) override
	{
		if (adapter != 0) return D3DERR_INVALIDCALL;
		switch (checkFormat) {
		case D3DFMT_A8R8G8B8:
		case D3DFMT_X8R8G8B8:
		case D3DFMT_R5G6B5:
		case D3DFMT_X1R5G5B5:
		case D3DFMT_A1R5G5B5:
		case D3DFMT_A4R4G4B4:
		case D3DFMT_A8:
		case D3DFMT_L8:
		case D3DFMT_A8L8:
		case D3DFMT_DXT1:
		case D3DFMT_DXT2:
		case D3DFMT_DXT3:
		case D3DFMT_DXT4:
		case D3DFMT_DXT5:
		case D3DFMT_D16:
		case D3DFMT_D24S8:
		case D3DFMT_D24X8:
		case D3DFMT_D32:
			return D3D_OK;
		default:
			return D3DERR_NOTAVAILABLE;
		}
	}

	HRESULT CheckDeviceMultiSampleType(UINT adapter, D3DDEVTYPE, D3DFORMAT, WINBOOL,
	                                   D3DMULTISAMPLE_TYPE multiSampleType) override
	{
		if (adapter != 0) return D3DERR_INVALIDCALL;
		return multiSampleType == D3DMULTISAMPLE_NONE ? D3D_OK : D3DERR_NOTAVAILABLE;
	}

	HRESULT CheckDepthStencilMatch(UINT adapter, D3DDEVTYPE, D3DFORMAT, D3DFORMAT,
	                               D3DFORMAT depthStencilFormat) override
	{
		if (adapter != 0) return D3DERR_INVALIDCALL;
		switch (depthStencilFormat) {
		case D3DFMT_D16:
		case D3DFMT_D24S8:
		case D3DFMT_D24X8:
			return D3D_OK;
		default:
			return D3DERR_NOTAVAILABLE;
		}
	}

	HRESULT GetDeviceCaps(UINT adapter, D3DDEVTYPE, D3DCAPS8 *pCaps) override
	{
		if (!pCaps || adapter != 0) return D3DERR_INVALIDCALL;
		FillCaps(pCaps, adapter);
		return D3D_OK;
	}

	HMONITOR GetAdapterMonitor(UINT) override { return nullptr; }

	HRESULT CreateDevice(UINT adapter, D3DDEVTYPE, HWND focusWindow, DWORD behaviorFlags,
	                     D3DPRESENT_PARAMETERS *pp, struct IDirect3DDevice8 **ppReturnedDeviceInterface) override
	{
		if (!pp || !ppReturnedDeviceInterface || adapter != 0) return D3DERR_INVALIDCALL;
		// GeneralsX @build Android port 09/05/2026 AutoDepthStencilFormat is
		// what DX8Wrapper::Clear() keys its "does this device have a stencil
		// buffer?" decision on (via GetDepthStencilSurface()->GetDesc()), and
		// therefore whether D3DCLEAR_STENCIL is ever requested at all. It was
		// inferred to be D3DFMT_D24S8 from Find_Z_Mode's preference order --
		// print it instead of inferring it. 80 = D3DFMT_D24S8, 77 = D3DFMT_D16,
		// 79 = D3DFMT_D24X8, 71 = D3DFMT_D32, 0 = D3DFMT_UNKNOWN.
		fprintf(stderr, "[d3d8gles] CreateDevice %ux%u fmt=%d autoDS=%d dsFmt=%d "
		        "(Phase 0 null device)\n",
		        pp->BackBufferWidth, pp->BackBufferHeight, (int)pp->BackBufferFormat,
		        (int)pp->EnableAutoDepthStencil, (int)pp->AutoDepthStencilFormat);
		*ppReturnedDeviceInterface = new WebGLDevice(this, pp, focusWindow, behaviorFlags);
		return D3D_OK;
	}

private:
	static constexpr UINT kModeCount = 8;
	static constexpr UINT kModes[kModeCount][2] = {
		{800, 600}, {1024, 768}, {1280, 720}, {1280, 1024},
		{1600, 900}, {1920, 1080}, {2560, 1440}, {3840, 2160},
	};
};

constexpr UINT WebGLDirect3D8::kModes[WebGLDirect3D8::kModeCount][2];

int WebGLDirect3D8::s_nativeModeW = 1024;
int WebGLDirect3D8::s_nativeModeH = 768;

// Called from WebMain once the real canvas size is known (before GameMain
// creates the device), so mode enumeration matches the -xres/-yres the game
// will ask for.
extern "C" void d3d8gles_set_native_mode(int w, int h)
{
	if (w > 0 && h > 0) {
		WebGLDirect3D8::s_nativeModeW = w;
		WebGLDirect3D8::s_nativeModeH = h;
	}
}

// Forward a canvas/window resize to the WebGL pipeline so the backbuffer
// and viewport match the browser viewport (called from SDL3GameEngine's
// SDL_EVENT_WINDOW_RESIZED handler).
extern "C" void d3d8gles_resize(int w, int h)
{
	if (w > 0 && h > 0) {
		WebGLPipeline::get()->resize(w, h);
	}
}

// GeneralsX @feature XR port Phase 1.0 - external-context mode config, see
// d3d8gles.h. File-static, visible to gles_pipeline.cpp below (same TU).
static const D3D8GLES_XRConfig *g_xrConfig = nullptr;

extern "C" void d3d8gles_SetXRConfig(const D3D8GLES_XRConfig *cfg)
{
	g_xrConfig = cfg;
	fprintf(stderr, "[d3d8gles] XR config %s\n", cfg ? "set (external-context mode)" : "cleared");
}

extern "C" const D3D8GLES_XRConfig *d3d8gles_GetXRConfig()
{
	return g_xrConfig;
}

extern "C" unsigned int d3d8gles_GetGameTexture()
{
	return WebGLPipeline::get()->offscreenTexture();
}

extern "C" void d3d8gles_InvalidateCachedState()
{
	WebGLPipeline::get()->invalidateCachedGLState();
}
extern "C" void d3d8gles_BeginXRFrame(bool split,bool elideOrdinaryWorld) { WebGLPipeline::get()->beginXRFrame(split,elideOrdinaryWorld); }
extern "C" void d3d8gles_RequireXRFullWorld() {WebGLPipeline::get()->requireXRFullWorld();}
extern "C" void d3d8gles_SetShroudTexture(IDirect3DBaseTexture8 *texture,const float *worldToUV) {
	WebGLTexture *tex = texture && texture->GetType()==D3DRTYPE_TEXTURE ? static_cast<WebGLTexture *>(texture):nullptr;
	WebGLPipeline::get()->setShroudTexture(tex,worldToUV);
}
extern "C" int d3d8gles_ShroudFoldAvailable() {return WebGLPipeline::get()->shroudFoldAvailable() ? 1:0;}
extern "C" void d3d8gles_SetShroudFold(int active) {WebGLPipeline::get()->setShroudFold(active!=0);}
extern "C" unsigned d3d8gles_XROrdinarySkipped() {return WebGLPipeline::get()->xrOrdinarySkipped();}
extern "C" bool d3d8gles_BeginXRUI(bool elideWorldCopy) { return WebGLPipeline::get()->beginXRUI(elideWorldCopy); }
extern "C" bool d3d8gles_XRSplitReady() { return WebGLPipeline::get()->xrSplitReady(); }
extern "C" unsigned int d3d8gles_GetXRWorldTexture() { return WebGLPipeline::get()->xrWorldTexture(); }
extern "C" bool d3d8gles_BeginXRStereo(int w,int h,const float *l,const float *r,const float *b,float a,const float *c,bool atlas,bool multiview) { return WebGLPipeline::get()->beginXRStereo(w,h,l,r,b,a,c,atlas,multiview); }
extern "C" void d3d8gles_ConfigureXRMultiview(void *(*resolver)(const char *)) {WebGLPipeline::get()->configureXRMultiview(resolver);}
extern "C" bool d3d8gles_XRStereoMultiview() {return WebGLPipeline::get()->xrStereoMultiview();}
extern "C" bool d3d8gles_XRStereoAtlas() { return WebGLPipeline::get()->xrStereoAtlas(); }
extern "C" void d3d8gles_EndXRStereo() { WebGLPipeline::get()->endXRStereo(); }
extern "C" void d3d8gles_DrawXRDecorations(const float *vertices,int count) {WebGLPipeline::get()->drawXRDecorations(vertices,count);}
extern "C" unsigned int d3d8gles_XRStereoTexture(int eye) { return WebGLPipeline::get()->xrStereoTexture(eye); }
extern "C" unsigned int d3d8gles_GetXRUITexture() { return WebGLPipeline::get()->xrUITexture(); }

// ---------------------------------------------------------------------------
// Entry point. Named distinctly from the real Direct3DCreate8 (unlike the
// web port, which statically links this as the *only* D3D8 implementation
// in the binary) because on Android this coexists in the same libmain.so
// with DXVK's dlopen'd libdxvk_d3d8.so, which exports its own
// Direct3DCreate8 -- see DX8Wrapper::Init's backend switch in dx8wrapper.cpp.
// ---------------------------------------------------------------------------

extern "C" IDirect3D8 *WINAPI Direct3DCreate8_GLES(UINT sdkVersion)
{
	g_trace = getenv("D3D8GLES_TRACE") != nullptr;
	fprintf(stderr, "[d3d8gles] Direct3DCreate8_GLES(sdk=%u) - native GLES3 renderer\n", sdkVersion);
	return new WebGLDirect3D8();
}

// The pipeline lives in the same translation unit so it can touch the device
// and resource internals directly (see gles_pipeline.h for the interface).
#include "gles_pipeline.cpp"
