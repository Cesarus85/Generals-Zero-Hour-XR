#!/usr/bin/env bash
# GeneralsX @test Codex 14/09/2026 Actual XR frame/UI boundary on real GLES.
# Usage: CXX=<NDK aarch64-linux-android29-clang++> bash this-script OUTPUT_DIR
# Run the resulting world-copy-test on an Android EGL device. No game assets.
set -euo pipefail
repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$repo_dir"
test_dir="${1:?Supply a fresh test output directory}"
mkdir -p "$test_dir"
src=Core/Libraries/Source/d3d8gles/src/gles_pipeline.cpp
{
 sed -n '/^static void argbToFloats(/,/^}/p' "$src"
 sed -n '/^void WebGLPipeline::clear(/,/^}/p' "$src"
 sed -n '/^void WebGLPipeline::beginXRFrame(/,/^}/p' "$src"
 sed -n '/^bool WebGLPipeline::beginXRUI(/,/^}/p' "$src"
 sed -n '/^void WebGLPipeline::finishXRFrame(/,/^}/p' "$src"
} > "$test_dir/world-copy-production.inc"
sed -n '/GLuint xrWorldTexture() const/p; /GLuint offscreenTexture() const/p' Core/Libraries/Source/d3d8gles/src/gles_pipeline.h > "$test_dir/world-copy-getter.inc"
"${CXX:?Set CXX to the Android NDK compiler}" -std=c++17 -Wall -Wextra -Werror -static-libstdc++ \
 -I"$test_dir" -IGeneralsMD/Code/Main -ICore/Libraries/Source/d3d8gles/include \
 -Ireferences/fbraz3-dxvk/include/native -Ireferences/fbraz3-dxvk/include/native/windows \
 -Ireferences/fbraz3-dxvk/include/native/directx scripts/qa/xr-world-copy-test.cpp -lEGL -lGLESv3 -o "$test_dir/world-copy-test"
