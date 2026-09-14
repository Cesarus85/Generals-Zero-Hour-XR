#!/usr/bin/env bash
# GeneralsX @test Codex 14/09/2026 Production XR frame with shadow-state spies.
# Usage: bash scripts/qa/xr-shadow-scope-test.sh [configured-android-build]
# CXX overrides the host compiler; no device, game data or network access.
set -euo pipefail
export CPATH="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../Core/Libraries/Source/d3d8gles/include" && pwd)${CPATH:+:$CPATH}"
repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$repo_dir"
build_dir="${1:-build/android-vulkan}"
test_dir="$(mktemp -d "${TMPDIR:-/tmp}/generals-shadow-test.XXXXXX")"
sed -n '/^Bool XrGameBoot_Frame()/,/^}/p' GeneralsMD/Code/Main/XrGameBoot.cpp > "$test_dir/xr-shadow-scope.inc"
"${CXX:-clang++}" -std=c++17 -Wall -Wextra -Werror -fsanitize=undefined \
 -IGeneralsMD/Code/Main -I"$test_dir" -I"$build_dir/vcpkg_installed/arm64-android/include" \
 scripts/qa/xr-shadow-scope-test.cpp -o "$test_dir/shadow-test"
"$test_dir/shadow-test"
