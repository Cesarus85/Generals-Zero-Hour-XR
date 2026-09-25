#!/usr/bin/env bash
# GeneralsX @feature Codex 16/09/2026 Host test and source guards for opt-in LAN CRC tracing.
set -euo pipefail

repo_root="$(cd "$(dirname "$0")/../.." && pwd)"
test_dir="$(mktemp -d)"
trap 'rm -f "$test_dir/lan-crc-trace-test" "$test_dir/trace.log"; rmdir "$test_dir"' EXIT

"${CXX:-clang++}" -std=c++17 -Wall -Wextra -Werror -fsanitize=undefined \
    -I"$repo_root/Core/Libraries/Include" \
    "$repo_root/scripts/qa/lan-crc-trace-test.cpp" -o "$test_dir/lan-crc-trace-test"
(cd "$test_dir" && ./lan-crc-trace-test 2>trace.log)

test "$(rg -c '^\[GX-LAN-CRC\] begin' "$test_dir/trace.log")" -eq 2
test "$(rg -c '^\[GX-LAN-CRC\] generated' "$test_dir/trace.log")" -eq 8
test "$(rg -c '^\[GX-LAN-CRC\] object-summary' "$test_dir/trace.log")" -eq 8
test "$(rg -c '^\[GX-LAN-CRC\] object ' "$test_dir/trace.log")" -eq 16
test "$(rg -c '^\[GX-LAN-CRC\] object-detail' "$test_dir/trace.log")" -eq 1
test "$(rg -c '^\[GX-LAN-CRC\] object-field' "$test_dir/trace.log")" -eq 1
test "$(rg -c '^\[GX-LAN-CRC\] object-transform-word' "$test_dir/trace.log")" -eq 4
test "$(rg -c '^\[GX-LAN-CRC\] railroad-step' "$test_dir/trace.log")" -eq 1
test "$(rg -c '^\[GX-LAN-CRC\] checkpoint' "$test_dir/trace.log")" -eq 8
test "$(rg -c '^\[GX-LAN-CRC\] failure' "$test_dir/trace.log")" -eq 2
rg -q 'reason=missing_crc detector_reason=missing_crc' "$test_dir/trace.log"
rg -q 'reason=different_crc detector_reason=different_crc' "$test_dir/trace.log"
rg -q 'object-summary frame=0 total=3 captured=2 truncated=1 limit=2048' "$test_dir/trace.log"
rg -q 'object frame=0 order=0 id=00000010 crc=00000100' "$test_dir/trace.log"
rg -q 'object-detail frame=0 order=14 id=000000D3 template=TestObject start_crc=00000001' "$test_dir/trace.log"
rg -q 'object-field frame=0 order=14 id=000000D3 field=private_status crc=00000002' "$test_dir/trace.log"
rg -q 'object-transform-word frame=0 order=14 id=000000D3 index=3 bits=41200000' "$test_dir/trace.log"
rg -q 'railroad-step frame=100 id=00000010 pull_track=3F800000 .*relative=41900000' "$test_dir/trace.log"

# Source-level guards only: they catch accidental edits to cadence, message,
# and traversal sites; a full engine/headset run is needed for runtime proof.
logic="$repo_root/GeneralsMD/Code/GameEngine/Source/GameLogic/System/GameLogic.cpp"
rg -Fq 'm_frame % TheGameInfo->getCRCInterval()' "$logic"
rg -Fq 'm_CRC = getCRC( CRC_RECALC );' "$logic"
rg -Fq 'msg->appendIntegerArgument(m_CRC);' "$logic"
rg -Fq 'xferCRC->xferSnapshot( obj );' "$logic"
rg -Fq 'GXLanCRCTrace::observeObject(traceObjects' "$logic"
rg -Fq 'GXLanCRCTrace::railroadStep(' "$repo_root/GeneralsMD/Code/GameEngine/Source/GameLogic/Object/Update/AIUpdate/RailroadGuideAIUpdate.cpp"
rg -Fq 'Real desiredAngle = WWMath::Atan2(dy, dx);' "$repo_root/GeneralsMD/Code/GameEngine/Source/GameLogic/Object/Update/AIUpdate/RailroadGuideAIUpdate.cpp"
matrix="$repo_root/Core/Libraries/Source/WWVegas/WWMath/matrix3d.h"
sed -n '/Matrix3D::In_Place_Pre_Rotate_Z(float theta)/,/^}/p' "$matrix" | rg -Fq 'c = WWMath::Cos(theta);'
sed -n '/Matrix3D::In_Place_Pre_Rotate_Z(float theta)/,/^}/p' "$matrix" | rg -Fq 's = WWMath::Sin(theta);'
rg -Fq 'xferCRC->xferSnapshot( ThePartitionManager );' "$logic"
rg -Fq 'xferCRC->xferSnapshot( ThePlayerList );' "$logic"
rg -Fq 'xferCRC->xferSnapshot( TheAI );' "$logic"

echo 'LAN CRC trace: host bounds, reset, reasons, marker/offline and source guards passed'
