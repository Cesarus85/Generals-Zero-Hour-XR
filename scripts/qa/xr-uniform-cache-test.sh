#!/usr/bin/env bash
# GeneralsX @test Codex 14/09/2026 Production XR uniform caching on real GLES.
# Usage: CXX=<NDK aarch64-linux-android29-clang++> bash this-script OUTPUT_DIR
set -euo pipefail
repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$repo_dir"
test_dir="${1:?Supply a fresh output directory}"
mkdir -p "$test_dir"
src=Core/Libraries/Source/d3d8gles/src/gles_pipeline.cpp
header=Core/Libraries/Source/d3d8gles/src/gles_pipeline.h
sed -n '/^\tstruct ViewProjKey {/,/^\tuint64_t m_xrUniformEpoch=1;/p' "$header" > "$test_dir/uniform-members.inc"
sed -n '/^struct WebGLPipeline::ProgramInfo {/,/^};/p' "$src" > "$test_dir/uniform-program.inc"
{
 sed -n '/^static float dwordToFloat(/,/^}/p' "$src"
 sed -n '/^static void argbToFloats(/,/^}/p' "$src"
 sed -n '/^void WebGLPipeline::applyUniforms(/,/^}/p' "$src"
 sed -n '/^void WebGLPipeline::invalidateCachedGLState(/,/^}/p' "$src"
} > "$test_dir/uniform-production.inc"
"${CXX:?Set CXX to the Android NDK compiler}" -std=c++17 -Wall -Wextra -Werror -Wno-unused-parameter -static-libstdc++ \
 -I"$test_dir" -ICore/Libraries/Source/d3d8gles/include \
 -Ireferences/fbraz3-dxvk/include/native -Ireferences/fbraz3-dxvk/include/native/windows -Ireferences/fbraz3-dxvk/include/native/directx \
 scripts/qa/xr-uniform-cache-test.cpp -lEGL -lGLESv3 -o "$test_dir/uniform-cache-test"
