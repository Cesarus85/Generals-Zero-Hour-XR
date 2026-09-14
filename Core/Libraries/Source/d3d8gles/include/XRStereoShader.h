// GeneralsX @feature Codex 13/09/2026 Shared production/test P7 shader contract.
#pragma once
#include "XRBoardBounds.h"
#define GX_XR_MULTIVIEW_PREAMBLE "#extension GL_OVR_multiview2 : require\nlayout(num_views=2) in;\n"
#define GX_XR_MULTIVIEW_VERTEX_DECL "uniform highp int uXrActive,uXrViewSpace; uniform mat4 uXrEyes[2],uXrBoard,uXrCamera; out highp vec3 vXrBoard;\n"
#define GX_XR_MULTIVIEW_VERTEX_BODY "if(uXrActive==1 || uXrActive==3) { vec4 xp=uXrViewSpace!=0 ? uXrCamera*wpos:wpos; gl_Position=uXrEyes[gl_ViewID_OVR]*xp; vXrBoard=(uXrBoard*xp).xyz; }\n"
#define GX_XR_STEREO_VERTEX_DECL "uniform highp int uXrActive,uXrViewSpace; uniform mat4 uXrEyeClip,uXrBoard,uXrCamera; out highp vec3 vXrBoard;\n"
#define GX_XR_STEREO_VERTEX_BODY "if(uXrActive==1 || uXrActive==3) { vec4 xp=uXrViewSpace!=0 ? uXrCamera*wpos:wpos; gl_Position=uXrEyeClip*xp; vXrBoard=(uXrBoard*xp).xyz; }\n"
#define GX_XR_STEREO_FRAGMENT_DECL "uniform highp int uXrActive,uXrOpaque; uniform highp float uXrAspect; uniform highp mat4 uXrBoard; in highp vec3 vXrBoard;\n"
#define GX_XR_STEREO_FRAGMENT_BODY "if(uXrActive==1 && (abs(vXrBoard.x)>0.5 || abs(vXrBoard.y)>uXrAspect*0.5 || vXrBoard.z < -0.15 || vXrBoard.z > " GX_XR_BOARD_CEILING_GLSL("uXrBoard") ")) discard;\n"
// GeneralsX @bugfix Codex 13/09/2026 D3D opaque material alpha is not MR coverage.
// Apply AFTER the original alpha test, preserving cutout holes and RGB.
#define GX_XR_STEREO_COVERAGE_BODY "if(uXrActive!=0 && uXrOpaque!=0) cur.a=1.0;\n"
