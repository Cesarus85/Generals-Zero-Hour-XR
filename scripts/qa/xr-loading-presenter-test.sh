#!/usr/bin/env bash
# GeneralsX @test Codex 14/09/2026 Actual presenter with deterministic XR spies.
# Usage: bash scripts/qa/xr-loading-presenter-test.sh [configured-android-build]
set -euo pipefail
export CPATH="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../Core/Libraries/Source/d3d8gles/include" && pwd)${CPATH:+:$CPATH}"
repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$repo_dir"
build_dir="${1:-build/android-vulkan}"
test_dir="$(mktemp -d "${TMPDIR:-/tmp}/generals-loading-test.XXXXXX")"
sed -n '/^struct XrLoadingPresenter {/,/^};/p' GeneralsMD/Code/Main/XrHello.cpp > "$test_dir/xr-loading-presenter.inc"
"${CXX:-clang++}" -std=c++17 -Wall -Wextra -Werror -Wno-missing-field-initializers -fsanitize=undefined \
 -IGeneralsMD/Code/Main -I"$test_dir" -I"$build_dir/vcpkg_installed/arm64-android/include" \
 scripts/qa/xr-loading-presenter-test.cpp -o "$test_dir/presenter-test"
"$test_dir/presenter-test"
