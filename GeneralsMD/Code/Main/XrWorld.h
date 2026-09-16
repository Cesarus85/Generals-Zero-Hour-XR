// GeneralsX @feature Codex 13/09/2026 P7 render-only world-to-table contract.
#pragma once
#include "XrPlacement.h"
#include "XRBoardBounds.h"

struct XrWorldFrame {
	bool enabled=false;
	XrSurface board;
	XrPosef eyes[2]={};
	XrFovf fov[2]={};
	int width=1024,height=1024;
	float coverage=1.0f; // Map span, independent of physical table width.
	bool healthBars=true,unitRings=true,boardFrame=true;
	bool volumeShadows=false; // P12 reversible XR-only shadow A/B; decals stay enabled.
	bool multiviewStereo=true;
	bool atlasStereo=false; // P13 opt-in: same pixels; game performance gate open.
	bool elideWorldCopy=true; // P14 only after the current stereo visibility gate.
};

inline float xrMapCoverage(float zoom,float tableWidth) {
	return std::clamp(zoom,.5f,3.0f)*std::clamp(tableWidth/1.1f,.5f,3.64f);
}
inline bool xrBoardContainsSphere(const float *mapping,float aspect,XrVector3f center,float radius) {
	const XrVector3f p={mapping[0]*center.x+mapping[4]*center.y+mapping[12],
		mapping[1]*center.x+mapping[5]*center.y+mapping[13],mapping[10]*center.z+mapping[14]};
	const float r=radius*sqrtf(mapping[0]*mapping[0]+mapping[1]*mapping[1]);
	return fabsf(p.x)<=.5f+r && fabsf(p.y)<=aspect*.5f+r && p.z>=-.15f-r && p.z<=gxXrBoardCeiling(mapping)+r;
}

// GeneralsX @feature Codex 14/09/2026 Sharper eyes within the validated
// Ultra+ is explicitly opt-in. Balanced and High preserve their proven extents;
// the larger target is bounded to 2560 and must pass the GPU allocation gate.
inline void xrStereoExtent(unsigned viewWidth,unsigned viewHeight,int &width,int &height,int tier=1) {
	const int target=tier>=2 ? 2304:tier==1 ? 1920:1536;
	const int limit=tier>=2 ? 2560:2048;
	if(!viewWidth || !viewHeight) {width=height=target;return;}
	const double scale=std::min(double(target)/viewWidth,double(limit)/viewHeight);
	width=std::clamp(int(viewWidth*scale),64,limit);
	height=std::clamp(int(viewHeight*scale),64,limit);
}

// Eye-facing ribbon, not a GL line (whose width varies by driver). Endpoints
// remain in shared room space; only its narrow cross-section faces each eye.
inline bool xrRayRibbon(float *m,XrVector3f start,XrVector3f end,XrVector3f eye,float aspect) {
	const auto along=xrSub(end,start);const float length=xrLength(along);
	if(!std::isfinite(length) || length<.001f || aspect<=0) return false;
	const auto center=xrScale(xrAdd(start,end),.5f);
	auto side=xrCross(along,xrSub(eye,center));
	if(xrLength(side)<1e-6f) side=xrCross(along,fabsf(along.y)<length*.9f ? XrVector3f{0,1,0}:XrVector3f{1,0,0});
	side=xrScale(side,.0025f/xrLength(side));
	const auto up=xrScale(along,1/aspect);
	const float values[]={side.x,side.y,side.z,0,up.x,up.y,up.z,0,0,0,1,0,center.x,center.y,center.z,1};
	memcpy(m,values,sizeof(values));return true;
}

