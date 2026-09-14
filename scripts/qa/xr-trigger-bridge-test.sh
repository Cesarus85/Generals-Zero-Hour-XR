#!/usr/bin/env bash
# GeneralsX @test Codex 13/09/2026 Test production drag classification and dispatch.
# Usage: bash scripts/qa/xr-trigger-bridge-test.sh [build-tree]
# Environment: CXX (host compiler, defaults to clang++). No device writes.
set -euo pipefail
export CPATH="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../Core/Libraries/Source/d3d8gles/include" && pwd)${CPATH:+:$CPATH}"
repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$repo_dir"
build_dir="${1:-build/android-vulkan}"
test_dir="$(mktemp -d "${TMPDIR:-/tmp}/generals-trigger-test.XXXXXX")"
sed -n '/^void XrGameBoot_SpatialTrigger(/,/^}/p' GeneralsMD/Code/Main/XrGameBoot.cpp > "$test_dir/xr-trigger-bridge.inc"
test -s "$test_dir/xr-trigger-bridge.inc"
"${CXX:-clang++}" -std=c++17 -Wall -Wextra -Werror -fsanitize=undefined \
  -IGeneralsMD/Code/Main -I"$test_dir" -I"$build_dir/vcpkg_installed/arm64-android/include" \
  scripts/qa/xr-trigger-bridge-test.cpp -o "$test_dir/trigger-test"
"$test_dir/trigger-test"
