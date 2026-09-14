#!/bin/bash
# Build Zero Hour for Android (arm64-v8a): checks deps, configures the
# android-vulkan preset, builds the game (libmain.so) and the DXVK d3d8/d3d9
# translation libraries, then verifies the artifacts are really Android/AArch64
# with the SDL3 WSI compiled in ("trust no successful exit code").
#
# Prerequisites:
#   - Android NDK r26+          export ANDROID_NDK_HOME=~/Android/Sdk/ndk/<ver>
#   - vcpkg (FULL clone)        export VCPKG_ROOT=~/vcpkg
#   - cmake >= 3.25, ninja, meson, pkg-config, git
#   - git submodule update --init references/fbraz3-dxvk
#
# Usage: ./scripts/build/android/build-android-zh.sh [--configure-only]
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
BUILD_DIR="${PROJECT_ROOT}/build/android-vulkan"

CONFIGURE_ONLY=0
for arg in "$@"; do
    case "$arg" in
        --configure-only) CONFIGURE_ONLY=1 ;;
        *) echo "ERROR: unknown argument '$arg' (usage: $0 [--configure-only])"; exit 1 ;;
    esac
done

# --- dependency checks -------------------------------------------------------
fail=0
if [[ -z "${ANDROID_NDK_HOME:-}" || ! -d "${ANDROID_NDK_HOME}" ]]; then
    echo "ERROR: ANDROID_NDK_HOME is unset or not a directory."
    echo "       Install the NDK (Android Studio SDK Manager or dl.google.com/android/repository)"
    echo "       and: export ANDROID_NDK_HOME=<sdk>/ndk/<version>"
    fail=1
elif [[ ! -f "${ANDROID_NDK_HOME}/build/cmake/android.toolchain.cmake" ]]; then
    echo "ERROR: ${ANDROID_NDK_HOME} does not look like an NDK (no build/cmake/android.toolchain.cmake)"
    fail=1
fi
if [[ -z "${VCPKG_ROOT:-}" || ! -f "${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" ]]; then
    echo "ERROR: VCPKG_ROOT is unset or has no scripts/buildsystems/vcpkg.cmake."
    echo "       git clone https://github.com/microsoft/vcpkg ~/vcpkg && ~/vcpkg/bootstrap-vcpkg.sh"
    echo "       (FULL clone — a shallow clone breaks manifest baselines)"
    fail=1
fi
for tool in cmake ninja meson pkg-config git; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        echo "ERROR: '$tool' not found on PATH"
        fail=1
    fi
done
if ! command -v glslangValidator >/dev/null 2>&1 && ! command -v glslang >/dev/null 2>&1; then
    echo "ERROR: glslangValidator not found (DXVK compiles its GLSL shaders with it)."
    echo "       apt install glslang-tools / brew install glslang"
    fail=1
fi
if [[ ! -e "${PROJECT_ROOT}/references/fbraz3-dxvk/.git" ]]; then
    echo "ERROR: DXVK fork submodule missing. Run:"
    echo "       git -C ${PROJECT_ROOT} submodule update --init references/fbraz3-dxvk"
    fail=1
elif [[ ! -f "${PROJECT_ROOT}/references/fbraz3-dxvk/include/spirv/include/spirv/unified1/spirv.hpp" ]]; then
    echo "ERROR: DXVK's nested submodules missing (SPIRV-Headers/Vulkan-Headers). Run:"
    echo "       git -C ${PROJECT_ROOT}/references/fbraz3-dxvk submodule update --init --depth 1"
    fail=1
fi
[[ $fail -eq 0 ]] || exit 1

# --- configure + build -------------------------------------------------------
cd "${PROJECT_ROOT}"
echo "==> Configuring (preset: android-vulkan)"
cmake --preset android-vulkan

if [[ $CONFIGURE_ONLY -eq 1 ]]; then
    echo "==> Configure-only requested; stopping."
    exit 0
