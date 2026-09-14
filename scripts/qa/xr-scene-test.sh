#!/usr/bin/env bash
# GeneralsX @test Codex 14/09/2026 Real scene state/geometry and modal routing.
# Usage: bash scripts/qa/xr-scene-test.sh [configured-android-build]
# Environment: CXX. Host only; no permissions, room data or device mutations.
set -euo pipefail
repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$repo_dir"
test_dir="$(mktemp -d "${TMPDIR:-/tmp}/generals-scene-test.XXXXXX")"
sed -n '/^static void requestSceneData(/,$p' GeneralsMD/Code/Main/XrSceneUI.h > "$test_dir/xr-scene-ui.inc"
"${CXX:-clang++}" -std=c++17 -Wall -Wextra -Werror -Wno-missing-field-initializers -fsanitize=undefined \
 -IGeneralsMD/Code/Main -ICore/Libraries/Source/d3d8gles/include -I"$test_dir" \
 -I"${1:-build/android-vulkan}/vcpkg_installed/arm64-android/include" \
 scripts/qa/xr-scene-test.cpp -o "$test_dir/scene-test"
"$test_dir/scene-test"
