// GeneralsX @feature Codex 13/09/2026 Decoration shares world depth; only
// feedback is board-clipped (c.a=1). Cut faces/frame (c.a=0) may extend below it.
#pragma once
#include "XRBoardBounds.h"
static constexpr const char *kXrDecorationVertex=R"GLSL(#version 300 es
layout(location=0) in vec3 p;
layout(location=1) in vec4 c;
uniform mat4 eye,board;
out vec4 color;out highp vec3 local;
void main(){gl_Position=eye*vec4(p,1);local=(board*vec4(p,1)).xyz;color=c;}
)GLSL";
static constexpr const char *kXrDecorationFragment=R"GLSL(#version 300 es
precision mediump float;
in vec4 color;in highp vec3 local;uniform highp float aspect;uniform highp mat4 board;
out vec4 frag;
void main(){
 if(color.a>0.5 && (abs(local.x)>0.5 || abs(local.y)>aspect*0.5 || local.z< -0.15 || local.z>
)GLSL" GX_XR_BOARD_CEILING_GLSL("board") R"GLSL())discard;
 frag=vec4(color.rgb,1.0);
}
)GLSL";
