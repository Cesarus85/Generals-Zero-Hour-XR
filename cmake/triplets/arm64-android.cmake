# Overlay triplet: pin the Android API level so vcpkg-built static libs
# (freetype, curl, openssl, ffmpeg, ...) match the engine's ANDROID_PLATFORM
# (android-28, see the android-vulkan preset). Without the pin, vcpkg builds
# against the community triplet's default API level and the final link can
# pull in symbols missing on the oldest supported devices (Redmi Note 8 Pro
# tops out at Android 11 stock; Android 9 = API 28 keeps headroom for LineageOS).
# Requires ANDROID_NDK_HOME in the environment (vcpkg reads it to find the NDK).
set(VCPKG_TARGET_ARCHITECTURE arm64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)
set(VCPKG_CMAKE_SYSTEM_NAME Android)
set(VCPKG_MAKE_BUILD_TRIPLET "--host=aarch64-linux-android")
set(VCPKG_CMAKE_CONFIGURE_OPTIONS -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-28 -DANDROID_STL=c++_shared)
