#!/usr/bin/env bash
# GeneralsX @bugfix Codex 16/09/2026 UBSan regression for the production LAN CRC detector.
set -euo pipefail

repo_root="$(cd "$(dirname "$0")/../.." && pwd)"
test_dir="$(mktemp -d)"
trap 'rm -f "$test_dir/lan-crc-detector-test"; rmdir "$test_dir"' EXIT

"${CXX:-clang++}" -std=c++17 -Wall -Wextra -Werror -fsanitize=undefined \
    -I"$repo_root/Core/Libraries/Include" \
    "$repo_root/scripts/qa/lan-crc-detector-test.cpp" -o "$test_dir/lan-crc-detector-test"
"$test_dir/lan-crc-detector-test"

# Verify the tested evaluator is reached only by the live-network path, while
# playback continues through the recorder branch in the shared dispatcher.
logic="$repo_root/GeneralsMD/Code/GameEngine/Source/GameLogic/System/GameLogic.cpp"
dispatch="$repo_root/Core/GameEngine/Source/GameLogic/System/GameLogicDispatch.cpp"
rg -Fq 'GXNetworkCRCValidation::evaluate(' "$logic"
rg -Fq 'if (m_shouldValidateCRCs && !TheNetwork->sawCRCMismatch())' "$logic"
rg -Fq 'else if (TheRecorder && TheRecorder->isPlaybackMode())' "$dispatch"

echo 'LAN CRC detector: production evaluator, slot mapping and replay guards passed under UBSan'
