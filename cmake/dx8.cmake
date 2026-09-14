# DirectX 8 headers and rendering backend selection
# GeneralsX @build BenderAI 10/02/2026 - Session 18
# Fighter19's approach: Fetch ONE OR THE OTHER, never both
#
# On Windows: Use min-dx8-sdk (minimal Windows DirectX headers + libs)
# On Linux:   Use DXVK native pre-built tarball (DirectX→Vulkan translation)
# On macOS:   Build DXVK from source using Meson + MoltenVK (DirectX→Metal bridge)
#
# CRITICAL: Mixing headers causes conflicts - dx8-src has incomplete types,
# DXVK has full DirectX8+Wine headers. Compiler picks first path = wrong headers.
#
# macOS DXVK build (Session 61, 24/02/2026):
#   DXVK 2.6 builds natively on macOS arm64 via its "native" build mode.
#   macOS fixes are maintained in the DXVK fork history consumed by this build.
#   This project no longer applies local patch scripts during configure/build.
#
# Reference: docs/WORKDIR/lessons/2026-02-LESSONS.md (historical patch rationale)

set(DXVK_VERSION "v2.6")

if(SAGE_USE_DX8)
  # Windows: Fetch min-dx8-sdk for native DirectX 8
  FetchContent_Declare(
    dx8
    GIT_REPOSITORY https://github.com/TheSuperHackers/min-dx8-sdk.git
    GIT_TAG        7bddff8c01f5fb931c3cb73d4aa8e66d303d97bc
  )
  FetchContent_MakeAvailable(dx8)
  message(STATUS "Using DirectX 8 SDK (Windows native)")

