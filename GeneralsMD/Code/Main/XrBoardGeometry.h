// GeneralsX @tweak Codex 16/09/2026 P20.1 shared thin tabletop geometry.
#pragma once

constexpr float kXrBoardSoilBottom=-.018f;
constexpr float kXrBoardPlinthThickness=.009f;
constexpr float kXrBoardUnderside=kXrBoardSoilBottom-kXrBoardPlinthThickness;
constexpr float kXrBoardLip=.012f;
constexpr float kXrBoardSurfaceClearance=.002f;
constexpr float kXrBoardOutlineClearance=.001f;

static_assert(kXrBoardPlinthThickness>0 && kXrBoardPlinthThickness<=.009f,
	"P20.1 tabletop underbody must remain at most half the P18 thickness");
