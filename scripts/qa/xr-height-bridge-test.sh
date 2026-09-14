#!/usr/bin/env bash
# GeneralsX @test Codex 14/09/2026 Compile the actual P18 mapping adapter.
# Usage: bash scripts/qa/xr-height-bridge-test.sh [configured-android-build]
set -euo pipefail
repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$repo_dir"
build_dir="${1:-build/android-vulkan}"
test_dir="$(mktemp -d "${TMPDIR:-/tmp}/generals-height-bridge.XXXXXX")"
sed -n '/^static bool xrPrepareWorldMapping(/,/^}/p' GeneralsMD/Code/Main/XrGameBoot.cpp > "$test_dir/height-bridge.inc"
"${CXX:-clang++}" -std=c++17 -Wall -Wextra -Werror -fsanitize=undefined \
 -IGeneralsMD/Code/Main -ICore/Libraries/Source/d3d8gles/include -I"$test_dir" \
 -I"$build_dir/vcpkg_installed/arm64-android/include" scripts/qa/xr-height-bridge-test.cpp -o "$test_dir/test"
"$test_dir/test"
