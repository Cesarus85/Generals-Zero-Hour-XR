#!/usr/bin/env bash
# GeneralsX @test 19/09/2026 POSIX ThreadClass revival; host-only, no device,
# game data or network access. DXVK_NATIVE_INC overrides the DXVK native
# headers when the references/fbraz3-dxvk submodule is not initialized.
# Usage: bash scripts/qa/wlib-thread-test.sh
# CXX overrides the host compiler.
set -euo pipefail
repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$repo_dir"
dxvk="${DXVK_NATIVE_INC:-$repo_dir/references/fbraz3-dxvk/include/native}"
test_dir="$(mktemp -d "${TMPDIR:-/tmp}/generals-thread-test.XXXXXX")"
# -isystem keeps pre-existing header warnings (compat shims) from failing the
# strict test TU; warnings in the compiled .cpp files themselves still apply.
includes="-isystem Core/Libraries/Source/WWVegas/WWLib -isystem Core/Libraries/Source/WWVegas/WWDebug -isystem Core/Libraries/Source/WWVegas -isystem GeneralsMD/Code/CompatLib/Include -isystem Dependencies/Utility -isystem Core/Libraries/Include -isystem $dxvk/windows -isystem $dxvk/directx -isystem $dxvk"
# The test TU stays strict; the linked WWLib/CompatLib TUs carry pre-existing
# -Wextra/-Werror findings (unused compat parameters), so they compile lax.
# shellcheck disable=SC2086
"${CXX:-clang++}" -std=c++17 -Wall -Wextra -Werror -fsanitize=undefined -D_UNIX -include Utility/CppMacros.h $includes \
 -c scripts/qa/wlib-thread-test.cpp -o "$test_dir/thread-test.o"
# shellcheck disable=SC2086
"${CXX:-clang++}" -std=c++17 -Wall -fsanitize=undefined -D_UNIX -include Utility/CppMacros.h $includes \
 -c Core/Libraries/Source/WWVegas/WWLib/thread.cpp -o "$test_dir/thread.o"
# shellcheck disable=SC2086
"${CXX:-clang++}" -std=c++17 -Wall -fsanitize=undefined -D_UNIX -include Utility/CppMacros.h $includes \
 -c GeneralsMD/Code/CompatLib/Source/thread_compat.cpp -o "$test_dir/thread_compat.o"
# shellcheck disable=SC2086
"${CXX:-clang++}" -std=c++17 -Wall -fsanitize=undefined -D_UNIX -include Utility/CppMacros.h $includes \
 -c GeneralsMD/Code/CompatLib/Source/time_compat.cpp -o "$test_dir/time_compat.o"
"${CXX:-clang++}" -fsanitize=undefined "$test_dir/thread-test.o" "$test_dir/thread.o" \
 "$test_dir/thread_compat.o" "$test_dir/time_compat.o" -o "$test_dir/thread-test" -pthread
"$test_dir/thread-test"
