# Overlay triplet: pin the iOS deployment target so vcpkg-built static libs
# (freetype, curl, openal, ...) match the engine's CMAKE_OSX_DEPLOYMENT_TARGET.
# Without this, vcpkg builds against the host SDK's default (e.g. iOS 26.5) and
# the final link warns "built for newer iOS version than being linked (16.0)" —
# and unguarded newer-API usage inside those libs can crash on older devices.
set(VCPKG_TARGET_ARCHITECTURE arm64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)
set(VCPKG_CMAKE_SYSTEM_NAME iOS)
set(VCPKG_OSX_DEPLOYMENT_TARGET "16.0")
set(VCPKG_OSX_ARCHITECTURES arm64)
