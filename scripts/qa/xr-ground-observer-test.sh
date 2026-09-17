#!/usr/bin/env bash
# GeneralsX @test Codex 17/09/2026 Host P25 state/mapping and frame-order checks.
set -euo pipefail
repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$repo_dir"
build_dir="${1:-build/android-vulkan}"
test_dir="$(mktemp -d "${TMPDIR:-/tmp}/generals-ground-observer.XXXXXX")"
"${CXX:-clang++}" -std=c++17 -Wall -Wextra -Werror -fsanitize=undefined \
 -IGeneralsMD/Code/Main -ICore/Libraries/Source/d3d8gles/include \
 -I"$build_dir/vcpkg_installed/arm64-android/include" \
 scripts/qa/xr-ground-observer-test.cpp -o "$test_dir/test"
"$test_dir/test" GeneralsMD/Code/Main/XrHello.cpp GeneralsMD/Code/Main/XrGameBoot.cpp
