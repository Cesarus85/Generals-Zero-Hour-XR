#!/usr/bin/env bash
# GeneralsX @test Codex 14/09/2026 Native tactical bridge, no game data needed.
set -euo pipefail
export CPATH="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../Core/Libraries/Source/d3d8gles/include" && pwd)${CPATH:+:$CPATH}"
repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$repo_dir"
build_dir="${1:-build/android-vulkan}"
test_dir="$(mktemp -d "${TMPDIR:-/tmp}/generals-tactical-test.XXXXXX")"
for name in xrSetWaypointQueue xrCancelTactics xrIssueGuard xrIssueMovement xrUpdateBookmarkSession XrGameBoot_FormationActive XrGameBoot_TacticalReason XrGameBoot_BookmarkKnown XrGameBoot_Bookmark XrGameBoot_CancelTarget XrGameBoot_TacticalAction; do
 sed -n -E "/^(static )?(bool|void|std::string) ${name}\\(/,/^}/p" GeneralsMD/Code/Main/XrGameBoot.cpp
done > "$test_dir/xr-tactical-bridge.inc"
"${CXX:-clang++}" -std=c++17 -Wall -Wextra -Werror -fsanitize=undefined \
 -IGeneralsMD/Code/Main -I"$test_dir" -I"$build_dir/vcpkg_installed/arm64-android/include" \
 scripts/qa/xr-tactical-bridge-test.cpp -o "$test_dir/tactical-test"
"$test_dir/tactical-test"
