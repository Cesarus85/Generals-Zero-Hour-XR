// GeneralsX @bugfix Codex 13/09/2026 Display-referred legacy RGB -> linear XR output.
#pragma once
#include <cmath>
inline float xrDisplayToLinear(float c) {
	return c<=.04045f ? c/12.92f : std::pow((c+.055f)/1.055f,2.4f);
}
// Shared with the hardware regression test, so the tested transfer is the shipped one.
#define XR_DISPLAY_TO_LINEAR_GLSL \
	"vec3 xrDisplayToLinear(vec3 c) {\n" \
	"  c=clamp(c,0.0,1.0);\n" \
	"  return mix(pow((c+0.055)/1.055,vec3(2.4)),c/12.92,lessThanEqual(c,vec3(0.04045)));\n" \
	"}\n"
