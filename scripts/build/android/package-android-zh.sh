#!/bin/bash
# Package the Android build of Zero Hour into an APK.
#
# Flow:
#   1. Stage the native libraries built by build-android-zh.sh into the Gradle
#      shell's jniLibs (libmain.so, SDL3, SDL3_image, openal, gamespy, DXVK
#      d3d8/d3d9, libc++_shared from the NDK).
#   2. Stage SDL3's Java glue (org.libsdl.app.SDLActivity et al) from the
#      in-tree SDL3 source, so Java and native SDL always match versions.
#   3. Stage small runtime assets bundled into the APK and extracted on first
#      launch: fonts/ (Liberation, renamed), dxvk.conf, DefaultOptions.ini.
#   4. gradle assemble<Flavor>Debug -> app-<flavor>-debug.apk, ready for
#      adb install. Two product flavors (see android/app/build.gradle): zh
#      (2D game) and xr (native OpenXR tabletop, own package
#      com.generalsx.zerohour.xr so it gets its own launcher icon -- Quest
#      shows one icon per app). GX_FLAVORS selects a subset ("zh xr" default).
#
# Game .big archives are NOT packaged: the user picks their own game data
# folder in Setup (see docs/port/ANDROID_PORT.md). Both flavors boot the
# full game now, so both need the folder (the xr flavor redirects to Setup
# when none is configured).
#
# Usage: ./scripts/build/android/package-android-zh.sh [--install]
#   --install  adb install the APK(s) to the first connected device
set -euo pipefail

DO_INSTALL=0
for arg in "$@"; do
    case "$arg" in
        --install) DO_INSTALL=1 ;;
        *) echo "ERROR: unknown argument '$arg' (usage: $0 [--install])"; exit 1 ;;
    esac
done

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
BUILD_DIR="${PROJECT_ROOT}/build/android-vulkan"
ANDROID_DIR="${PROJECT_ROOT}/android"
JNILIBS="${ANDROID_DIR}/app/src/main/jniLibs/arm64-v8a"
JAVA_SDL="${ANDROID_DIR}/app/src/main/java-sdl"
ASSETS="${ANDROID_DIR}/app/src/main/assets/gamedata"
DEFAULT_DRIVER_ASSETS="${ANDROID_DIR}/app/src/main/assets/default_driver"
STAGING="${GX_ANDROID_STAGING:-${HOME}/GeneralsX/android-staging}"