// GeneralsX @feature Codex 13/09/2026 Exact inverse of a rotation + uniform
// scale + translation, shared by rendering and bounded spatial picking.
inline XrVector3f xrTransformPoint(const float *m,XrVector3f p) {
	return {m[0]*p.x+m[4]*p.y+m[8]*p.z+m[12],m[1]*p.x+m[5]*p.y+m[9]*p.z+m[13],m[2]*p.x+m[6]*p.y+m[10]*p.z+m[14]};
}
inline XrVector3f xrInversePoint(const float *m,XrVector3f p) {
	p=xrSub(p,{m[12],m[13],m[14]});
	const float n=m[0]*m[0]+m[1]*m[1]+m[2]*m[2];
	return {(m[0]*p.x+m[1]*p.y+m[2]*p.z)/n,(m[4]*p.x+m[5]*p.y+m[6]*p.z)/n,(m[8]*p.x+m[9]*p.y+m[10]*p.z)/n};
}
inline bool xrWorldRay(const XrSurface &board,float aspect,const float *worldToBoard,
	const XrPosef &aim,XrVector3f &start,XrVector3f &end) {
	if(!std::isfinite(board.width) || board.width<.01f || !std::isfinite(aspect) || aspect<=0) return false;
	const auto inverse=xrPoseInverse(board.pose);
	const auto o=xrScale(xrAdd(inverse.position,xrRotate(inverse.orientation,aim.position)),1/board.width);
	const auto d=xrScale(xrRotate(inverse.orientation,xrRotate(aim.orientation,{0,0,-1})),1/board.width);
	const float origins[]={o.x,o.y,o.z},directions[]={d.x,d.y,d.z};
	const float lo[]={-.5f,-aspect*.5f,-.15f},hi[]={.5f,aspect*.5f,gxXrBoardCeiling(worldToBoard)};
	float near=0,far=10;
	for(int axis=0;axis<3;++axis) {
		if(!std::isfinite(origins[axis]) || !std::isfinite(directions[axis])) return false;
		if(fabsf(directions[axis])<1e-7f) {if(origins[axis]<lo[axis] || origins[axis]>hi[axis]) return false;continue;}
		float a=(lo[axis]-origins[axis])/directions[axis],b=(hi[axis]-origins[axis])/directions[axis];
		if(a>b) std::swap(a,b);near=std::max(near,a);far=std::min(far,b);if(near>=far) return false;
	}
	start=xrInversePoint(worldToBoard,xrAdd(o,xrScale(d,near)));
	end=xrInversePoint(worldToBoard,xrAdd(o,xrScale(d,far)));
	return std::isfinite(xrLength(start)) && std::isfinite(xrLength(end));
}
struct XrWorldHit {float x=0,y=0,distance=0;XrVector3f room={};};
// GeneralsX @bugfix Codex 14/09/2026 P18 use user zoom, not terrain ray
// distances or the native camera's terrain-following correction. W3D keeps
// camera-target distance at desired height / sin(ViewDefaultPitchRadians).
inline float xrStableWorldSpan(float height,float fov,float defaultPitch,float coverage) {
	if(!std::isfinite(height+fov+defaultPitch+coverage) || height<=0 || coverage<=0 ||
		fov<=.01f || fov>=3.0f || defaultPitch<=.01f || defaultPitch>=1.57f) return 0;
	return std::clamp(1.6f*height*tanf(fov*.5f)/sinf(defaultPitch)*coverage,200.0f,3000.0f);
}
inline bool xrWorldToBoard(float *m,XrVector3f center,XrVector3f right,float span) {
	const float length=sqrtf(right.x*right.x+right.y*right.y);
	if(!std::isfinite(span) || span<1 || !std::isfinite(length) || length<.001f ||
		!std::isfinite(xrLength(center))) return false;
	const float x=right.x/length/span,y=right.y/length/span;
	memset(m,0,16*sizeof(float));
	m[0]=x;m[4]=y;m[1]=-y;m[5]=x;m[10]=1/span;m[15]=1;
	m[12]=-x*center.x-y*center.y;m[13]=y*center.x-x*center.y;m[14]=-center.z/span;
	return true;
}
inline void xrWorldEyeClip(float *out,const XrWorldFrame &frame,int eye,const float *worldToBoard) {
	float board[16],view[16],proj[16],room[16],vm[16];surfaceMatrix(frame.board,board);
	for(int i=8;i<11;++i) board[i]*=frame.board.width;
	matMultiply(room,board,worldToBoard);matViewFromPose(view,frame.eyes[eye]);
	matPerspectiveFromFov(proj,frame.fov[eye],.05f,100);
	matMultiply(vm,view,room);matMultiply(out,proj,vm);
}
