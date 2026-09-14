# d3d8gles - D3D8 -> native GLES3 renderer for Android (experimental)
# GeneralsX @build Android port GLES experiment
#
# Ported from Lolendor/Generals-WebAssembly's d3d8webgl (D3D8 -> WebGL2 for
# Emscripten). Implements the subset of the Direct3D 8 API the engine uses
# (see Core/Libraries/Source/WWVegas/WW3D2/dx8wrapper.cpp) on top of native
# GLES3 via SDL3's GL context API, as an alternative to DXVK's D3D8->Vulkan
# translation. Statically linked into libmain.so (unlike DXVK, which is
# dlopen'd at runtime) -- see DX8Wrapper::Init()'s backend switch.
#
# Android-only for now: this is an experimental branch, not wired into any
# other platform's build.

if(NOT ANDROID)
    return()
endif()

add_library(d3d8gles STATIC
    src/d3d8gles.cpp
    src/gles_dispatch.cpp
)

target_include_directories(d3d8gles PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_include_directories(d3d8gles PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# D3D8 headers come from the same DXVK checkout our own D3D8 code already
# uses (d3d8lib, see GeneralsMD/Code/CompatLib/CMakeLists.txt); Win32 shim
# types (HWND, WINAPI, DWORD, ...) come from CompatLib transitively through
# it. sdl3lib provides GLES3/gl3.h (Android NDK) and the SDL3 GL context API
# (SDL_GL_CreateContext/MakeCurrent/SwapWindow) this module's context setup
# uses instead of Emscripten's WebGL2 canvas API.
#
# GeneralsX @build Android port ANGLE experiment - deliberately NOT linked
# against system GLESv3 (unlike the original port). gl* symbols are instead
# defined by src/gles_dispatch.cpp, which forwards them to function pointers
# resolved at runtime via dlopen/dlsym against a chosen GLES implementation
# library -- ANGLE's bundled libGLESv2_angle.so (routes through its Vulkan
# backend) by default, or system libGLESv3.so as a fallback/A-B toggle (see
# d3d8gles_LoadGLESDispatch's caller in SDL3Main.cpp). Direct DT_NEEDED
# linking against libGLESv3.so can't be repointed at a same-named bundled
# library -- Android's linker namespace reserves that name for the system
# library -- so an explicit dispatch table is the only way to route these
# calls through an app-bundled, differently-named implementation.
target_link_libraries(d3d8gles PRIVATE d3d8lib sdl3lib)

target_compile_features(d3d8gles PRIVATE cxx_std_20)
