#!/usr/bin/env bash
# GeneralsX @test Codex 13/09/2026 Compile the real terrain-cast regression.
# Usage: ANDROID_NDK_HOME=... bash scripts/qa/xr-terrain-device-test.sh [build-tree]
# Prints a temporary ARM64 executable; manually push/run with an explicit adb serial.
set -euo pipefail
repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$repo_dir"
build_dir="${1:-build/android-vulkan}"
: "${ANDROID_NDK_HOME:?Set ANDROID_NDK_HOME to the matching Android NDK}"
case "$(uname -s)" in
  Darwin) ndk_host=darwin-x86_64 ;;
  Linux) ndk_host=linux-x86_64 ;;
  *) echo 'Unsupported NDK host' >&2; exit 1 ;;
esac
test_dir="$(mktemp -d "${TMPDIR:-/tmp}/generals-terrain-test.XXXXXX")"
# Generated compilation input, extracted verbatim: never maintain a second
# copy of the collision algorithm in the fixture.
sed -n '/^bool BaseHeightMapRenderObjClass::Cast_Ray(/,/^}/p' \
  Core/GameEngineDevice/Source/W3DDevice/GameClient/BaseHeightMap.cpp > "$test_dir/xr-terrain-cast.inc"
test -s "$test_dir/xr-terrain-cast.inc"
"$ANDROID_NDK_HOME/toolchains/llvm/prebuilt/$ndk_host/bin/aarch64-linux-android29-clang++" \
  -std=c++20 -DLINUX -DNDEBUG -DDISABLE_GAMEMEMORY=1 -DSAGE_USE_GLM -static-libstdc++ \
  -include Utility/CppMacros.h -I"$test_dir" \
  -Ireferences/fbraz3-dxvk/include/native -Ireferences/fbraz3-dxvk/include/native/windows \
  -Ireferences/fbraz3-dxvk/include/native/directx \
  -IGenerals/Code/CompatLib/Include -IGeneralsMD/Code/CompatLib/Include \
  -ICore/Libraries/Source/WWVegas -ICore/Libraries/Source/WWVegas/WWLib \
  -ICore/Libraries/Source/WWVegas/WWDebug -ICore/Libraries/Source/WWVegas/WWMath \
  -ICore/Libraries/Source/WWVegas/WW3D2 -IDependencies/Utility -ICore/Libraries/Include \
  -I"$build_dir/vcpkg_installed/arm64-android/include" \
  scripts/qa/xr-terrain-device-test.cpp \
  "$build_dir/Core/Libraries/Source/WWVegas/WWMath/libwwmath.a" -o "$test_dir/terrain-test"
echo "ARM64 test: $test_dir/terrain-test"
