// GeneralsX @feature Codex 13/09/2026 Asset-free stereo miniature calibration.
// Coordinates: x/y on the board, +z above it. One unit = board width.
#pragma once
#include "XrPlacement.h"
#include "XrColor.h"
#include <vector>

constexpr float kXrDioramaAspect=.60f;
struct XrDioramaVertex { XrVector3f position,normal,color; };

// Unlike the 2D surface matrix, height MUST scale with both ground axes.
inline void xrDioramaMatrix(const XrSurface &s,float *m) {
	surfaceMatrix(s,m);
	for(int i=8;i<11;++i) m[i]*=s.width;
}

struct XrDioramaMesh {
	std::vector<XrDioramaVertex> vertices;
	void triangle(XrVector3f a,XrVector3f b,XrVector3f c,XrVector3f color) {
		auto n=xrCross(xrSub(b,a),xrSub(c,a));
		const float length=xrLength(n); if(length<1e-8f) return;
		n=xrScale(n,1/length);
		color={xrDisplayToLinear(color.x),xrDisplayToLinear(color.y),xrDisplayToLinear(color.z)};
		for(const auto p:{a,b,c}) vertices.push_back({p,n,color});
	}
	void quad(XrVector3f a,XrVector3f b,XrVector3f c,XrVector3f d,XrVector3f color) {
		triangle(a,b,c,color); triangle(a,c,d,color);
	}
	void box(float x,float y,float z,float w,float d,float h,XrVector3f color) {
		const XrVector3f a={x-w/2,y-d/2,z},b={x+w/2,y-d/2,z},c={x+w/2,y+d/2,z},e={x-w/2,y+d/2,z};
		const XrVector3f A=xrAdd(a,{0,0,h}),B=xrAdd(b,{0,0,h}),C=xrAdd(c,{0,0,h}),E=xrAdd(e,{0,0,h});
		quad(A,B,C,E,color);quad(e,c,b,a,color);
		quad(a,b,B,A,color);quad(b,c,C,B,color);quad(c,e,E,C,color);quad(e,a,A,E,color);
	}
	void cone(float x,float y,float z,float radius,float height,XrVector3f color,int sides=10) {
		for(int i=0;i<sides;++i) {
			const float a=6.283185307f*i/sides,b=6.283185307f*(i+1)/sides;
			const XrVector3f p={x+radius*cosf(a),y+radius*sinf(a),z},q={x+radius*cosf(b),y+radius*sinf(b),z};
			triangle(p,q,{x,y,z+height},color);
			triangle(q,p,{x,y,z},color);
		}
	}
	void tank(float x,float y,XrVector3f paint) {
		box(x,y,.001f,.071f,.046f,.002f,{.25f,.24f,.19f}); // contact plinth
		for(float side:{-.020f,.020f}) box(x,y+side,.003f,.062f,.010f,.010f,{.14f,.16f,.15f});
		box(x,y,.007f,.056f,.030f,.014f,paint);
		box(x-.004f,y,.021f,.026f,.024f,.012f,paint);
		box(x+.027f,y,.026f,.040f,.005f,.005f,{.28f,.33f,.23f});
		box(x-.009f,y,.033f,.012f,.012f,.002f,{.20f,.25f,.17f});
	}
};

