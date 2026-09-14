#!/usr/bin/env bash
# GeneralsX @test Codex 14/09/2026 Run binary on an Android EGL device.
set -euo pipefail
repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$repo_dir"
test_dir="${1:?Supply a fresh output directory}"
mkdir -p "$test_dir"
src=Core/Libraries/Source/d3d8gles/src/gles_pipeline.cpp
{
 sed -n '/^void WebGLPipeline::destroyXRStereo(/,/^}/p' "$src"
 sed -n '/^bool WebGLPipeline::beginXRStereo(/,/^}/p' "$src"
 sed -n '/^void WebGLPipeline::endXRStereo(/,/^}/p' "$src"
} > "$test_dir/multiview-production.inc"
sed -n '/GLuint xrStereoTexture(int eye) const/p; /bool xrStereoMultiview() const/p' Core/Libraries/Source/d3d8gles/src/gles_pipeline.h > "$test_dir/multiview-getters.inc"
"${CXX:?Set CXX to the Android NDK compiler}" -std=c++17 -Wall -Wextra -Werror -static-libstdc++ \
 -I"$test_dir" -ICore/Libraries/Source/d3d8gles/include \
 scripts/qa/xr-multiview-device-test.cpp -lEGL -lGLESv3 -o "$test_dir/multiview-test"
