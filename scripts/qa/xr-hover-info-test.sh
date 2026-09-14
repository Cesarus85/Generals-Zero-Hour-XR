#!/usr/bin/env bash
# GeneralsX @test Codex 14/09/2026 Production command description, host only.
# Usage: bash scripts/qa/xr-hover-info-test.sh
# Environment: CXX. No device/game assets required.
set -euo pipefail
repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$repo_dir"
test_dir="$(mktemp -d "${TMPDIR:-/tmp}/generals-hover-info.XXXXXX")"
sed -n '/^UnsignedInt ControlBar::describeCommand(/,/^}/p' Core/GameEngine/Source/GameClient/GUI/GUICallbacks/ControlBarPopupDescription.cpp > "$test_dir/xr-hover-info.inc"
"${CXX:-clang++}" -std=c++17 -Wall -Wextra -Werror -fsanitize=undefined -I"$test_dir" scripts/qa/xr-hover-info-test.cpp -o "$test_dir/hover-info-test"
"$test_dir/hover-info-test"
