# GeneralsX @feature Android port 10/07/2026 libadrenotools
# (https://github.com/bylaws/libadrenotools, BSD-2-Clause) lets the app load
# a user-supplied Vulkan driver -- most commonly a Mesa Turnip build for
# Adreno GPUs -- in place of the phone's stock vendor driver. It works by
# installing a linker-namespace-bypass hook that transparently redirects
# every later dlopen("libvulkan.so") call in the process, so DXVK's own
# internal loader (references/fbraz3-dxvk/src/vulkan/vulkan_loader.cpp)
# picks up the custom driver without ever being patched or made aware of
# this -- see TryLoadCustomVulkanDriver() in SDL3Main.cpp, which must run
# before SDL_Vulkan_LoadLibrary() so the hook is installed first.
#
# This is Adreno-only: Turnip has no Mali backend, so it cannot help phones
# whose GPU is Mali (e.g. the Mali-G76 case documented in SDL3Main.cpp).
# What it DOES help: Adreno phones whose stock driver reports Vulkan 1.1/1.2
# while DXVK 2.6 needs 1.3 -- exactly the ceiling AndroidManifest.xml's
# uses-feature currently documents.
#
# custom_driver.cfg is populated one of two ways: the user imports a driver
# via the Setup app's "Custom Vulkan Driver" section, or -- as of 13/07/2026 --
# SetupActivity.applyRecommendedDriverIfNeeded() auto-populates it with a
# bundled Turnip build (scripts/build/android/fetch-turnip.sh) on Adreno
# phones whose stock driver reports less than Vulkan 1.3. Either way this
# file is absent and TryLoadCustomVulkanDriver() is a no-op on phones that
# don't need it (non-Adreno GPUs, or Adreno with a stock driver that
# already handles 1.3).
if(ANDROID)
    set(GEN_INSTALL_TARGET OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
        adrenotools
        GIT_REPOSITORY https://github.com/bylaws/libadrenotools.git
        GIT_TAG        8fae8ce254dfc1344527e05301e43f37dea2df80
        GIT_SUBMODULES_RECURSE TRUE
    )
    FetchContent_MakeAvailable(adrenotools)
    message(STATUS "libadrenotools: custom Vulkan driver loading (Adreno/Turnip) enabled")
endif()
