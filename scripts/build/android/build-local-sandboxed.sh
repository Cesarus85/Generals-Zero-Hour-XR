#!/usr/bin/env bash
# Build the Android APK entirely inside a sandboxed dev environment whose
# egress policy blocks raw HTTPS to github.com/codeload.github.com but
# allows plain `git clone`/`fetch` to the same host (this is how the Claude
# Code web/cloud sandbox is configured; see
# docs/BUILD/ANDROID_SANDBOXED_LOCAL.md for the full story and for what to
# do on a handful of small binary assets this script can't fetch itself).
#
# On a normal machine with unrestricted GitHub access, just use
# ./scripts/build/android/build-android-zh.sh + package-android-zh.sh
# directly -- this script only exists to route around that one constraint.
set -euo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
cd "$REPO"

PRESET=android-vulkan
NDK_VERSION="27.2.12479018"
CMDLINE_TOOLS_VERSION="13114758"
CMDLINE_TOOLS_SHA1="5fdcc763663eefb86a5b8879697aa6088b041e70"
VCPKG_COMMIT="42e4e33e1505c9f47b58c21e0f557c1571b751ee"
FETCHCONTENT_SRC="/opt/fetchcontent-src"
SDL3_TAG="release-3.4.2"
SDL3_IMAGE_TAG="release-3.4.0"
OPENAL_SOFT_TAG="1.24.2"

echo "=== [1/8] apt: ccache ==="
sudo apt-get update -qq
sudo apt-get install -y -qq --no-install-recommends ccache
ccache --version | head -1

echo "=== [2/8] Android SDK (cmdline-tools + NDK + platform-35 + build-tools) ==="
# dl.google.com is not blocked -- this step needs no workaround.
if [ ! -f "/opt/android-sdk/ndk/${NDK_VERSION}/build/cmake/android.toolchain.cmake" ]; then
  curl -fL --retry 3 -o /tmp/cmdline-tools.zip \
    "https://dl.google.com/android/repository/commandlinetools-linux-${CMDLINE_TOOLS_VERSION}_latest.zip"
  echo "${CMDLINE_TOOLS_SHA1}  /tmp/cmdline-tools.zip" | sha1sum -c -
  sudo rm -rf /opt/android-sdk/cmdline-tools
  sudo mkdir -p /opt/android-sdk/cmdline-tools
  sudo unzip -q /tmp/cmdline-tools.zip -d /opt/android-sdk/cmdline-tools
  sudo mv /opt/android-sdk/cmdline-tools/cmdline-tools /opt/android-sdk/cmdline-tools/latest
  sudo chown -R "$(id -u):$(id -g)" /opt/android-sdk
  rm /tmp/cmdline-tools.zip
  SDKMANAGER=/opt/android-sdk/cmdline-tools/latest/bin/sdkmanager
  yes | "$SDKMANAGER" --licenses >/dev/null || true
  "$SDKMANAGER" --install "ndk;${NDK_VERSION}" "platforms;android-35" "build-tools;35.0.0" "platform-tools"
else
  echo "NDK already present, skipping SDK install"
fi

export ANDROID_HOME=/opt/android-sdk
export ANDROID_SDK_ROOT=/opt/android-sdk
export ANDROID_NDK_HOME="/opt/android-sdk/ndk/${NDK_VERSION}"
test -f "${ANDROID_NDK_HOME}/build/cmake/android.toolchain.cmake" || { echo "NDK toolchain file missing"; exit 1; }

echo "=== [3/8] ccache config ==="
export CCACHE_DIR="$HOME/.cache/ccache"
ccache --set-config=max_size=3G
ccache --set-config=compiler_check=content
ccache --zero-stats

echo "=== [4/8] Bootstrap vcpkg (pinned commit ${VCPKG_COMMIT}) + sandbox patches ==="
if [ ! -d /opt/vcpkg/.git ]; then
  sudo mkdir -p /opt/vcpkg
  sudo chown -R "$(id -u):$(id -g)" /opt/vcpkg
  git clone https://github.com/microsoft/vcpkg.git /opt/vcpkg
fi
CURRENT="$(git -C /opt/vcpkg rev-parse HEAD || echo none)"
if [ "$CURRENT" != "${VCPKG_COMMIT}" ]; then
  git -C /opt/vcpkg fetch origin "${VCPKG_COMMIT}"
  git -C /opt/vcpkg checkout "${VCPKG_COMMIT}"
  rm -f /opt/vcpkg/vcpkg