# --- 1. native libraries -----------------------------------------------------
GAME_LIB="$(find "${BUILD_DIR}" -name libmain.so -not -path "*/_deps/*" 2>/dev/null | head -1)"
if [[ -z "${GAME_LIB}" ]]; then
    echo "ERROR: libmain.so not found — run ./scripts/build/android/build-android-zh.sh first."
    exit 1
fi

rm -rf "${JNILIBS}"
mkdir -p "${JNILIBS}"
cp "${GAME_LIB}" "${JNILIBS}/libmain.so"

# Required runtime .so set. Fail loudly on any missing file: a stale or partial
# stage produces an APK that dies at System.loadLibrary / D3D init.
declare -a REQUIRED_LIBS=(
    "_deps/sdl3-build/libSDL3.so"
    "libdxvk_d3d8.so"
    "libdxvk_d3d9.so"
)
for rel in "${REQUIRED_LIBS[@]}"; do
    src="${BUILD_DIR}/${rel}"
    if [[ ! -f "${src}" ]]; then
        echo "ERROR: required library missing: ${src}"
        exit 1
    fi
    cp "${src}" "${JNILIBS}/"
done

# SDL3_image, openal, and gamespy are all DT_NEEDED by libmain.so — a runtime
# `dlopen` dependency, not an optional feature — so a missing one is a hard
# packaging failure, not a warning to ship past (a real device confirmed this
# the hard way: "libgamespy.so not found" at launch from a build that had only
# warned and shipped anyway). Each entry lists every path the corresponding
# FetchContent project is known to place its output at; gamespy's is the
# gotcha: GamespySDK's own CMakeLists.txt does
# `set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})`, and
# CMAKE_BINARY_DIR names the OUTERMOST project's build dir even from inside a
# FetchContent'd subproject — so libgamespy.so lands at ${BUILD_DIR} directly,
# not under _deps/gamespy-build/ like every other FetchContent output here.
declare -a RUNTIME_LIB_CANDIDATES=(
    "libSDL3_image.so:_deps/sdl3_image-build/libSDL3_image.so"
    "libopenal.so:_deps/openal_soft-build/libopenal.so"
    "libgamespy.so:libgamespy.so"
    # GeneralsX @bugfix Android port 10/07/2026 libadrenotools' own
    # CMakeLists.txt (cmake/adrenotools.cmake) doesn't pin STATIC/SHARED, so
    # it follows this project's BUILD_SHARED_LIBS -- which resolves to ON
    # here (via the vcpkg-chainloaded NDK toolchain), making it an actual
    # DT_NEEDED shared library of libmain.so, not just a statically-linked
    # archive. CI's "Verify APK contains every library" step caught this
    # missing on the first real build.
    "libadrenotools.so:_deps/adrenotools-build/libadrenotools.so"
)
for entry in "${RUNTIME_LIB_CANDIDATES[@]}"; do
    name="${entry%%:*}"
    primary_rel="${entry#*:}"
    src="${BUILD_DIR}/${primary_rel}"
    if [[ ! -f "${src}" ]]; then
        src="$(find "${BUILD_DIR}" -maxdepth 6 -name "${name}" 2>/dev/null | head -1)"
    fi
    if [[ -z "${src}" || ! -f "${src}" ]]; then
        echo "ERROR: ${name} not found anywhere under ${BUILD_DIR} — libmain.so needs it at dlopen time and the APK will crash on launch without it."
        exit 1
    fi
    cp "${src}" "${JNILIBS}/"
done

# libadrenotools' hook shims (cmake/adrenotools.cmake). Not DT_NEEDED by
# libmain.so -- adrenotools_open_libvulkan() dlopen()s these by path only
# when a user has actually imported a custom driver via the Setup app's
# "Custom Vulkan Driver" section, and that path is required to equal
# nativeLibraryDir (i.e. this jniLibs directory). Package them unconditionally
# so the feature works the first time someone imports a driver, without
# needing a rebuild.
declare -a ADRENOTOOLS_HOOK_LIBS=(
    "libmain_hook.so"
    "libfile_redirect_hook.so"
    "libgsl_alloc_hook.so"
    "libhook_impl.so"
)
for name in "${ADRENOTOOLS_HOOK_LIBS[@]}"; do
    src="$(find "${BUILD_DIR}" -maxdepth 6 -name "${name}" 2>/dev/null | head -1)"
    if [[ -z "${src}" || ! -f "${src}" ]]; then
        echo "ERROR: ${name} not found anywhere under ${BUILD_DIR} — required by the Custom Vulkan Driver feature (cmake/adrenotools.cmake)."
        exit 1
    fi
    cp "${src}" "${JNILIBS}/"
done

# libc++_shared.so from the NDK (ANDROID_STL=c++_shared)
if [[ -z "${ANDROID_NDK_HOME:-}" ]]; then
    echo "ERROR: ANDROID_NDK_HOME must be set (for libc++_shared.so)."
    exit 1
fi
LIBCXX="$(ls "${ANDROID_NDK_HOME}"/toolchains/llvm/prebuilt/*/sysroot/usr/lib/aarch64-linux-android/libc++_shared.so 2>/dev/null | head -1)"
if [[ -z "${LIBCXX}" ]]; then
    echo "ERROR: libc++_shared.so not found in the NDK sysroot."
    exit 1
fi
cp "${LIBCXX}" "${JNILIBS}/"