fi

echo "==> Building z_generals (libmain.so) + DXVK d3d8/d3d9"
# GeneralsX @bugfix Android port 10/07/2026 main_hook/file_redirect_hook/
# gsl_alloc_hook/hook_impl (cmake/adrenotools.cmake) are separate shared
# libraries adrenotools_open_libvulkan() dlopen()s by path at runtime -- they
# are NOT a link-time dependency of z_generals (which only links the static
# "adrenotools" lib), so a targeted Ninja build never produces them unless
# named explicitly here. Missing this list is exactly what made
# package-android-zh.sh's "libmain_hook.so not found" check fail on the
# first CI run of the Custom Vulkan Driver feature.
# GeneralsX @build Android port 08/09/2026 Force the crash handler to recompile every
# build, so the "[build compiled ...]" stamp it prints is the stamp of THIS build.
# It bakes in __DATE__/__TIME__, and nothing normally edits that file, so an
# incremental build left the stamp frozen at whenever it was last touched -- which
# made every device log claim the same build date regardless of what was actually
# installed. Several rounds of a bug hunt were spent unable to tell one build from
# another because of it. One touch is cheaper than that ambiguity.
touch "${PROJECT_ROOT}/GeneralsMD/Code/Main/AndroidCrashHandler.cpp"

cmake --build "${BUILD_DIR}" --target z_generals dxvk_d3d8_install \
    main_hook file_redirect_hook gsl_alloc_hook hook_impl

# --- artifact verification ---------------------------------------------------
# Silent fallbacks all exit 0; check what actually got built.
READELF="$(ls "${ANDROID_NDK_HOME}"/toolchains/llvm/prebuilt/*/bin/llvm-readelf 2>/dev/null | head -1)"
[[ -n "${READELF}" ]] || READELF="readelf"

GAME_LIB="$(find "${BUILD_DIR}" -name libmain.so -not -path "*/_deps/*" | head -1)"
if [[ -z "${GAME_LIB}" ]]; then
    echo "ERROR: libmain.so not found under ${BUILD_DIR}"
    exit 1
fi
# NOTE: capture first, then grep. Under `set -o pipefail`, `readelf | grep -q`
# reports the pipeline as failed whenever grep exits on its first match before
# readelf has finished writing, killing readelf with SIGPIPE (status 141) -- a
# race that fails a perfectly good AArch64 build at random.
GAME_LIB_ELF_HEADER="$("${READELF}" -h "${GAME_LIB}")"
if ! grep -q "AArch64" <<< "${GAME_LIB_ELF_HEADER}"; then
    echo "ERROR: ${GAME_LIB} is not AArch64 — wrong toolchain reached the build."
    exit 1
fi

for lib in libdxvk_d3d8.so libdxvk_d3d9.so; do
    if [[ ! -f "${BUILD_DIR}/${lib}" ]]; then
        echo "ERROR: ${BUILD_DIR}/${lib} missing — DXVK build failed upstream of packaging."
        exit 1
    fi
done
DXVK_D3D9_STRINGS="$(strings "${BUILD_DIR}/libdxvk_d3d9.so" || true)"
if ! grep -q "Sdl3WsiDriver" <<< "${DXVK_D3D9_STRINGS}"; then
    echo "ERROR: libdxvk_d3d9.so was built WITHOUT the SDL3 WSI (silent SDL2/none fallback)."
    echo "       The game window is SDL3; this DXVK cannot present. Check the meson"
    echo "       configure log in ${BUILD_DIR}/_deps/dxvk-build-android/meson-logs/."
    exit 1
fi

echo "==> OK"
echo "    game:  ${GAME_LIB}"
echo "    dxvk:  ${BUILD_DIR}/libdxvk_d3d8.so + libdxvk_d3d9.so (SDL3 WSI verified)"
echo "Next: ./scripts/build/android/package-android-zh.sh"