fi
export VCPKG_ROOT=/opt/vcpkg
# Replaces vcpkg's hash-checked HTTPS download of GitHub source archives
# with a git-clone-based fallback for the exact same URL shapes -- see
# vcpkg-sandbox-patches/apply.sh and the doc for why.
"${REPO}/scripts/build/android/vcpkg-sandbox-patches/apply.sh"
if [ ! -f /opt/vcpkg/vcpkg ]; then
  # bootstrap-vcpkg.sh's own download of the vcpkg-tool release binary hits
  # the same 403; see docs/BUILD/ANDROID_SANDBOXED_LOCAL.md "One-off binary
  # assets" for how to place it manually before this point.
  /opt/vcpkg/bootstrap-vcpkg.sh -disableMetrics
fi

echo "=== [5/8] Pre-fetch CMake FetchContent sources vcpkg's patches don't cover ==="
# SDL3/SDL3_image/openal-soft are pulled in by cmake/sdl3.cmake and
# cmake/openal.cmake via CMake's own FetchContent(URL ...), not vcpkg -- the
# git-clone patches above don't apply to CMake's built-in downloader. Same
# fix, different mechanism: clone the pinned release tag and point
# FETCHCONTENT_SOURCE_DIR_<NAME> at it so FetchContent uses it directly
# and never tries to download the release tarball itself.
mkdir -p "${FETCHCONTENT_SRC}"
for entry in "SDL3-src:https://github.com/libsdl-org/SDL.git:${SDL3_TAG}" \
             "SDL3_image-src:https://github.com/libsdl-org/SDL_image.git:${SDL3_IMAGE_TAG}" \
             "openal-soft-src:https://github.com/kcat/openal-soft.git:${OPENAL_SOFT_TAG}"; do
  dir="${entry%%:*}"; rest="${entry#*:}"; repo_url="${rest%:*}"; tag="${rest##*:}"
  if [ ! -d "${FETCHCONTENT_SRC}/${dir}/.git" ]; then
    git clone --quiet --depth 1 --branch "${tag}" "${repo_url}" "${FETCHCONTENT_SRC}/${dir}"
  fi
done

echo "=== [6/8] Configure CMake (Android) ==="
mkdir -p logs
cmake --preset "${PRESET}" \
  -DCMAKE_C_COMPILER_LAUNCHER=ccache \
  -DCMAKE_CXX_COMPILER_LAUNCHER=ccache \
  -DANDROID_CI_BUILD_NUMBER=$(date +%s) \
  -DFETCHCONTENT_SOURCE_DIR_SDL3="${FETCHCONTENT_SRC}/SDL3-src" \
  -DFETCHCONTENT_SOURCE_DIR_SDL3_IMAGE="${FETCHCONTENT_SRC}/SDL3_image-src" \
  -DFETCHCONTENT_SOURCE_DIR_OPENAL_SOFT="${FETCHCONTENT_SRC}/openal-soft-src" \
  ${GX_EXTRA_CMAKE:-} \
  2>&1 | tee logs/configure_android.log

# DXVK's meson build finds SDL3 via a generated sdl3.pc that hardcodes
# includedir=<build>/_deps/sdl3-src/include (the normal FetchContent
# download layout). FETCHCONTENT_SOURCE_DIR_SDL3 above redirects the real
# checkout elsewhere, but CMake's FetchContent still creates an empty
# _deps/sdl3-src placeholder directory even with the override active --
# `ln -sfn` onto an existing directory nests the link inside it instead of
# replacing it, so remove the placeholder first.
mkdir -p "build/${PRESET}/_deps"
rm -rf "build/${PRESET}/_deps/sdl3-src"
ln -sfn "${FETCHCONTENT_SRC}/SDL3-src" "build/${PRESET}/_deps/sdl3-src"

echo "=== [7/8] Build z_generals + DXVK d3d8/d3d9 + hooks ==="
cmake --build "build/${PRESET}" --target z_generals dxvk_d3d8_install \
  main_hook file_redirect_hook gsl_alloc_hook hook_impl -- -k 0 2>&1 | tee logs/build_android.log
ccache --show-stats

echo "=== [8/8] Verify, strip, package APK ==="
READELF="$(ls "${ANDROID_NDK_HOME}"/toolchains/llvm/prebuilt/*/bin/llvm-readelf | head -1)"
GAME_LIB="build/${PRESET}/GeneralsMD/Code/Main/libmain.so"
[ -f "$GAME_LIB" ] || { echo "libmain.so not found at $GAME_LIB"; exit 1; }
readelf_out="$("$READELF" -h "$GAME_LIB")"
grep -q AArch64 <<< "$readelf_out" || { echo "libmain.so is not AArch64"; exit 1; }
for lib in libdxvk_d3d8.so libdxvk_d3d9.so; do
  [ -f "build/${PRESET}/$lib" ] || { echo "$lib missing"; exit 1; }
