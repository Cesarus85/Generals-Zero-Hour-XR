#!/usr/bin/env bash
# GeneralsX @test Codex 14/09/2026 Compile production draw/state code for GLES.
# Usage: bash scripts/qa/xr-stereo-state-test.sh OUTPUT_DIR
# Set CXX to NDK aarch64-linux-android29-clang++; run output on an EGL device.
set -euo pipefail
repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$repo_dir"
test_dir="${1:?Supply a fresh output directory}"
mkdir -p "$test_dir"
src=Core/Libraries/Source/d3d8gles/src/gles_pipeline.cpp
{
  sed -n '/^static GLenum d3dCmpToGL(/,/^}/p' "$src"
  sed -n '/^static GLenum d3dBlendToGL(/,/^}/p' "$src"
  sed -n '/^static GLenum d3dStencilOpToGL(/,/^}/p' "$src"
  sed -n '/^static GLenum primModeGL(/,/^}/p' "$src"
  sed -n '/^static unsigned primVertexCount(/,/^}/p' "$src"
  sed -n '/^enum GeneralsXDrawCategory {/,/^static unsigned s_gxDrawsByCategory/p' "$src"
} > "$test_dir/state-helpers.inc"
sed -n '/^\tstruct FixedStateKey {/,/^\t};/p' Core/Libraries/Source/d3d8gles/src/gles_pipeline.h > "$test_dir/state-key.inc"
{
  sed -n '/^void WebGLPipeline::applyFixedState(/,/^}/p' "$src"
  sed -n '/^void WebGLPipeline::drawCommon(/,/^}/p' "$src"
  # Reference differs only in restoring the old full-state application.
  sed -n '/^void WebGLPipeline::drawCommon(/,/^}/p' "$src" | sed \
    -e 's/WebGLPipeline::drawCommon(/WebGLPipeline::drawReference(/' \
    -e 's/m_xrRestoreCalls+=gxXrRestoreDrawState(.*);/m_haveFixedStateKey=false;applyFixedState(dev);/'
} > "$test_dir/state-production.inc"
{
 sed -n '/^static const char \*kQuadVertShader =/,/^$/p' GeneralsMD/Code/Main/XrHello.cpp
 sed -n '/^static const char \*kQuadFragShader =/,/^$/p' GeneralsMD/Code/Main/XrHello.cpp
} > "$test_dir/quad-production.inc"
"${CXX:?Set CXX to the Android NDK compiler}" -std=c++17 -Wall -Wextra -Werror -static-libstdc++ \
 -I"$test_dir" -ICore/Libraries/Source/d3d8gles/include -IGeneralsMD/Code/Main \
 -Ireferences/fbraz3-dxvk/include/native -Ireferences/fbraz3-dxvk/include/native/windows \
 -Ireferences/fbraz3-dxvk/include/native/directx \
 scripts/qa/xr-stereo-state-test.cpp -lEGL -lGLESv3 -o "$test_dir/stereo-state-test"