# GeneralsX @build Android port ANGLE experiment - bundled prebuilt ANGLE
# (Vulkan backend) so the GLES rendering path (d3d8gles) can route its EGL
# context and gl* calls through ANGLE's Vulkan translation instead of the
# device's own GLES driver (see gles_dispatch.cpp / SDL3Main.cpp's
# GENERALSX_GLES_ANGLE toggle). Committed as prebuilt binaries, not rebuilt
# here: this ANGLE checkout is a Vulkan-only Android arm64 build from the
# AOSP mirror (android.googlesource.com/platform/external/angle), built with
# a hand-bootstrapped gn+ninja toolchain instead of the official
# depot_tools/gclient workflow (100GB+ disk, unavailable in this sandbox) --
# not a build this script can reasonably reproduce on every run. Not
# required for the game to run: d3d8gles falls back to the system's
# libGLESv3.so at runtime if either file is missing or fails to dlopen.
ANGLE_PREBUILT="${PROJECT_ROOT}/Core/Libraries/Source/d3d8gles/angle-prebuilt/arm64-v8a"
if [[ -f "${ANGLE_PREBUILT}/libEGL_angle.so" && -f "${ANGLE_PREBUILT}/libGLESv2_angle.so" ]]; then
    cp "${ANGLE_PREBUILT}/libEGL_angle.so" "${ANGLE_PREBUILT}/libGLESv2_angle.so" "${JNILIBS}/"
else
    echo "WARNING: prebuilt ANGLE libraries not found at ${ANGLE_PREBUILT} -- GLES backend will fall back to the system GLES driver."
fi

# Opt-in Vulkan validation layer (SDL3Main.cpp dxvk_validation.txt marker,
# see docs/BUILD/ANDROID_SANDBOXED_LOCAL.md). Not required for the game to
# run -- skip with a warning instead of failing the build if it isn't
# staged, same as the diagnostic tool it is.
VVL_STAGED="${STAGING}/vulkan_validation/libVkLayer_khronos_validation.so"
if [[ ! -f "${VVL_STAGED}" ]]; then
    echo "==> Vulkan validation layer not staged yet; fetching"
    "${PROJECT_ROOT}/scripts/build/android/fetch-vulkan-validation-layer.sh" || true
fi
if [[ -f "${VVL_STAGED}" ]]; then
    cp "${VVL_STAGED}" "${JNILIBS}/"
else
    echo "WARNING: Vulkan validation layer not available -- dxvk_validation.txt will have no effect in this build."
fi

echo "==> Staged $(ls "${JNILIBS}" | wc -l | tr -d ' ') native libraries:"
ls -la "${JNILIBS}"

# --- 2. SDL3 Java glue -------------------------------------------------------
SDL_JAVA_SRC="${BUILD_DIR}/_deps/sdl3-src/android-project/app/src/main/java/org/libsdl/app"
if [[ ! -d "${SDL_JAVA_SRC}" ]]; then
    echo "ERROR: SDL3 Java sources not found at ${SDL_JAVA_SRC} (configure/build first)."
    exit 1
