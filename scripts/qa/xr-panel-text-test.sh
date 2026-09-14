#!/usr/bin/env bash
# GeneralsX @test Codex 14/09/2026 Check actual localized Canvas payloads.
# Usage: bash scripts/qa/xr-panel-text-test.sh [configured-android-build]
set -euo pipefail
export CPATH="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../Core/Libraries/Source/d3d8gles/include" && pwd)${CPATH:+:$CPATH}"
repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$repo_dir"
build_dir="${1:-build/android-vulkan}"
test_dir="$(mktemp -d "${TMPDIR:-/tmp}/generals-panel-test.XXXXXX")"
sed -n -e '/^static std::string commandExplanation(/,/^}/p' -e '/^static void updateMenuTextures(/,/^}/p' GeneralsMD/Code/Main/XrMenuPainting.h > "$test_dir/xr-panel-text-bridge.inc"
"${CXX:-clang++}" -std=c++17 -Wall -Wextra -Werror -fsanitize=undefined \
 -IGeneralsMD/Code/Main -I"$test_dir" -I"$build_dir/vcpkg_installed/arm64-android/include" \
 scripts/qa/xr-panel-text-test.cpp -o "$test_dir/panel-test"
"$test_dir/panel-test" "$test_dir/panels.bin"
echo "Canvas payloads: $test_dir/panels.bin"
