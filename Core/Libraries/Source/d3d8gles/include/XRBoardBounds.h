// GeneralsX @bugfix Codex 14/09/2026 P18 shared CPU/GPU vertical envelope.
#pragma once
#include <algorithm>
// Retail heightmaps store bytes, scaled by 10/16 world units. Keep the
// original half-board of model/aircraft headroom ABOVE the highest terrain.
// Derive this from the existing matrix: no new uniform uploads per draw.
#define GX_XR_TERRAIN_MAX_Z 159.375
#define GX_XR_STRING_IMPL(value) #value
#define GX_XR_STRING(value) GX_XR_STRING_IMPL(value)
#define GX_XR_BOARD_CEILING_GLSL(matrix) "(0.5 + max(0.0, " GX_XR_STRING(GX_XR_TERRAIN_MAX_Z) " * " matrix "[2][2] + " matrix "[3][2]))"
inline float gxXrBoardCeiling(const float *mapping) {
    return .5f+std::max(0.0f,float(GX_XR_TERRAIN_MAX_Z)*mapping[10]+mapping[14]);
}