done
# "Sdl3WsiDriver" is a C++ class name, not a string literal, so it isn't
# retained as raw text in a release binary -- "SDL3 WSI:" (its runtime
# log-message prefix) is. Capturing to a variable first, rather than piping
# `strings` straight into `grep -q`, sidesteps a pipefail/SIGPIPE trap: grep
# exits right after the first match and closes its end of the pipe, and
# `strings` (still mid-write on a multi-MB .so) gets SIGPIPE for it, which
# pipefail then reports as the pipeline failing even though the match was
# found.
dxvk_strings="$(strings "build/${PRESET}/libdxvk_d3d9.so")"
grep -q "SDL3 WSI:" <<< "$dxvk_strings" || { echo "libdxvk_d3d9.so built without SDL3 WSI"; exit 1; }
echo "Artifacts verified: AArch64 libmain.so + DXVK with SDL3 WSI"

# Strip debug symbols from everything except the two DXVK libraries before
# packaging: android/app/build.gradle deliberately keeps libdxvk_d3d8/d3d9.so
# unstripped (keepDebugSymbols) because stripping them crashes DXVK's draw
# path on real hardware -- see the comment there. AGP's own stripDebugSymbols
# task fails silently on all our custom-built libraries for an unrelated
# reason ("Unable to strip ... packaging them as they are"), so an unstripped
# libmain.so alone (282MB) otherwise ships straight into the APK.
STRIP="${ANDROID_NDK_HOME}/toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-strip"
for lib in "$GAME_LIB" \
           "build/${PRESET}/_deps/sdl3-build/libSDL3.so" \
           "build/${PRESET}/_deps/sdl3_image-build/libSDL3_image.so" \
           "build/${PRESET}/_deps/openal_soft-build/libopenal.so" \
           "build/${PRESET}/libgamespy.so" \
           "build/${PRESET}/_deps/adrenotools-build/libadrenotools.so" \
           "build/${PRESET}/_deps/adrenotools-build/src/hook/libgsl_alloc_hook.so" \
           "build/${PRESET}/_deps/adrenotools-build/src/hook/libhook_impl.so" \
           "build/${PRESET}/_deps/adrenotools-build/src/hook/libmain_hook.so" \
           "build/${PRESET}/_deps/adrenotools-build/src/hook/libfile_redirect_hook.so"; do
  [ -f "$lib" ] && "$STRIP" --strip-unneeded "$lib"
done

export GX_ANDROID_STAGING="${REPO}/android-staging"
# GeneralsX @feature Android port 01/08/2026 Respect a caller-provided
# override instead of always clearing it -- lets a one-off diagnostic build
# force a distinct versionCode/versionName so a tester can be certain a
# fresh APK actually replaced the one already installed on their device,
# rather than relying on the debug keystore's default 100/1.0.0 every time.
export GX_ANDROID_VERSION_CODE="${GX_ANDROID_VERSION_CODE:-}"
export GX_ANDROID_VERSION_NAME="${GX_ANDROID_VERSION_NAME:-}"
if [ -x /opt/gradle/bin/gradle ]; then
  export PATH="/opt/gradle/bin:$PATH"
fi
# Fonts (docs/BUILD/ANDROID_SANDBOXED_LOCAL.md "One-off binary assets") must
# already be staged at ${GX_ANDROID_STAGING}/fonts/*.ttf, and the default
# Turnip driver at ${GX_ANDROID_STAGING}/default_driver/{meta.json,*.so} --
# both are fetched from github.com release assets, which this script can't
# reach itself.
# Always package from a clean Gradle build dir. AGP's incremental packager
# can leave superseded entries physically in the zip when a library shrinks,
# rewriting only the central directory -- the .apk then carries dead weight
# that no amount of stripping removes. This has bitten twice now: once when
# libmain.so went 282MB -> 41MB post-strip and the .apk didn't shrink one
# byte, and again at 58MB where 18MB turned out to be orphaned. Re-running
# packaging costs ~10s against a native build measured in minutes, so pay it
# unconditionally rather than shipping a mystery-sized APK.
rm -rf android/app/build

./scripts/build/android/package-android-zh.sh

APK="android/app/build/outputs/apk/debug/app-debug.apk"
if [ -f "$APK" ]; then
  cp "$APK" "${REPO}/GeneralsXZH-android-local.apk"
  echo "=== BUILD SUCCEEDED: ${REPO}/GeneralsXZH-android-local.apk ($(du -h "$APK" | cut -f1)) ==="

  # Guard against the packager regressing: every byte in the file should
  # belong to a live zip entry, give or take the central directory itself.
  python3 - "$APK" <<'PY' || true
import os, sys, zipfile
path = sys.argv[1]
live = sum(i.compress_size for i in zipfile.ZipFile(path).infolist())
orphaned = os.path.getsize(path) - live
if orphaned > 2 * 1024 * 1024:
    print(f"WARNING: {orphaned/1048576:.1f}MB of this APK belongs to no zip entry "
          f"-- stale incremental packaging, the APK is bloated.")
PY
else
  echo "=== BUILD FAILED: APK not found at $APK ==="
  exit 1
fi
