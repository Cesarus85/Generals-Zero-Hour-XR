#!/usr/bin/env bash
# GeneralsX @test 19/09/2026 threads_compat CRITICAL_SECTION nesting; host-only.
# Usage: bash scripts/qa/critsec-compat-test.sh
# CXX overrides the host compiler.
set -euo pipefail
repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$repo_dir"
test_dir="$(mktemp -d "${TMPDIR:-/tmp}/generals-critsec-test.XXXXXX")"
"${CXX:-clang++}" -std=c++17 -Wall -Wextra -Werror -fsanitize=undefined -D_UNIX \
 -isystem GeneralsMD/Code/CompatLib/Include \
 scripts/qa/critsec-compat-test.cpp -o "$test_dir/critsec-test" -pthread
"$test_dir/critsec-test"