elseif(APPLE AND SAGE_USE_MOLTENVK)
  # macOS: Build DXVK 2.6 from source using Meson + MoltenVK
  # GeneralsX @build BenderAI 24/02/2026 - Phase 5 macOS port (Session 61)
  find_program(MESON_EXECUTABLE meson HINTS /usr/local/bin /opt/homebrew/bin)
  find_program(NINJA_EXECUTABLE ninja HINTS /usr/local/bin /opt/homebrew/bin)

  if(NOT MESON_EXECUTABLE)
    message(FATAL_ERROR "DXVK macOS build requires meson: brew install meson")
  endif()
  if(NOT NINJA_EXECUTABLE)
    message(FATAL_ERROR "DXVK macOS build requires ninja: brew install ninja")
  endif()

  # Detect host architecture so Clang targets the correct slice.
  # IMPORTANT: prefer CMAKE_OSX_ARCHITECTURES (set by the preset) over uname -m.
  # On Apple Silicon Macs running CMake / meson via Rosetta, uname -m returns
  # x86_64 even though the native executable arch is arm64. Using CMAKE_OSX_ARCHITECTURES
  # (e.g. "arm64" from the macos-vulkan preset) avoids building an x86_64 dylib that
  # the arm64 game binary cannot dlopen.
  if(CMAKE_OSX_ARCHITECTURES)
    # Use the first entry (handles "arm64;x86_64" fat-binary requests too)
    list(GET CMAKE_OSX_ARCHITECTURES 0 DXVK_HOST_ARCH)
  else()
    execute_process(
      COMMAND uname -m
      OUTPUT_VARIABLE DXVK_HOST_ARCH
      OUTPUT_STRIP_TRAILING_WHITESPACE
    )
  endif()
  message(STATUS "Building DXVK ${DXVK_VERSION} for macOS/${DXVK_HOST_ARCH} with Meson (${MESON_EXECUTABLE})")

  include(ExternalProject)
  # GeneralsX @build BenderAI 13/03/2026 Add explicit source mode to keep remote branch updates deterministic by default.
  set(DXVK_LOCAL_FORK_DIR "${CMAKE_SOURCE_DIR}/references/fbraz3-dxvk")
  option(SAGE_DXVK_USE_LOCAL_FORK "Build DXVK from local references/fbraz3-dxvk checkout" OFF)

  if(SAGE_DXVK_USE_LOCAL_FORK AND EXISTS "${DXVK_LOCAL_FORK_DIR}/.git")
    set(DXVK_SOURCE_DIR "${DXVK_LOCAL_FORK_DIR}")
    message(STATUS "DXVK macOS build: using local fork source at ${DXVK_SOURCE_DIR}")
    # iOS needs Patches/dxvk-ios.patch (bundle-relative MoltenVK dlopen + SDL3
    # drawable fixes) — apply it idempotently: skip when the working tree
    # already carries it (reverse-check passes), fail the configure otherwise
    # so an unpatched DXVK can never ship silently.
    if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
      execute_process(
        COMMAND git -C "${DXVK_LOCAL_FORK_DIR}" apply --reverse --check "${CMAKE_SOURCE_DIR}/Patches/dxvk-ios.patch"
        RESULT_VARIABLE DXVK_PATCH_ALREADY_APPLIED
        ERROR_QUIET)
      if(NOT DXVK_PATCH_ALREADY_APPLIED EQUAL 0)
        execute_process(
          COMMAND git -C "${DXVK_LOCAL_FORK_DIR}" apply "${CMAKE_SOURCE_DIR}/Patches/dxvk-ios.patch"
          RESULT_VARIABLE DXVK_PATCH_RESULT)
        if(NOT DXVK_PATCH_RESULT EQUAL 0)
          message(FATAL_ERROR "Failed to apply Patches/dxvk-ios.patch to references/fbraz3-dxvk — the iOS DXVK build requires it.")
        endif()
        message(STATUS "DXVK iOS: applied Patches/dxvk-ios.patch")
      else()
        message(STATUS "DXVK iOS: Patches/dxvk-ios.patch already applied")
      endif()
    endif()
  elseif(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    # The remote clone has no way to receive the iOS patch; a silent fallback
    # here previously produced dylibs that die at Vulkan init on device.
    message(FATAL_ERROR "iOS DXVK requires the local fork submodule. Run: git submodule update --init references/fbraz3-dxvk")
  else()
    set(DXVK_SOURCE_DIR "${CMAKE_BINARY_DIR}/_deps/dxvk-src-fbraz3")
    message(STATUS "DXVK macOS build: using GitHub source clone at ${DXVK_SOURCE_DIR}")
  endif()
  set(DXVK_BUILD_DIR  "${CMAKE_BINARY_DIR}/_deps/dxvk-build-macos")
  set(DXVK_D3D8_LIB  "${DXVK_BUILD_DIR}/src/d3d8/libdxvk_d3d8.0.dylib")
  set(DXVK_D3D9_LIB  "${DXVK_BUILD_DIR}/src/d3d9/libdxvk_d3d9.0.dylib")

  # Detect Vulkan SDK location for Meson configuration.
  # VULKAN_SDK must point to the platform subdir (e.g. ~/VulkanSDK/1.4.x/macOS)
  # where lib/libvulkan.dylib and lib/libMoltenVK.dylib live.
  # GeneralsX @build BenderAI 03/03/2026: Normalize env path to macOS platform subdir
  set(VULKAN_SDK_ENV "$ENV{VULKAN_SDK}")

  # If VULKAN_SDK points to the version root (has macOS/ subdir), normalize it
  if(VULKAN_SDK_ENV AND EXISTS "${VULKAN_SDK_ENV}/macOS/lib/libMoltenVK.dylib")
    set(VULKAN_SDK_ENV "${VULKAN_SDK_ENV}/macOS")
    message(STATUS "DXVK macOS build: Normalized VULKAN_SDK to platform subdir: ${VULKAN_SDK_ENV}")
  endif()

  if(NOT VULKAN_SDK_ENV OR NOT EXISTS "${VULKAN_SDK_ENV}/lib/libMoltenVK.dylib")
    # Try home directory: look for ~/VulkanSDK/*/macOS
    file(GLOB VULKAN_HOME_DIRS "$ENV{HOME}/VulkanSDK/*/macOS")
    if(VULKAN_HOME_DIRS)
      list(SORT VULKAN_HOME_DIRS)
      list(REVERSE VULKAN_HOME_DIRS)
      list(GET VULKAN_HOME_DIRS 0 POTENTIAL_SDK)
      if(EXISTS "${POTENTIAL_SDK}/lib/libMoltenVK.dylib")
        set(VULKAN_SDK_ENV "${POTENTIAL_SDK}")
      endif()
    endif()
  endif()

  if(NOT VULKAN_SDK_ENV OR NOT EXISTS "${VULKAN_SDK_ENV}/lib/libMoltenVK.dylib")
    # Try common Homebrew locations
    foreach(BREW_PATH "/usr/local/Caskroom/vulkan-sdk/latest/VulkanSDK/macOS" "/opt/homebrew/Caskroom/vulkan-sdk/latest/VulkanSDK/macOS")
      if(EXISTS "${BREW_PATH}/lib/libMoltenVK.dylib")
        set(VULKAN_SDK_ENV "${BREW_PATH}")
        break()
      endif()
    endforeach()
  endif()

  if(VULKAN_SDK_ENV AND EXISTS "${VULKAN_SDK_ENV}/lib/libMoltenVK.dylib")
    message(STATUS "DXVK macOS build: Using Vulkan SDK at ${VULKAN_SDK_ENV}")
    set(VULKAN_SDK_ENV_VAR "VULKAN_SDK=${VULKAN_SDK_ENV}")
  else()
    message(WARNING "DXVK macOS build: Vulkan SDK / MoltenVK not found; Meson will search system paths")
    if(VULKAN_SDK_ENV)
      message(STATUS "  VULKAN_SDK checked: ${VULKAN_SDK_ENV}")
    endif()
    set(VULKAN_SDK_ENV_VAR "")
  endif()

  # iOS cross-compiles DXVK with a meson cross file (iPhoneOS sysroot); macOS uses
  # the native file. Arch/sysroot flags come from the machine file in both cases.
  if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    # The cross file is generated from a template so the iPhoneOS SDK path comes
    # from xcrun (Xcode-beta / renamed installs) instead of a hardcoded Xcode.app.
    execute_process(COMMAND xcrun --sdk iphoneos --show-sdk-path
                    OUTPUT_VARIABLE IOS_SDK OUTPUT_STRIP_TRAILING_WHITESPACE
                    COMMAND_ERROR_IS_FATAL ANY)
    configure_file(${CMAKE_SOURCE_DIR}/cmake/meson-arm64-ios-cross.ini.in
                   ${CMAKE_BINARY_DIR}/meson-arm64-ios-cross.ini @ONLY)
    set(DXVK_MESON_MACHINE_ARGS --cross-file ${CMAKE_BINARY_DIR}/meson-arm64-ios-cross.ini)
  else()
    set(DXVK_MESON_MACHINE_ARGS --native-file ${CMAKE_SOURCE_DIR}/cmake/meson-arm64-native.ini)
  endif()

  # Generate a pkg-config file for the in-tree (FetchContent) SDL3 so meson's
  # dependency('SDL3') resolves to it. Without this, meson silently falls back to a
  # system SDL2 (e.g. Homebrew) and compiles the WSI as Sdl2WsiDriver, which cannot
  # drive the SDL3 window the game creates (D3D device creation then fails at runtime).
  set(DXVK_SDL3_PC_DIR "${CMAKE_BINARY_DIR}/sdl3-pkgconfig")
  file(WRITE "${DXVK_SDL3_PC_DIR}/sdl3.pc"
"prefix=${CMAKE_BINARY_DIR}/_deps
libdir=\${prefix}/sdl3-build
includedir=\${prefix}/sdl3-src/include

Name: sdl3
Description: Simple DirectMedia Layer (in-tree FetchContent build)
Version: 3.4.2
Libs: -L\${libdir} -lSDL3
Cflags: -I\${includedir}
")
  if(DEFINED ENV{PKG_CONFIG_PATH})
    set(DXVK_PKG_CONFIG_PATH "${DXVK_SDL3_PC_DIR}:$ENV{PKG_CONFIG_PATH}")
  else()
    set(DXVK_PKG_CONFIG_PATH "${DXVK_SDL3_PC_DIR}")
  endif()
  set(DXVK_PKG_CONFIG_ENV "PKG_CONFIG_PATH=${DXVK_PKG_CONFIG_PATH}")

  if(SAGE_DXVK_USE_LOCAL_FORK AND EXISTS "${DXVK_LOCAL_FORK_DIR}/.git")
    ExternalProject_Add(dxvk_macos_build
      # GeneralsX @build BenderAI 13/03/2026 Build from local fbraz3 fork to avoid stale remote hash pins.
      SOURCE_DIR        ${DXVK_SOURCE_DIR}
      BINARY_DIR        ${DXVK_BUILD_DIR}
      DOWNLOAD_COMMAND  ""
      UPDATE_COMMAND    ""
      PATCH_COMMAND     ""
      CONFIGURE_COMMAND ${CMAKE_COMMAND} -E env CC=clang CXX=clang++ "CFLAGS=-arch ${DXVK_HOST_ARCH} -mcpu=apple-m1" "CXXFLAGS=-arch ${DXVK_HOST_ARCH} -mcpu=apple-m1" "LDFLAGS=-arch ${DXVK_HOST_ARCH}" "${DXVK_PKG_CONFIG_ENV}" ${VULKAN_SDK_ENV_VAR} ${MESON_EXECUTABLE} setup ${DXVK_BUILD_DIR} ${DXVK_SOURCE_DIR} ${DXVK_MESON_MACHINE_ARGS} -Ddxvk_native_wsi=sdl3 --buildtype=release --reconfigure
      BUILD_COMMAND     ${NINJA_EXECUTABLE} -C ${DXVK_BUILD_DIR} src/d3d9/libdxvk_d3d9.0.dylib src/d3d8/libdxvk_d3d8.0.dylib
      INSTALL_COMMAND   ""
      UPDATE_DISCONNECTED TRUE
    )
  else()
    # GeneralsX @build copilot 01/04/2026 Pin remote DXVK to immutable commit produced by fix/macos-size_t-cstddef.
    set(DXVK_REMOTE_REF 46a3bc018bcae408d49d3c500e4e536a11f6789a)
    ExternalProject_Add(dxvk_macos_build
      # GeneralsX @build BenderAI 08/04/2026 Consume pre-patched source from pinned fork commit.
      GIT_REPOSITORY    https://github.com/fbraz3/dxvk.git
      GIT_TAG           ${DXVK_REMOTE_REF}
      # GeneralsX @build copilot 01/04/2026 Keep pinned commit fetch reliable across clean CI builds.
      GIT_SHALLOW       FALSE
      SOURCE_DIR        ${DXVK_SOURCE_DIR}
      BINARY_DIR        ${DXVK_BUILD_DIR}
      PATCH_COMMAND     ""
      CONFIGURE_COMMAND ${CMAKE_COMMAND} -E env CC=clang CXX=clang++ "CFLAGS=-arch ${DXVK_HOST_ARCH} -mcpu=apple-m1" "CXXFLAGS=-arch ${DXVK_HOST_ARCH} -mcpu=apple-m1" "LDFLAGS=-arch ${DXVK_HOST_ARCH}" "${DXVK_PKG_CONFIG_ENV}" ${VULKAN_SDK_ENV_VAR} ${MESON_EXECUTABLE} setup ${DXVK_BUILD_DIR} ${DXVK_SOURCE_DIR} ${DXVK_MESON_MACHINE_ARGS} -Ddxvk_native_wsi=sdl3 --buildtype=release --reconfigure
      BUILD_COMMAND     ${NINJA_EXECUTABLE} -C ${DXVK_BUILD_DIR} src/d3d9/libdxvk_d3d9.0.dylib src/d3d8/libdxvk_d3d8.0.dylib
      INSTALL_COMMAND   ""
      UPDATE_DISCONNECTED FALSE
    )
  endif()

  # Copy libdxvk_d3d9 + libdxvk_d3d8 to build dir and create unversioned symlinks.
  # d3d8 links against d3d9 via @rpath, so both must be present at runtime.
  add_custom_command(
    OUTPUT  "${CMAKE_BINARY_DIR}/libdxvk_d3d9.0.dylib"
            "${CMAKE_BINARY_DIR}/libdxvk_d3d8.0.dylib"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
              ${DXVK_D3D9_LIB} "${CMAKE_BINARY_DIR}/libdxvk_d3d9.0.dylib"
    COMMAND ${CMAKE_COMMAND} -E create_symlink
              libdxvk_d3d9.0.dylib "${CMAKE_BINARY_DIR}/libdxvk_d3d9.dylib"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
              ${DXVK_D3D8_LIB} "${CMAKE_BINARY_DIR}/libdxvk_d3d8.0.dylib"
    COMMAND ${CMAKE_COMMAND} -E create_symlink
              libdxvk_d3d8.0.dylib "${CMAKE_BINARY_DIR}/libdxvk_d3d8.dylib"
    DEPENDS dxvk_macos_build
    COMMENT "Installing libdxvk_d3d8 + libdxvk_d3d9 to build directory"
  )
  add_custom_target(dxvk_d3d8_install ALL
    DEPENDS "${CMAKE_BINARY_DIR}/libdxvk_d3d8.0.dylib"
            "${CMAKE_BINARY_DIR}/libdxvk_d3d9.0.dylib"
  )

  # Export path so other cmake files know where the headers are
  set(DXVK_INCLUDE_DIR "${DXVK_SOURCE_DIR}/include/native" CACHE PATH "DXVK native headers")
  # GeneralsX @build felipebraz 10/06/2025 Mirror lowercase dxvk_SOURCE_DIR that FetchContent sets on Linux
  # so CompatLib/CMakeLists.txt check works on macOS as well (CACHE PATH survives auto-regeneration)
  set(dxvk_SOURCE_DIR "${DXVK_SOURCE_DIR}" CACHE PATH "DXVK source directory (macOS)")
  message(STATUS "DXVK source directory: ${DXVK_SOURCE_DIR}")
  message(STATUS "DXVK d3d8 library:     ${DXVK_D3D8_LIB}")

elseif(ANDROID)
  # Android: Build DXVK 2.6 from source using Meson + the NDK clang cross toolchain.
  # GeneralsX @build Android port 06/07/2026
  # Unlike iOS there is no MoltenVK layer: Android speaks Vulkan natively, so the
  # chain is D3D8 -> DXVK -> Vulkan -> vendor driver (Adreno/Mali). DXVK's loader
  # already tries "libvulkan.so" first on non-Apple Unix, which is exactly the
  # system Vulkan loader every Vulkan-capable Android device ships.
  find_program(MESON_EXECUTABLE meson)
  find_program(NINJA_EXECUTABLE ninja)

  if(NOT MESON_EXECUTABLE)
    message(FATAL_ERROR "DXVK Android build requires meson (pip install meson / brew install meson / apt install meson)")
  endif()
  if(NOT NINJA_EXECUTABLE)
    message(FATAL_ERROR "DXVK Android build requires ninja")
  endif()

  include(ExternalProject)
  set(DXVK_LOCAL_FORK_DIR "${CMAKE_SOURCE_DIR}/references/fbraz3-dxvk")
  option(SAGE_DXVK_USE_LOCAL_FORK "Build DXVK from local references/fbraz3-dxvk checkout" OFF)

  if(NOT (SAGE_DXVK_USE_LOCAL_FORK AND EXISTS "${DXVK_LOCAL_FORK_DIR}/.git"))
    # Android needs local patches (unversioned .so naming for APK packaging);
    # a remote clone cannot receive them, and a silent fallback would produce
    # libraries the app's linker namespace can never resolve.
    message(FATAL_ERROR "Android DXVK requires the local fork submodule. Run: git submodule update --init references/fbraz3-dxvk")
  endif()
  set(DXVK_SOURCE_DIR "${DXVK_LOCAL_FORK_DIR}")
  message(STATUS "DXVK Android build: using local fork source at ${DXVK_SOURCE_DIR}")

  # DXVK's own nested submodules must be present: SPIRV-Headers and
  # Vulkan-Headers (meson aborts with "Missing SPIRV-Headers" otherwise; on
  # macOS the Vulkan SDK papered over this, Android has no such fallback).
  if(NOT EXISTS "${DXVK_LOCAL_FORK_DIR}/include/spirv/include/spirv/unified1/spirv.hpp")
    message(FATAL_ERROR "DXVK Android build: nested submodules missing. Run: git -C ${DXVK_LOCAL_FORK_DIR} submodule update --init --depth 1")
  endif()

  # glslangValidator compiles DXVK's GLSL blit/present shaders to SPIR-V at
  # build time (host tool; apt install glslang-tools / brew install glslang).
  find_program(GLSLANG_EXECUTABLE NAMES glslangValidator glslang)
  if(NOT GLSLANG_EXECUTABLE)
    message(FATAL_ERROR "DXVK Android build requires glslangValidator (apt install glslang-tools / brew install glslang)")
  endif()

  # Apply the Android + iOS + Vulkan 1.1 patch set idempotently (skip when the
  # working tree already carries a patch, fail the configure when apply
  # fails, so an unpatched DXVK can never ship silently):
  #  - dxvk-android.patch: unversioned .so names (APK/jniLibs cannot carry the
  #    versioned-name + symlink layout meson emits for desktop Unix)
  #  - dxvk-ios.patch: SDL3 WSI pixel-size fix (platform-neutral, wanted on
  #    Android high-density displays; its Apple loader entries are compiled out)
  #  - dxvk-vulkan11-adaptive.patch: lowers the hard Vulkan floor from 1.3 to
  #    1.1, falling back to the pre-1.3 KHR/EXT extensions on adapters that
  #    only report Vulkan 1.1/1.2 (Mali, Unisoc, PowerVR) while leaving >=1.3
  #    adapters (including Adreno via Turnip driver injection) unaffected —
  #    see issue #5 ("DxvkAdapter: Failed to create device" on Unisoc)
  #  - dxvk-resource-refcount-memory-order.patch: DxvkResourceAllocation's
  #    incRef/decRef used memory_order_acquire on both, with no release —
  #    a no-op on x86 (DXVK's dev/test target, where every RMW is already a
  #    full fence) but on ARM's weaker model it lets a thread recycle an
  #    allocation before another thread's just-prior writes to it are
  #    visible. Matches the exact corruption real devices reported
  #    ("DxvkResourceAllocationPool: corrupted free list head") — see
  #    issues #2, #9, #11.
  #  - dxvk-mali-clip-distance.patch: the D3D9 backend requested
  #    shaderClipDistance/shaderCullDistance unconditionally and the DXSO
  #    compiler declared SPIR-V's ClipDistance capability on every vertex
  #    shader regardless of device support — true on desktop/Adreno, false
  #    on Mali G57, confirmed via Vulkan validation layers as the exact
  #    VUID-VkShaderModuleCreateInfo-pCode-08740 error preceding the
  #    libGLES_mali.so SIGSEGV on issue #9's device. Now gated on whether
  #    the device actually reports the feature.
  #  - dxvk-mali-g76-robustness2-optional.patch: VK_EXT_robustness2 was
  #    hard DxvkExtMode::Required — fine on desktop/Adreno, but Mali-G76
  #    (Redmi Note 8 Pro) reports Vulkan 1.1 with no robustness2 at all,
  #    so vkCreateDevice refused outright ("required extension(s) missing").
  #    The two features this fork leans on already survive without it on
  #    macOS (MoltenVK doesn't support it either), and everything else that
  #    reads them already treats them as optional. Now Optional, same as
  #    every other extension this project doesn't unconditionally require.
  # dxvk-android-missing-fallback-extensions.patch: the sub-1.2/1.3 KHR/EXT
  # fallback extensions (timelineSemaphore, vulkanMemoryModel, dynamicRendering,
  # maintenance4, synchronization2, hostQueryReset) were declared and queried
  # but never registered in DxvkAdapter::getExtensionList(), so vkCreateDevice
  # never actually enabled them despite the feature bits being reported as
  # supported -- root cause of the Mali-G76 (Redmi Note 8 Pro) timeline
  # semaphore VUID crash found after the robustness2-optional fix.
  # dxvk-mali-g76-legacy-barrier-fallback.patch: root cause of the Mali-G76
  # SIGSEGV, confirmed via a real Android tombstone (root access): the driver
  # has no VK_KHR_synchronization2 at all (apiVersion 1.1, extension absent),
  # so vkCmdPipelineBarrier2/vkCmdSetEvent2/vkQueueSubmit2 (and their KHR
  # aliases) all resolved to nullptr, and DxvkBarrierBatch::flush's
  # unconditional call crashed with a null-pointer-call SIGSEGV on the
  # dxvk-cs thread -- not a bug in DxvkBarrierTracker (that address was a
  # bystander symbol from our own crash handler's PC==0 blind spot, since
  # fixed separately in AndroidCrashHandler.cpp). This adds a legacy
  # vkCmdPipelineBarrier/vkCmdSetEvent/vkQueueSubmit fallback (with a
  # VkPipelineStageFlags2/VkAccessFlags2 -> legacy 32-bit conversion) for
  # when synchronization2 isn't available, the same thing every
  # mobile-shipping game engine (including this exact device's copy of Call
  # of Duty Mobile) already does instead of depending on it.
  # dxvk-mali-g76-semaphore-fn-fallback.patch: same class of bug as the
  # barrier fallback, one layer down -- vkResetQueryPool/vkGetSemaphore-
  # CounterValue/vkSignalSemaphore/vkWaitSemaphores are promoted-in-1.2 core
  # functions with no KHR/EXT fallback, so they resolved to nullptr on this
  # 1.1-only device too. Found via a real Android tombstone after the
  # barrier fix got device init past its previous crash point:
  # DxvkSubmissionQueue::finishCmdLists's vkWaitSemaphores call, null-pointer
  # SIGSEGV on the dxvk-queue thread.
  # dxvk-mali-g76-4444-format.patch: D3DFMT_A4R4G4B4/X4R4G4B4 mapped to
  # VK_FORMAT_A4R4G4B4_UNORM_PACK16, core-only since Vulkan 1.3. Registering
  # VK_EXT_4444_formats (its sub-1.3 backport) wasn't enough on Mali-G76 --
  # the driver still reports the format unusable for actual image creation
  # even with the extension enabled, so CreateTexture kept silently failing
  # (tombstone-confirmed SIGSEGV in WaterRenderObjClass::init, which locks a
  # surface DXVK never created). Remapped to the same universally-supported
  # 8-bit BGRA format already used for A8R8G8B8/X8R8G8B8 instead -- used
  # pervasively (fonts, radar, shroud, thumbnails, water's white
  # placeholder), none of which notice the extra memory.
  # dxvk-mali-g76-copy-commands2.patch: VK_KHR_copy_commands2 (needed for
  # vkCmdCopyBufferToImage2/vkCmdCopyImage2/vkCmdBlitImage2/etc., all
  # already wired with a KHR fallback in vulkan_loader.h) was never
  # registered in getExtensionList, same missing-registration bug as the
  # earlier six-extension fix, just found one crash later: SIGSEGV in
  # DxvkContext::copyImageBufferData on Mali-G76, confirmed via tombstone.
  # dxvk-mali-g76-legacy-copy-fallback.patch: registering VK_KHR_copy_commands2
  # wasn't enough -- Mali-G76 genuinely doesn't support it (unlike the
  # earlier six-extension registration bug), so vkCmdCopyBufferToImage2 and
  # its five siblings (vkCmdCopyBuffer2, vkCmdCopyImage2, vkCmdCopyImage-
  # ToBuffer2, vkCmdBlitImage2, vkCmdResolveImage2) all stayed null.
  # SIGSEGV persisted identically in DxvkContext::copyImageBufferData after
  # the extension-registration fix, confirmed via a second tombstone at the
  # exact same offsets. Adds a legacy (non-"2") fallback for all six,
  # gated on the resolved function pointer itself rather than a cached
  # feature bool (DxvkDevice is only forward-declared in this header).
  # dxvk-mali-g76-legacy-render-pass.patch: Mali-G76 also genuinely lacks
  # VK_KHR_dynamic_rendering, which DXVK 2.x's whole render-target-binding
  # and (monolithic) pipeline-creation path is unconditionally built on --
  # SIGSEGV (null vkCmdBeginRendering) on the very first draw call,
  # confirmed via tombstone in DxvkContext::renderPassBindFramebuffer. No
  # 1:1 legacy function exists for dynamic rendering, so this adds a
  # minimal classic VkRenderPass/VkFramebuffer object cache back
  # (dxvk_legacy_renderpass.h/.cpp, new files) used only when
  # !features().vk13.dynamicRendering: DxvkContext::renderPassBindFramebuffer
  # /renderPassUnbindFramebuffer branch to vkCmdBeginRenderPass/
  # vkCmdEndRenderPass, and DxvkGraphicsPipeline::createOptimizedPipeline
  # uses a format/sample-count-compatible render pass instead of chaining
  # VkPipelineRenderingCreateInfo (graphics-pipeline-library's own
  # createBasePipeline path was already gated on device support and needs
  # no change).
  # dxvk-mali-g76-composite-alpha.patch: swap chain creation hardcoded
  # VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR. Real-device testing (Redmi Note 8
  # Pro / Mali-G76 MC4) found a surface that only reports
  # VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR as supported (validation:
  # VUID-VkSwapchainCreateInfoKHR-compositeAlpha-01280), so
  # vkCreateSwapchainKHR failed and left the swapchain null, crashing the
  # next time the render loop touched it. Now picks OPAQUE when supported,
  # otherwise the first bit the surface actually reports.
  # dxvk-mali-g76-vertex-buffer-stride-fallback.patch: vkCmdBindVertexBuffers2
  # (Vulkan 1.3 core / VK_EXT_extended_dynamic_state) was called
  # unconditionally in DxvkContext::updateVertexBufferBindings, null on
  # Mali-G76 (Vulkan 1.1 only) -- tombstone-confirmed SIGSEGV on the very
  # first indexed draw call. Legacy vkCmdBindVertexBuffers has no per-draw
  # stride parameter, so updateVertexBufferBindings also now only selects
  # dynamic per-draw strides when the "2" function is actually available;
  # otherwise the real stride is baked into the pipeline's
  # VkVertexInputBindingDescription as usual, avoiding a stride-0 fallback
  # (silent broken rendering) once the crash itself is fixed.
  # dxvk-mali-g76-extended-dynamic-state.patch: after the fixes above got
  # the game rendering, validation on Mali-G76 revealed
  # vkCreateGraphicsPipelines() unconditionally using
  # VK_DYNAMIC_STATE_CULL_MODE/FRONT_FACE/VIEWPORT_WITH_COUNT/etc.
  # (VK_EXT_extended_dynamic_state) and vkCreateShaderModule() declaring
  # SPV_EXT_demote_to_helper_invocation (D3D9 alpha-test emulation),
  # neither ever registered on this fork -- same missing-registration bug
  # as khrCopyCommands2, just found via VUIDs instead of a crash this
  # time (the driver appears to tolerate the malformed pipeline/shader
  # module well enough to not immediately SIGSEGV, but it's undefined
  # behavior per spec). Also found and fixed a second, unrelated gap
  # while investigating: D3D9's *fixed-function* vertex shader compiler
  # (d3d9_fixed_function.cpp) unconditionally declared SPIR-V's
  # ClipDistance capability too -- a separate compiler from the DXSO one
  # that dxvk-mali-clip-distance.patch already gated, so it was missed by
  # that earlier fix.
  # dxvk-mali-g76-dynamic-state-fallback.patch: registering
  # VK_EXT_extended_dynamic_state (dxvk-mali-g76-extended-dynamic-state.patch)
  # wasn't enough -- Mali-G76 genuinely doesn't support it (unlike the
  # earlier six-extension registration bug), confirmed by a real tombstone:
  # SIGSEGV inside the Mali driver itself, called from
  # DxvkGraphicsPipeline::createOptimizedPipeline. Unlike previous legacy-
  # function-pointer fallbacks, there is no dynamic per-draw equivalent for
  # VK_DYNAMIC_STATE_CULL_MODE/FRONT_FACE at all, so this drops them from
  # the pipeline's dynamic state entirely and bakes a fixed
  # VK_CULL_MODE_NONE (this is a "step 1" cheap fix: crash gone, but no
  # per-draw cull mode support on this device until/unless a bigger
  # follow-up threads real cull state through the pipeline cache key).
  # VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT/SCISSOR_WITH_COUNT do have a
  # legacy equivalent (core 1.0 VIEWPORT/SCISSOR, fixed count baked in --
  # harmless since D3D9 never uses more than one viewport), so those fall
  # back properly instead of being dropped.
  # dxvk-mali-g76-demote-to-helper-fallback.patch: VK_EXT_shader_demote_
  # to_helper_invocation is ALSO genuinely unsupported on Mali-G76 (used
  # by D3D9's alpha-test and texkill/clip instruction emulation) -- unlike
  # extended_dynamic_state this produced a hard failure, not just a
  # validation warning: vkCreateShaderModule returned
  # VK_ERROR_INITIALIZATION_FAILED outright, and DXVK didn't handle the
  # resulting null shader module gracefully in
  # DxvkGraphicsPipeline::createOptimizedPipeline. Adds SpirvModule::
  # opKill() (didn't exist anywhere in this codebase -- every discard-like
  # operation went through demote-to-helper) as a fallback: a plain SPIR-V
  # discard instead of demoting the invocation. Slightly less accurate
  # (no derivatives survive past the discard, so texkill'd pixels can
  # pick a slightly wrong mip level for subsequent texture samples in the
  # same shader), but a fixed, acceptable tradeoff against not being able
  # to compile the shader at all. Alpha test is unaffected by this
  # tradeoff since it's always the last operation in the pixel shader.
  # dxvk-mali-g76-null-descriptor-fallback.patch: VK_EXT_robustness2's
  # nullDescriptor sub-feature is also genuinely unsupported on Mali-G76
  # (already correctly disabled at the device level by
  # dxvk-mali-g76-robustness2-optional.patch), but the descriptor-set-
  # update code in DxvkContext still unconditionally wrote VK_NULL_HANDLE
  # for unbound sampled/storage/combined-image-sampler and uniform/storage
  # buffer slots, which is only a valid VkImageView/VkBuffer value when
  # nullDescriptor IS supported. Confirmed via engine log VUIDs
  # (VUID-VkWriteDescriptorSet-descriptorType-02997,
  # VUID-VkDescriptorBufferInfo-buffer-02998) right before a crash inside
  # the Mali driver itself. Adds a dummy 1x1 image (plus per-view-type
  # DxvkImageView cache, covering 1D/2D/3D/Cube/array variants) to
  # DxvkUnboundResources, mirroring its existing dummy buffer/sampler
  # pattern, and uses it (plus the existing dummy buffer) instead of
  # VK_NULL_HANDLE when nullDescriptor is unavailable.
  # dxvk-mali-g76-swapchain-blitter-legacy-renderpass.patch:
  # DxvkSwapchainBlitter::present() (the final swapchain present/blit,
  # HUD and cursor compositing) is a separate consumer of dynamic
  # rendering that doesn't go through DxvkContext::renderPassBind
  # Framebuffer at all, so it was missed by dxvk-mali-g76-legacy-
  # render-pass.patch above. Confirmed via a real tombstone: SIGSEGV at
  # pc=0x0 (null function pointer call, vkCmdBeginRendering) on the CS
  # thread inside DxvkSwapchainBlitter::present, reached only once the
  # main render path (already using the legacy render pass pool) starts
  # succeeding far enough to present a frame. Routes present() through
  # the same DxvkLegacyRenderPassPool via a single-color-attachment
  # DxvkRenderTargets/DxvkRenderPassOps built from the same layout/
  # loadOp values already computed for the dynamic-rendering path, so
  # the two branches stay in sync.
  # dxvk-mali-g76-blitter-pipeline-legacy-renderpass.patch: the previous
  # patch got present() past its own vkCmdBeginRendering/vkCmdEndRendering
  # calls, but performDraw() inside that same present() call still
  # crashed one level deeper -- DxvkSwapchainBlitter::createPipeline()
  # (and the sibling createCursorPipeline()/HudRenderer::createPipeline(),
  # same pattern) build their VkGraphicsPipelineCreateInfo independently
  # of DxvkGraphicsPipeline's own cache, chaining
  # VkPipelineRenderingCreateInfo unconditionally and leaving .renderPass
  # at VK_NULL_HANDLE (only valid under dynamic rendering). Confirmed via
  # a real tombstone: SIGSEGV inside the Mali driver's
  # vkCreateGraphicsPipelines, called from DxvkSwapchainBlitter::
  # createPipeline via performDraw via present. All three now chain a
  # real getCompatibleRenderPass() handle instead when the device lacks
  # dynamic rendering, same as the main pipeline path. Note: this patch
  # file was later extended (same crash site, dynamic-state fix) -- see
  # its own header comment for the full history.
  # dxvk-mali-g76-blitter-null-descriptor-fallback.patch: one more
  # independent consumer of the nullDescriptor gap
  # (dxvk-mali-g76-null-descriptor-fallback.patch above only covered
  # DxvkContext's own descriptor update loop). DxvkSwapchainBlitter::
  # performDraw() writes its gamma/HUD/cursor descriptors unconditionally
  # (all 4 bindings are statically referenced by the present shader via
  # spec constants), leaving imageView at VK_NULL_HANDLE whenever that
  # frame has no gamma ramp/HUD/cursor to composite -- confirmed via a
  # real tombstone at the exact same Mali driver call site as the
  # earlier nullDescriptor fix. Adds a small DxvkDevice::dummyResources()
  # forwarding accessor (the blitter isn't a DxvkContext, so it can't
  # reach DxvkObjects's dummy resources via the usual m_common friend
  # access) and uses the existing dummy-image-view cache through it.
  # dxvk-mali-g76-format-properties3-fallback.patch: root cause of the
  # green/magenta texture corruption. DxvkAdapter::getFormatFeatures read
  # its results out of a VkFormatProperties3 chained into
  # vkGetPhysicalDeviceFormatProperties2, but that struct is core only in
  # Vulkan 1.3 and otherwise needs VK_KHR_format_feature_flags2 -- which
  # this build never enables anywhere. A Vulkan 1.1 driver silently ignores
  # the unrecognised pNext node and never writes to it, so the
  # zero-initialised struct came back untouched and EVERY format reported
  # ZERO features. Completely invisible: no VUID, no DXVK error, and the
  # engine's own "no valid texture format" assert is compiled out in
  # release. Downstream that made IDirect3D9::CheckDeviceFormat answer
  # D3DERR_NOTAVAILABLE for every format, so DX8Caps marked even DXTC
  # unsupported, Get_Valid_Texture_Format fell off the end of its fallback
  # ladder and degraded every texture in the game to R5G6B5, and the
  # CompatLib D3DX shims then bailed out on the resulting format mismatch
  # without writing any texels -- leaving uninitialised VRAM (the flat
  # green) or the engine's own magenta "missing texture" placeholder.
  # Fixes it by reading the core VkFormatProperties2::formatProperties
  # when VkFormatProperties3 is unavailable; VkFormatFeatureFlagBits2
  # deliberately shares bit values with VkFormatFeatureFlagBits, so the
  # widening is exact.
  # dxvk-mali-g76-hud-image-legacy-renderpass.patch: DxvkSwapchainBlitter::
  # renderHudImage is a third consumer of dynamic rendering in that file,
  # separate from present() and from the pipeline creation paths, and it was
  # missed when those were converted. It only runs when the DXVK HUD is
  # enabled AND the swap chain needs composition, which is why it stayed
  # hidden until the HUD was turned on to collect frame timings -- then it
  # crashed immediately on a null vkCmdBeginRendering (tombstone: pc=0x0 in
  # renderHudImage). Routes it through the same DxvkLegacyRenderPassPool.
  # Unlike present(), this pass uses LOAD_OP_CLEAR, so it always supplies a
  # clear value.
  # dxvk-mali-g76-hud-stderr-log.patch: mirrors the same fps/drawcalls/
  # submissions/pipelines values the on-screen DXVK_HUD overlay already
  # computes into stderr (one throttled line per item, same ~0.5s cadence
  # as the overlay refresh), so real numbers can be read straight out of
  # generalszhlogNN.txt instead of relying on a screenshot of the overlay.
  # HudPipelineStatsItem had no built-in throttle of its own (it recomputes
  # every frame), so it gets its own timer purely for the log line.
  # dxvk-refcount-memory-order-audit.patch: a real device report (Adreno
  # 830, main-menu background video playing -- i.e. very frequent
  # texture/surface lock-unlock churn) still showed
  # DxvkResourceAllocationPool corruption after dxvk-resource-refcount-
  # memory-order.patch above, which fixed exactly this signature for
  # DxvkResourceAllocation specifically. Auditing every other atomic
  # refcounted class in this fork for the same acquire-only (or, for
  # DxvkSampler, fully relaxed) decrement pattern found the identical gap
  # in DxvkGpuEvent, DxvkGpuQuery/DxvkQuery, DxvkLatencyTracker, DxvkSampler
  # and DxvkPagedResource (dxvk_sparse.h's acquire()/release(), which
  # DxvkPagedResource::incRef/decRef just forward to). Same fix as before:
  # relaxed increment, release decrement, and an explicit acquire fence on
  # the thread that actually observes the count reach zero, before it
  # destroys/recycles the object -- the standard Boost.SmartPtr/libstdc++
  # shared_ptr idiom. This closes every other instance of the same class of
  # bug found so far, but is a mitigation applied by code audit, not a
  # confirmed fix for this specific tombstone -- no access to the reporting
  # device to verify live.
  # dxvk-gpu-event-second-class-refcount-memory-order.patch: TSan-based
  # litmus testing (tools/dxvk-refcount-litmus/) of this exact idiom on the
  # host amd64 toolchain (TSan checks the C++ memory model's happens-before
  # proof, not physical CPU reordering, so it validates the ARM-relevant
  # synchronization discipline even off-device) led to re-auditing every
  # incRef/decRef pair by hand rather than trusting the previous audit was
  # exhaustive. dxvk_gpu_event.h turned out to define TWO separate
  # ref-counted classes -- DxvkGpuEvent (fixed above by
  # dxvk-refcount-memory-order-audit.patch) and, ~50 lines below it in the
  # same file, DxvkEvent (used for D3D9/D3D11 QUERY_EVENT queries via
  # DxvkDevice::createGpuEvent()) -- and the previous audit only patched the
  # first pair it found per file, missing DxvkEvent's identical acquire-only
  # increment / release-only-decrement-with-no-fence bug entirely. Same fix
  # as always: relaxed increment, release decrement, explicit acquire fence
  # before delete on the thread that observes the count reach zero.
  foreach(DXVK_PATCH_NAME dxvk-android.patch dxvk-ios.patch dxvk-vulkan11-adaptive.patch dxvk-resource-refcount-memory-order.patch dxvk-mali-clip-distance.patch dxvk-mali-g76-robustness2-optional.patch dxvk-android-missing-fallback-extensions.patch dxvk-mali-g76-legacy-barrier-fallback.patch dxvk-mali-g76-semaphore-fn-fallback.patch dxvk-mali-g76-4444-format.patch dxvk-mali-g76-copy-commands2.patch dxvk-mali-g76-legacy-copy-fallback.patch dxvk-mali-g76-legacy-render-pass.patch dxvk-mali-g76-composite-alpha.patch dxvk-mali-g76-vertex-buffer-stride-fallback.patch dxvk-mali-g76-extended-dynamic-state.patch dxvk-mali-g76-dynamic-state-fallback.patch dxvk-mali-g76-demote-to-helper-fallback.patch dxvk-mali-g76-null-descriptor-fallback.patch dxvk-mali-g76-swapchain-blitter-legacy-renderpass.patch dxvk-mali-g76-blitter-pipeline-legacy-renderpass.patch dxvk-mali-g76-blitter-null-descriptor-fallback.patch dxvk-mali-g76-format-properties3-fallback.patch dxvk-mali-g76-hud-image-legacy-renderpass.patch dxvk-mali-g76-hud-stderr-log.patch dxvk-composite-alpha-log.patch dxvk-android-force-opaque-alpha.patch dxvk-refcount-memory-order-audit.patch dxvk-gpu-event-second-class-refcount-memory-order.patch)
    execute_process(
      COMMAND git -C "${DXVK_LOCAL_FORK_DIR}" apply --reverse --check "${CMAKE_SOURCE_DIR}/Patches/${DXVK_PATCH_NAME}"
      RESULT_VARIABLE DXVK_PATCH_ALREADY_APPLIED
      ERROR_QUIET)
    if(NOT DXVK_PATCH_ALREADY_APPLIED EQUAL 0)
      # Some patches touch code that a later patch also touches nearby;
      # the surrounding 3-line context can drift just enough that the
      # exact-context idempotency check above no longer matches even
      # though the patch's actual content is present. Retry with a
      # smaller context window (git's own -C flag) before concluding the
      # patch genuinely isn't applied yet.
      execute_process(
        COMMAND git -C "${DXVK_LOCAL_FORK_DIR}" apply --reverse --check -C1 "${CMAKE_SOURCE_DIR}/Patches/${DXVK_PATCH_NAME}"
        RESULT_VARIABLE DXVK_PATCH_ALREADY_APPLIED
        ERROR_QUIET)
    endif()
    if(NOT DXVK_PATCH_ALREADY_APPLIED EQUAL 0)
      execute_process(
        COMMAND git -C "${DXVK_LOCAL_FORK_DIR}" apply "${CMAKE_SOURCE_DIR}/Patches/${DXVK_PATCH_NAME}"
        RESULT_VARIABLE DXVK_PATCH_RESULT)
      if(NOT DXVK_PATCH_RESULT EQUAL 0)
        execute_process(
          COMMAND git -C "${DXVK_LOCAL_FORK_DIR}" apply -C1 "${CMAKE_SOURCE_DIR}/Patches/${DXVK_PATCH_NAME}"
          RESULT_VARIABLE DXVK_PATCH_RESULT)
      endif()
      if(NOT DXVK_PATCH_RESULT EQUAL 0)
        message(FATAL_ERROR "Failed to apply Patches/${DXVK_PATCH_NAME} to references/fbraz3-dxvk — the Android DXVK build requires it.")
      endif()
      message(STATUS "DXVK Android: applied Patches/${DXVK_PATCH_NAME}")
    else()
      message(STATUS "DXVK Android: Patches/${DXVK_PATCH_NAME} already applied")
    endif()
  endforeach()

  set(DXVK_BUILD_DIR "${CMAKE_BINARY_DIR}/_deps/dxvk-build-android")
  set(DXVK_D3D8_LIB  "${DXVK_BUILD_DIR}/src/d3d8/libdxvk_d3d8.so")
  set(DXVK_D3D9_LIB  "${DXVK_BUILD_DIR}/src/d3d9/libdxvk_d3d9.so")

  # Locate the NDK's llvm toolchain bin dir for the meson cross file. The NDK
  # toolchain file (chainloaded by vcpkg) sets CMAKE_ANDROID_NDK / ANDROID_NDK.
  if(CMAKE_ANDROID_NDK)
    set(DXVK_ANDROID_NDK "${CMAKE_ANDROID_NDK}")
  elseif(ANDROID_NDK)
    set(DXVK_ANDROID_NDK "${ANDROID_NDK}")
  elseif(DEFINED ENV{ANDROID_NDK_HOME})
    set(DXVK_ANDROID_NDK "$ENV{ANDROID_NDK_HOME}")
  else()
    message(FATAL_ERROR "DXVK Android build: cannot locate the NDK (CMAKE_ANDROID_NDK/ANDROID_NDK/ANDROID_NDK_HOME all unset)")
  endif()
  file(GLOB DXVK_NDK_TOOLCHAIN_BINS "${DXVK_ANDROID_NDK}/toolchains/llvm/prebuilt/*/bin")
  if(NOT DXVK_NDK_TOOLCHAIN_BINS)
    message(FATAL_ERROR "DXVK Android build: no llvm toolchain found under ${DXVK_ANDROID_NDK}/toolchains/llvm/prebuilt")
  endif()
  list(GET DXVK_NDK_TOOLCHAIN_BINS 0 ANDROID_TOOLCHAIN_BIN)

  # API level, in order of trustworthiness. GeneralsX @bugfix Android CI
  # 07/07/2026: when vcpkg chainloads the NDK toolchain, CMAKE_SYSTEM_VERSION
  # can end up as the literal "1" (vcpkg's generic Android system stanza) —
  # which produced a nonexistent aarch64-linux-android1-clang in the cross
  # file. Prefer the NDK toolchain's own numeric ANDROID_PLATFORM_LEVEL, then
  # the ANDROID_PLATFORM cache var ("android-28") the preset sets, and only
  # then a sanity-checked CMAKE_SYSTEM_VERSION (real API levels start at 21
  # for arm64).
  if(ANDROID_PLATFORM_LEVEL MATCHES "^[0-9]+$")
    set(ANDROID_API "${ANDROID_PLATFORM_LEVEL}")
  elseif(ANDROID_PLATFORM MATCHES "([0-9]+)$")
    set(ANDROID_API "${CMAKE_MATCH_1}")
  elseif(CMAKE_SYSTEM_VERSION MATCHES "^[0-9]+$" AND CMAKE_SYSTEM_VERSION GREATER_EQUAL 21)
    set(ANDROID_API "${CMAKE_SYSTEM_VERSION}")
  else()
    set(ANDROID_API "28")
  endif()
  message(STATUS "Building DXVK ${DXVK_VERSION} for Android arm64-v8a (API ${ANDROID_API}) with Meson (${MESON_EXECUTABLE})")

  # Generate a pkg-config file for the in-tree (FetchContent) SDL3 so meson's
  # dependency('SDL3') resolves to it — the exact same silent-SDL2-fallback trap
  # as on macOS applies (see above). DXVK_SDL3_PC_DIR must be set BEFORE the
  # cross file is configured below: the template embeds it as the host
  # machine's pkg_config_libdir (env vars only reach the build machine in a
  # meson cross build).
  set(DXVK_SDL3_PC_DIR "${CMAKE_BINARY_DIR}/sdl3-pkgconfig")
  # The file is written under BOTH casings: meson asks pkg-config for 'SDL3',
  # and pkg-config maps that to SDL3.pc on a case-sensitive filesystem — a
  # lowercase-only sdl3.pc worked on macOS (case-insensitive) and silently
  # broke the SDL3 lookup on Linux.
  foreach(DXVK_SDL3_PC_NAME sdl3.pc SDL3.pc)
    file(WRITE "${DXVK_SDL3_PC_DIR}/${DXVK_SDL3_PC_NAME}"
"prefix=${CMAKE_BINARY_DIR}/_deps
libdir=\${prefix}/sdl3-build
includedir=\${prefix}/sdl3-src/include

Name: sdl3
Description: Simple DirectMedia Layer (in-tree FetchContent build)
Version: 3.4.2
Libs: -L\${libdir} -lSDL3
Cflags: -I\${includedir}
")
  endforeach()
  # Belt-and-braces for the build machine side; the host machine reads the
  # pkg_config_libdir property from the cross file.
  set(DXVK_PKG_CONFIG_ENV "PKG_CONFIG_LIBDIR=${DXVK_SDL3_PC_DIR}")

  configure_file(${CMAKE_SOURCE_DIR}/cmake/meson-arm64-android-cross.ini.in
                 ${CMAKE_BINARY_DIR}/meson-arm64-android-cross.ini @ONLY)
  set(DXVK_MESON_MACHINE_ARGS --cross-file ${CMAKE_BINARY_DIR}/meson-arm64-android-cross.ini)

  ExternalProject_Add(dxvk_android_build
    SOURCE_DIR        ${DXVK_SOURCE_DIR}
    BINARY_DIR        ${DXVK_BUILD_DIR}
    DOWNLOAD_COMMAND  ""
    UPDATE_COMMAND    ""
    PATCH_COMMAND     ""
    CONFIGURE_COMMAND ${CMAKE_COMMAND} -E env "${DXVK_PKG_CONFIG_ENV}" ${MESON_EXECUTABLE} setup ${DXVK_BUILD_DIR} ${DXVK_SOURCE_DIR} ${DXVK_MESON_MACHINE_ARGS} -Ddxvk_native_wsi=sdl3 --buildtype=release --reconfigure
    BUILD_COMMAND     ${NINJA_EXECUTABLE} -C ${DXVK_BUILD_DIR} src/d3d9/libdxvk_d3d9.so src/d3d8/libdxvk_d3d8.so
    BUILD_BYPRODUCTS  ${DXVK_D3D9_LIB} ${DXVK_D3D8_LIB}
    INSTALL_COMMAND   ""
    UPDATE_DISCONNECTED TRUE
    BUILD_ALWAYS      TRUE
  )
  # GeneralsX @bugfix Android port 30/07/2026 Two compounding staleness bugs
  # found the hard way -- libdxvk_d3d9.so in this environment was last
  # actually built 23/07, and every DXVK source patch applied since
  # (including two meant to fix real device crashes, today) silently never
  # made it into a single shipped APK despite every build reporting success:
  #   1. Without BUILD_ALWAYS, ExternalProject_Add only ever runs
  #      BUILD_COMMAND once and then trusts a stamp file forever, regardless
  #      of source changes. Ninja's own incremental build inside
  #      DXVK_BUILD_DIR is a no-op in well under a second when nothing
  #      changed, so forcing this step to always run costs nothing and makes
  #      ninja -- not a CMake stamp file -- the actual source of truth.
  #   2. Even with (1) fixed, the add_custom_command below that copies the
  #      built .so out of DXVK_BUILD_DIR only depended on the
  #      dxvk_android_build *target* (an ordering-only dependency), not on
  #      the .so *file* -- so Ninja never saw a reason to re-run the copy
  #      when the file's content changed underneath it, and kept shipping
  #      the stale copy. BUILD_BYPRODUCTS is what tells Ninja these paths
  #      are real files this target produces, so downstream DEPENDS on the
  #      file (not just the target) actually tracks freshness.
  # meson links the DXVK libs against the in-tree libSDL3.so (from the generated
  # sdl3.pc); make sure it exists before the ExternalProject's build step runs.
  if(TARGET SDL3-shared)
    add_dependencies(dxvk_android_build SDL3-shared)
  endif()

  # Copy libdxvk_d3d8 + libdxvk_d3d9 to the build dir root, where the packaging
  # script picks them up as jniLibs. d3d8 links d3d9 via DT_NEEDED, both ship.
  add_custom_command(
    OUTPUT  "${CMAKE_BINARY_DIR}/libdxvk_d3d9.so"
            "${CMAKE_BINARY_DIR}/libdxvk_d3d8.so"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
              ${DXVK_D3D9_LIB} "${CMAKE_BINARY_DIR}/libdxvk_d3d9.so"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
              ${DXVK_D3D8_LIB} "${CMAKE_BINARY_DIR}/libdxvk_d3d8.so"
    DEPENDS dxvk_android_build ${DXVK_D3D9_LIB} ${DXVK_D3D8_LIB}
    COMMENT "Installing libdxvk_d3d8 + libdxvk_d3d9 to build directory"
  )
  add_custom_target(dxvk_d3d8_install ALL
    DEPENDS "${CMAKE_BINARY_DIR}/libdxvk_d3d8.so"
            "${CMAKE_BINARY_DIR}/libdxvk_d3d9.so"
  )

  # Export paths so other cmake files know where the headers are
  set(DXVK_INCLUDE_DIR "${DXVK_SOURCE_DIR}/include/native" CACHE PATH "DXVK native headers")
  set(dxvk_SOURCE_DIR "${DXVK_SOURCE_DIR}" CACHE PATH "DXVK source directory (Android)")
  message(STATUS "DXVK source directory: ${DXVK_SOURCE_DIR}")
  message(STATUS "DXVK d3d8 library:     ${DXVK_D3D8_LIB}")

else()
  # Linux: Fetch pre-built DXVK native binary for DirectX→Vulkan translation
  # Native 32-bit and 64-bit Linux binaries (.so)
  FetchContent_Declare(
    dxvk
    URL        https://github.com/doitsujin/dxvk/releases/download/v2.6/dxvk-native-2.6-steamrt-sniper.tar.gz
  )
  FetchContent_MakeAvailable(dxvk)
  message(STATUS "Using DXVK native (Linux DirectX→Vulkan)")
  message(STATUS "DXVK source directory: ${dxvk_SOURCE_DIR}")
endif()
