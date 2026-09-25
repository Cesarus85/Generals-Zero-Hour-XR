#!/usr/bin/env bash
# GeneralsX @test Muse 16/09/2026 Match-result latch, dismiss, reset and card pose.
# Usage: bash scripts/qa/xr-endgame-test.sh
# Environment: CXX. Host only; no device, assets or engine singletons.
set -euo pipefail
repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$repo_dir"
test_dir="$(mktemp -d "${TMPDIR:-/tmp}/generals-endgame-test.XXXXXX")"
"${CXX:-clang++}" -std=c++17 -Wall -Wextra -Werror -fsanitize=undefined \
 -IGeneralsMD/Code/Main \
 scripts/qa/xr-endgame-test.cpp -o "$test_dir/endgame-test"
"$test_dir/endgame-test"
