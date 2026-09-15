#!/usr/bin/env bash
# GeneralsX @test Codex 14/09/2026 Compile actual XR group/language functions.
# Usage: bash scripts/qa/xr-console-bridge-test.sh [configured-android-build]
set -euo pipefail
export CPATH="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../Core/Libraries/Source/d3d8gles/include" && pwd)${CPATH:+:$CPATH}"
repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$repo_dir"
build_dir="${1:-build/android-vulkan}"
test_dir="$(mktemp -d "${TMPDIR:-/tmp}/generals-console-test.XXXXXX")"
for name in TacticalGroup GroupSize Communicator SetLanguage LanguageStatus ViewBase; do
  sed -n -E "/^(void|int|bool|std::string) XrGameBoot_${name}\(/,/^}/p" GeneralsMD/Code/Main/XrGameBoot.cpp
done > "$test_dir/xr-console-bridge.inc"
"${CXX:-clang++}" -std=c++17 -Wall -Wextra -Werror -fsanitize=undefined \
 -IGeneralsMD/Code/Main -I"$test_dir" -I"$build_dir/vcpkg_installed/arm64-android/include" \
 scripts/qa/xr-console-bridge-test.cpp -o "$test_dir/console-test"
"$test_dir/console-test" "$test_dir/game-language.cfg"
