// GeneralsX @feature Codex 13/09/2026 Testable rigid placement and uniform scaling.
#pragma once
#include "XrMath.h"
#include <algorithm>

inline XrVector3f xrAdd(XrVector3f a, XrVector3f b) { return {a.x+b.x,a.y+b.y,a.z+b.z}; }
inline XrVector3f xrSub(XrVector3f a, XrVector3f b) { return {a.x-b.x,a.y-b.y,a.z-b.z}; }
inline XrVector3f xrScale(XrVector3f a, float s) { return {a.x*s,a.y*s,a.z*s}; }
inline float xrDot(XrVector3f a, XrVector3f b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
inline XrVector3f xrCross(XrVector3f a, XrVector3f b) {
	return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};
}
inline float xrLength(XrVector3f a) { return sqrtf(xrDot(a,a)); }
inline XrQuaternionf xrNormalize(XrQuaternionf q) {
	const float n=sqrtf(q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w);
	if (!std::isfinite(n) || n<1e-6f) return {0,0,0,1};
	return {q.x/n,q.y/n,q.z/n,q.w/n};
}
inline XrQuaternionf xrConjugate(XrQuaternionf q) { return {-q.x,-q.y,-q.z,q.w}; }
inline XrQuaternionf xrMul(XrQuaternionf a, XrQuaternionf b) {
	return xrNormalize({a.w*b.x+a.x*b.w+a.y*b.z-a.z*b.y,
		a.w*b.y-a.x*b.z+a.y*b.w+a.z*b.x,a.w*b.z+a.x*b.y-a.y*b.x+a.z*b.w,
		a.w*b.w-a.x*b.x-a.y*b.y-a.z*b.z});
}
inline XrQuaternionf xrAxisAngle(XrVector3f axis,float angle) {
	const float n=xrLength(axis); if(n<1e-6f) return {0,0,0,1};
	const float s=sinf(angle*0.5f)/n; return {axis.x*s,axis.y*s,axis.z*s,cosf(angle*0.5f)};
}
inline XrVector3f xrRotate(XrQuaternionf q,XrVector3f p) {
	const XrVector3f v={q.x,q.y,q.z}, t=xrScale(xrCross(v,p),2);
	return xrAdd(p,xrAdd(xrScale(t,q.w),xrCross(v,t)));
}
inline XrPosef xrPoseMul(XrPosef a,XrPosef b) {
	return {xrMul(a.orientation,b.orientation),xrAdd(a.position,xrRotate(a.orientation,b.position))};
}
inline XrPosef xrPoseInverse(XrPosef p) {
	const auto q=xrConjugate(p.orientation); return {q,xrRotate(q,xrScale(p.position,-1))};
}
inline XrQuaternionf xrFromTo(XrVector3f a,XrVector3f b) {
	const float la=xrLength(a),lb=xrLength(b);
	if (la<1e-6f || lb<1e-6f) return {0,0,0,1};
	a=xrScale(a,1/la); b=xrScale(b,1/lb); const float d=xrDot(a,b);
	if(d<-.9999f) return xrAxisAngle(xrCross(a,fabsf(a.x)<.8f ? XrVector3f{1,0,0}:XrVector3f{0,1,0}),3.14159265f);
	const auto c=xrCross(a,b); return xrNormalize({c.x,c.y,c.z,1+d});
}

struct XrSurface {
	XrPosef pose={{0,0,0,1},{0,0,0}};
	float width=1.1f;
};
inline void surfaceMatrix(const XrSurface &s,float *m) {
	const auto right=xrRotate(s.pose.orientation,{s.width,0,0});
	const auto up=xrRotate(s.pose.orientation,{0,s.width,0});
	const auto normal=xrRotate(s.pose.orientation,{0,0,1});
	const float values[]={right.x,right.y,right.z,0,up.x,up.y,up.z,0,
		normal.x,normal.y,normal.z,0,s.pose.position.x,s.pose.position.y,s.pose.position.z,1};
	memcpy(m,values,sizeof(values));
}
inline void snapSurface(XrSurface &s,bool horizontal) {
	const auto axis=xrRotate(s.pose.orientation,horizontal ? XrVector3f{0,-1,0}:XrVector3f{0,0,1});
	if (axis.x*axis.x+axis.z*axis.z<1e-6f) return;
	s.pose.orientation=xrAxisAngle({0,1,0},atan2f(axis.x,axis.z));
	if(horizontal) s.pose.orientation=xrMul(s.pose.orientation,xrAxisAngle({1,0,0},-1.57079633f));
}

// GeneralsX @feature Codex 13/09/2026 Hinge the legend up from a flat board's far edge.
// Upright panels retain their orientation. The pose is shared by both eyes and
// depends on the board, not the moving head, so text stays world-locked.
inline XrSurface xrLegendSurface(const XrSurface &parent,float parentAspect,float legendAspect) {
	XrSurface result=parent;
	const float up=xrRotate(parent.pose.orientation,{0,0,1}).y;
	const float tilt=std::clamp((up-.35f)/.5f,0.0f,1.0f)*.959931089f; // 55 degrees
	const float half=legendAspect*.5f;
	const XrVector3f local={0,(parentAspect*.5f+.012f+half*cosf(tilt))*parent.width,
		(.003f+half*sinf(tilt))*parent.width};
	result.pose.position=xrAdd(parent.pose.position,xrRotate(parent.pose.orientation,local));
	result.pose.orientation=xrMul(parent.pose.orientation,xrAxisAngle({1,0,0},tilt));
	return result;
}

// Captures are rebased on every one/two-hand transition: no snap when adding
// or removing a hand. Tracking loss requires release before another capture.
struct XrSurfaceGrab {
	int mode=0;
	bool armed=false;
	XrSurface start;
	XrPosef handOffset={{0,0,0,1},{0,0,0}};
	XrVector3f startMid={},startVector={};
	void cancel() { mode=0; armed=false; }
	bool update(XrSurface &surface,const XrPosef hands[2],const bool held[2],const bool valid[2],float maxWidth=2.5f) {
		if ((held[0]&&!valid[0]) || (held[1]&&!valid[1])) { cancel(); return false; }
		if(!armed) { if(!held[0]&&!held[1]) armed=true; return false; }
		const int next=(held[0]&&valid[0]?1:0)|(held[1]&&valid[1]?2:0);
		const auto mid=xrScale(xrAdd(hands[0].position,hands[1].position),.5f);
		const auto vector=xrSub(hands[1].position,hands[0].position);
		if(next==3 && xrLength(vector)<.08f) { cancel(); return false; }
		if(next!=mode) {
			mode=next; start=surface; startMid=mid; startVector=vector;
			if(mode==1 || mode==2) handOffset=xrPoseMul(xrPoseInverse(hands[mode==1?0:1]),surface.pose);
			return false;
		}
		if(mode==1 || mode==2) { surface.pose=xrPoseMul(hands[mode==1?0:1],handOffset); return true; }
		if(mode==3) {
			const float width=std::clamp(start.width*xrLength(vector)/xrLength(startVector),.45f,maxWidth);
			const float ratio=width/start.width;
			const auto rotation=xrFromTo(startVector,vector);
			surface.width=width;
			surface.pose.orientation=xrMul(rotation,start.pose.orientation);
			surface.pose.position=xrAdd(mid,xrRotate(rotation,xrScale(xrSub(start.pose.position,startMid),ratio)));
			return true;
		}
		return false;
	}
};
