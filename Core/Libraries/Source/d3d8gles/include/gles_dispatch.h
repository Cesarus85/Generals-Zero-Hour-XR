// GeneralsX @build Android port ANGLE integration
// Auto-generated dispatch shim: forwards the 77 gl* entry points d3d8gles
// actually calls to function pointers resolved from a dlopen'd GLES
// implementation (ANGLE's libGLESv2_angle.so, or a system libGLESv3.so
// fallback), instead of linking directly against system libGLESv3.so.
// Android's linker namespace won't let an app-bundled library override a
// public system library of the same name via implicit DT_NEEDED linking,
// so ANGLE's shared libraries must be dlopen'd explicitly under their own
// distinct names (libEGL_angle.so / libGLESv2_angle.so) and every GL entry
// point resolved through this shim instead.
#pragma once

// Loads the given GLES implementation library (by soname, resolved via the
// normal dlopen search path -- e.g. "libGLESv2_angle.so" for the ANGLE
// build bundled in jniLibs) and resolves all gl* pointers used below from
// it. Must be called after the GL context is current (SDL_GL_MakeCurrent)
// and before any gl* call. Returns false (leaving the shim unresolved) if
// the library or any required symbol can't be found -- callers should fall
// back to linking against system GLESv3 in that case.
bool d3d8gles_LoadGLESDispatch(const char *libName);

