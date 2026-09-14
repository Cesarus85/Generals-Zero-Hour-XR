#!/usr/bin/env bash
# GeneralsX @test Codex 14/09/2026 Compile W3D pickDrawable verbatim with spies.
# Usage: bash scripts/qa/xr-pick-bridge-test.sh
set -euo pipefail
repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$repo_dir"
test_dir="$(mktemp -d "${TMPDIR:-/tmp}/generals-pick-test.XXXXXX")"
sed -n '/^Drawable \*W3DView::pickDrawable(/,/^}/p' Core/GameEngineDevice/Source/W3DDevice/GameClient/W3DView.cpp > "$test_dir/xr-pick-bridge.inc"
test -s "$test_dir/xr-pick-bridge.inc"
"${CXX:-clang++}" -std=c++17 -Wall -Wextra -Werror -fsanitize=undefined -I"$test_dir" \
  scripts/qa/xr-pick-bridge-test.cpp -o "$test_dir/pick-test"
"$test_dir/pick-test"