fi
rm -rf "${JAVA_SDL}"
mkdir -p "${JAVA_SDL}/org/libsdl/app"
cp "${SDL_JAVA_SRC}"/*.java "${JAVA_SDL}/org/libsdl/app/"
echo "==> Staged SDL3 Java glue ($(ls "${JAVA_SDL}/org/libsdl/app" | wc -l | tr -d ' ') files)"

# java-sdl/ is gitignored (re-staged from the vanilla SDL3 tarball above on
# every packaging run), so any GeneralsX-specific fix to this vendored glue
# code has to be reapplied here every time, not just edited in place -- see
# android/patches/*.patch. Patches are applied fresh against the just-copied
# vanilla source each run, so there's no idempotency concern the way the
# DXVK submodule patches (cmake/dx8.cmake) have to guard against.
PATCH_DIR="${SCRIPT_DIR}/../../../android/patches"
if [[ -d "${PATCH_DIR}" ]]; then
    for patch_file in "${PATCH_DIR}"/*.patch; do
        [[ -f "${patch_file}" ]] || continue
        echo "==> Applying $(basename "${patch_file}") to SDL3 Java glue"
        patch -p1 -d "${JAVA_SDL}" < "${patch_file}"
    done
fi

# --- 3. runtime assets -------------------------------------------------------
mkdir -p "${ASSETS}"

# Fonts (Liberation, renamed to the Windows names the game requests).
if [[ ! -f "${STAGING}/fonts/arial.ttf" ]]; then
    echo "==> Fonts not staged yet; fetching Liberation fonts"
    GX_FONTS="${STAGING}/fonts" "${PROJECT_ROOT}/scripts/build/ios/stage-fonts.sh"
fi
rm -rf "${ASSETS}/fonts"
cp -R "${STAGING}/fonts" "${ASSETS}/fonts"

# dxvk.conf — tuned translation-layer defaults (16x aniso, quiet logs).
if [[ -f "${STAGING}/dxvk.conf" ]]; then
    cp "${STAGING}/dxvk.conf" "${ASSETS}/dxvk.conf"
else
    cat > "${ASSETS}/dxvk.conf" <<'EOF'
# DXVK configuration for GeneralsX Zero Hour on Android.
# Read from the game's working directory at startup.
dxvk.logLevel = none
# RTS camera angles smear terrain with plain trilinear; 16x anisotropic is the
# single biggest perceived-sharpness win and free on modern mobile GPUs.
d3d9.samplerAnisotropy = 16
# Background memory defragmentation corrupted the allocation pool on a real
# Adreno device (use-after-free crash in createAllocation; upstream has the
# same class of workaround for Intel ANV, dxvk issue #4395). SDL3Main.cpp
# also forces this via DXVK_CONFIG so existing installs with an older
# dxvk.conf in their game folder are covered too.
dxvk.enableMemoryDefrag = False
EOF
fi

# DefaultOptions.ini — seed full detail on first run: the 2003 GPU auto-detect
# doesn't know "Adreno 830" and would silently pick Low LOD + quarter-res textures.
if [[ -f "${STAGING}/DefaultOptions.ini" ]]; then
    cp "${STAGING}/DefaultOptions.ini" "${ASSETS}/DefaultOptions.ini"
else
    cat > "${ASSETS}/DefaultOptions.ini" <<'EOF'
AntiAliasing = 1
BuildingOcclusion = yes
DynamicLOD = no
ExtraAnimations = yes
GameSpyIPAddress = 0.0.0.0
HeatEffects = yes
IPAddress = 0.0.0.0
IdealStaticGameLOD = High
Retaliation = yes
ShowSoftWaterEdge = yes
ShowTrees = yes
StaticGameLOD = High
TextureReduction = 0
UseAlternateMouse = no
UseCloudMap = yes
UseDoubleClickAttackMove = no
UseLightMap = yes
UseShadowDecals = yes
UseShadowVolumes = yes
EOF
fi

# Default Vulkan driver (Mesa Turnip, K11MCH1/AdrenoToolsDrivers) -- fallback
# for Adreno phones whose stock driver reports less than Vulkan 1.3 (DXVK
# 2.6's floor). SetupActivity.applyRecommendedDriverIfNeeded() only uses this
# when the device actually needs it AND the user hasn't manually imported
# their own driver; a no-op everywhere else, including all non-Adreno GPUs.
# Lives in its own top-level assets/ folder (not gamedata/): it's extracted
# to app-private internal storage, not the user's external game-data folder.
if [[ ! -f "${STAGING}/default_driver/meta.json" ]]; then
    echo "==> Default Vulkan driver not staged yet; fetching Turnip"
    GX_DEFAULT_DRIVER="${STAGING}/default_driver" "${PROJECT_ROOT}/scripts/build/android/fetch-turnip.sh"
fi
rm -rf "${DEFAULT_DRIVER_ASSETS}"
mkdir -p "${DEFAULT_DRIVER_ASSETS}"
cp -R "${STAGING}/default_driver/." "${DEFAULT_DRIVER_ASSETS}/"

# GeneralsX @note Android port 11/07/2026 the GeneralsOnline lobby screens
# (WOLWelcomeMenu.wnd, WOLCustomLobby.wnd, WOLQuickMatchMenu.wnd,
# PopupPlayerInfo.wnd, WOLBuddyOverlay.wnd, OptionsMenu.wnd) are NOT custom
# assets we ship -- the real GeneralsOnline client reuses these unmodified,
# original Zero Hour .wnd files exactly as they already ship inside the base
# game's own .big archives (the user's copied game data). Only the C++
# callback/backend logic needed porting (see GUICallbacks/Menus/WOL*.cpp);
# there is nothing to stage here.

echo "==> Staged APK assets:"
find "${ASSETS}" -type f | sed "s|${ASSETS}/|    |"
find "${DEFAULT_DRIVER_ASSETS}" -type f | sed "s|${DEFAULT_DRIVER_ASSETS}/|    default_driver/|"

# --- 4. gradle ---------------------------------------------------------------
cd "${ANDROID_DIR}"
GRADLE_CMD=""
if [[ -x "./gradlew" ]]; then
    GRADLE_CMD="./gradlew"
elif command -v gradle >/dev/null 2>&1; then
    GRADLE_CMD="gradle"
else
    echo "ERROR: no gradle wrapper and no 'gradle' on PATH."
    echo "       Either install Gradle 8.x (sdkman/brew/apt) or open android/ once in"
    echo "       Android Studio (which generates the wrapper), then re-run this script."
    exit 1
fi
# versionCode/versionName are managed by hand in android/app/build.gradle
# (versionCode must strictly increase for Android to accept installing one
# APK "over" another via a normal tap install; adb install -r doesn't care).
# GX_ANDROID_VERSION_CODE / GX_ANDROID_VERSION_NAME let a manual CI dispatch
# (or a local one-off build) override either without editing build.gradle;
# leave both unset to just build whatever is committed there.
# (Plain string, not a bash array: an empty array expanded with "${arr[@]}"
# under `set -u` throws "unbound variable" on bash < 4.4 — still the default
# /bin/bash on macOS, which this script also runs on.)
GRADLE_VERSION_ARG=""
if [[ -n "${GX_ANDROID_VERSION_CODE:-}" ]]; then
    GRADLE_VERSION_ARG="-PandroidVersionCode=${GX_ANDROID_VERSION_CODE}"
fi
if [[ -n "${GX_ANDROID_VERSION_NAME:-}" ]]; then
    GRADLE_VERSION_ARG="${GRADLE_VERSION_ARG} -PandroidVersionName=${GX_ANDROID_VERSION_NAME}"
fi

FLAVORS="${GX_FLAVORS:-zh xr}"
TASKS=""
for flavor in $FLAVORS; do
    TASKS="${TASKS} assemble$(tr '[:lower:]' '[:upper:]' <<< "${flavor:0:1}")${flavor:1}Debug"
done
echo "==> ${GRADLE_CMD}${TASKS} ${GRADLE_VERSION_ARG}"
# shellcheck disable=SC2086
"${GRADLE_CMD}" $TASKS ${GRADLE_VERSION_ARG}

APKS=""
for flavor in $FLAVORS; do
    APK="${ANDROID_DIR}/app/build/outputs/apk/${flavor}/debug/app-${flavor}-debug.apk"
    if [[ ! -f "${APK}" ]]; then
        echo "ERROR: expected APK not found at ${APK}"
        exit 1
    fi
    echo "==> APK (${flavor}): ${APK}"
    # GeneralsX @build Codex 14/09/2026 Stable, product-named XR handoff artifact.
    if [[ "${flavor}" == "xr" ]]; then
        mkdir -p "${PROJECT_ROOT}/build/apk"
        cp "${APK}" "${PROJECT_ROOT}/build/apk/Generals-Zero-Hour-XR.apk"
        echo "==> Generals: Zero Hour XR: ${PROJECT_ROOT}/build/apk/Generals-Zero-Hour-XR.apk"
    fi
    APKS="${APKS} ${APK}"
done

if [[ $DO_INSTALL -eq 1 ]]; then
    command -v adb >/dev/null 2>&1 || { echo "ERROR: adb not found on PATH"; exit 1; }
    for APK in $APKS; do
        echo "==> adb install -r ${APK}"
        adb install -r "${APK}"
    done
    echo "==> Installed. Game data goes to:"
    echo "    /storage/emulated/0/Android/data/com.generalsx.zerohour/files/"
    echo "    e.g.: adb push ~/GeneralsX/GeneralsZH/. /storage/emulated/0/Android/data/com.generalsx.zerohour/files/"
fi
