#!/usr/bin/env bash
# GeneralsX @test Codex 14/09/2026 Production build-angle bridge and edit geometry.
# Usage: bash scripts/qa/xr-build-controls-test.sh [configured-android-build]
# Environment: CXX (host compiler). No device or game-data writes.
set -euo pipefail
repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$repo_dir"
test_dir="$(mktemp -d "${TMPDIR:-/tmp}/generals-build-controls.XXXXXX")"
{
 sed -n '/^bool W3DInGameUI::rotateXrPlacement(/,/^}/p' GeneralsMD/Code/GameEngineDevice/Source/W3DDevice/GameClient/W3DInGameUI.cpp
 sed -n -e '/^bool XrGameBoot_CanRotatePlacement(/,/^}/p' -e '/^bool XrGameBoot_RotatePlacement(/,/^}/p' -e '/^float XrGameBoot_PlacementDegrees(/,/^}/p' GeneralsMD/Code/Main/XrGameBoot.cpp
} > "$test_dir/xr-build-controls.inc"
"${CXX:-clang++}" -std=c++17 -Wall -Wextra -Werror -fsanitize=undefined \
 -IGeneralsMD/Code/Main -ICore/Libraries/Source/d3d8gles/include -I"$test_dir" \
 -I"${1:-build/android-vulkan}/vcpkg_installed/arm64-android/include" \
 scripts/qa/xr-build-controls-test.cpp -o "$test_dir/build-controls-test"
"$test_dir/build-controls-test"
