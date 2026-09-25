#!/usr/bin/env bash
# GeneralsX @test Codex 14/09/2026 Compile actual dialog detection and geometry.
# Usage: bash scripts/qa/xr-workspace-test.sh [configured-android-build]
# Environment: CXX (host clang++ by default). No device or game-data access.
set -euo pipefail
export CPATH="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../Core/Libraries/Source/d3d8gles/include" && pwd)${CPATH:+:$CPATH}"
repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$repo_dir"
build_dir="${1:-build/android-vulkan}"
test_dir="$(mktemp -d "${TMPDIR:-/tmp}/generals-workspace-test.XXXXXX")"
{
  sed -n '/^bool XrGameBoot_ExpandedUI(/,/^}/p' GeneralsMD/Code/Main/XrGameBoot.cpp
  sed -n '/^const char \*XrGameBoot_CinematicBlocker(/,/^}/p' GeneralsMD/Code/Main/XrGameBoot.cpp
  sed -n '/^const char \*XrGameBoot_PresentationBlocker(/,/^}/p' GeneralsMD/Code/Main/XrGameBoot.cpp
  sed -n '/^bool GX_XR_SplitUIAllowed() {/,/^}/p' GeneralsMD/Code/Main/XrGameBoot.cpp
  sed -n '/^bool XrGameBoot_CanControlCamera(/,/^}/p' GeneralsMD/Code/Main/XrGameBoot.cpp
  sed -n '/^bool XrGameBoot_CameraPreset(/,/^}/p' GeneralsMD/Code/Main/XrGameBoot.cpp
  sed -n '/^bool XrGameBoot_AdjustCamera(/,/^}/p' GeneralsMD/Code/Main/XrGameBoot.cpp
  sed -n '/^std::string XrGameBoot_PresentationStatus(/,/^}/p' GeneralsMD/Code/Main/XrGameBoot.cpp
  sed -n '/^bool XrGameBoot_CanStereoWorld(/,/^}/p' GeneralsMD/Code/Main/XrGameBoot.cpp
  sed -n '/^static XrGameRect surfaceRect(/,/^}/p' GeneralsMD/Code/Main/XrHello.cpp
  sed -n '/^static float surfaceAspect(/,/^}/p' GeneralsMD/Code/Main/XrHello.cpp
  sed -n '/^static XrSurface displayedSurface(/,/^}/p' GeneralsMD/Code/Main/XrHello.cpp
} > "$test_dir/xr-workspace-bridge.inc"
for lan_preview in 0 1; do
  "${CXX:-clang++}" -std=c++17 -Wall -Wextra -Werror -fsanitize=undefined \
    -D__ANDROID__=1 -DGX_XR_LAN_PREVIEW="$lan_preview" \
    -IGeneralsMD/Code/Main -I"$test_dir" -I"$build_dir/vcpkg_installed/arm64-android/include" \
    scripts/qa/xr-workspace-test.cpp -o "$test_dir/workspace-test"
  "$test_dir/workspace-test" "$test_dir/layout-$lan_preview.cfg"
done