inline XrDioramaMesh xrBuildDiorama() {
	XrDioramaMesh m;
	const XrVector3f dark={.12f,.18f,.20f},trim={.27f,.49f,.49f},sand={.64f,.57f,.40f};
	// GeneralsX @bugfix Codex 13/09/2026 Keep the plinth top below the sand;
	// coincident opaque tops caused black z-fighting at oblique headset angles.
	m.box(0,0,-.027f,1.028f,kXrDioramaAspect+.028f,.022f,dark);
	m.box(0,0,-.005f,1,kXrDioramaAspect,.005f,sand);
	for(float y:{-.307f,.307f}) m.box(0,y,0,1.028f,.012f,.008f,trim);
	for(float x:{-.507f,.507f}) m.box(x,0,0,.012f,kXrDioramaAspect,.008f,trim);
	// Road, river and bridge provide planar alignment and occlusion references.
	m.box(-.05f,-.075f,0,.86f,.062f,.001f,{.27f,.28f,.26f});
	for(int i=0;i<19;++i) m.box(-.46f+i*.043f,-.075f,.0012f,.021f,.002f,.0003f,{.76f,.72f,.55f});
	m.box(.28f,0,.001f,.09f,.59f,.001f,{.19f,.40f,.43f});
	m.box(.28f,-.075f,.003f,.14f,.064f,.009f,{.50f,.48f,.38f});
	for(float y:{-.110f,-.040f}) m.box(.28f,y,.012f,.14f,.004f,.010f,dark);
	// Low-poly ridge at the far edge; front remains flat for calibrated props.
	for(int i=0;i<5;++i) m.cone(-.37f+i*.125f,.215f,0,.080f,.045f+(i%3)*.023f,
		{.52f+i*.015f,.47f+i*.010f,.33f},7);
	// Command building with facade recesses, roof lip and beacon.
	m.box(-.23f,.065f,0,.18f,.12f,.004f,{.39f,.37f,.29f});
	m.box(-.23f,.065f,.004f,.15f,.09f,.065f,{.70f,.65f,.48f});
	m.box(-.23f,.065f,.069f,.16f,.10f,.010f,{.34f,.42f,.36f});
	m.box(-.23f,.018f,.005f,.024f,.003f,.034f,dark);
	for(int i=0;i<4;++i) m.box(-.282f+i*.035f,.018f,.046f,.019f,.003f,.012f,{.23f,.47f,.51f});
	m.box(-.26f,.075f,.079f,.034f,.032f,.017f,dark);
	m.box(-.26f,.075f,.096f,.003f,.003f,.029f,{.67f,.70f,.63f});
	m.cone(-.26f,.075f,.125f,.006f,.008f,{.91f,.52f,.18f},8);
	// Hangar with a shallow pitched roof and visible doorway.
	m.box(.04f,.080f,0,.13f,.11f,.050f,{.50f,.56f,.43f});
	m.box(.04f,.024f,.002f,.075f,.002f,.035f,dark);
	m.quad({-.03f,.020f,.050f},{.11f,.020f,.050f},{.11f,.080f,.075f},{-.03f,.080f,.075f},{.30f,.37f,.30f});
	m.quad({-.03f,.080f,.075f},{.11f,.080f,.075f},{.11f,.14f,.050f},{-.03f,.14f,.050f},{.30f,.37f,.30f});
	m.triangle({-.025f,.025f,.050f},{-.025f,.080f,.075f},{-.025f,.135f,.050f},{.50f,.56f,.43f});
	m.triangle({.105f,.135f,.050f},{.105f,.080f,.075f},{.105f,.025f,.050f},{.50f,.56f,.43f});
	for(int i=0;i<3;++i) m.tank(-.30f+i*.11f,-.075f,{.43f,.50f,.31f});
	for(int i=0;i<6;++i) {
		const float x=.39f+(i%2)*.058f,y=-.20f+(i/2)*.17f;
		m.box(x,y,0,.009f,.009f,.030f,{.37f,.29f,.19f});
		m.cone(x,y,.017f,.024f,.055f,{.24f,.40f,.27f});
	}
	// 2/5/10 cm stepped gauge at 1m board width, plus 5cm ground ticks.
	const float heights[]={.02f,.05f,.10f};
	for(int i=0;i<3;++i) m.box(-.39f+i*.065f,-.225f,0,.040f,.040f,heights[i],{.25f,.58f,.63f});
	for(int i=0;i<=18;++i) m.box(-.45f+i*.05f,-.281f,.0002f,.002f,.012f,.0005f,{.34f,.37f,.31f});
	return m;
}

// Shared with the isolated GLES device test; all colors are linear.
static constexpr const char *kXrDioramaVert=R"GLSL(#version 300 es
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;
layout(location=2) in vec3 aColor;
uniform mat4 uMVP;
out vec3 vColor;
void main() {
  float diffuse=max(dot(normalize(aNormal),normalize(vec3(-0.5,-0.7,1.0))),0.0);
  vColor=aColor*(0.48+0.52*diffuse);
  gl_Position=uMVP*vec4(aPos,1.0);
}
)GLSL";
static constexpr const char *kXrDioramaFrag=R"GLSL(#version 300 es
precision mediump float;
in vec3 vColor;
out vec4 oColor;
void main() { oColor=vec4(vColor,1.0); }
)GLSL";
